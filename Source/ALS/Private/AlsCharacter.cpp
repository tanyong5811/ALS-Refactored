#include "AlsCharacter.h"

#include "AlsAnimationInstance.h"
#include "AlsCharacterMovementComponent.h"
#include "TimerManager.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Curves/CurveFloat.h"
#include "GameFramework/GameNetworkManager.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Settings/AlsCharacterSettings.h"
#include "Settings/AlsAnimationInstanceSettings.h"
#include "Utility/AlsConstants.h"
#include "Utility/AlsMacros.h"
#include "Utility/AlsRotation.h"
#include "Utility/AlsUtility.h"
#include "Utility/AlsVector.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AlsCharacter)

namespace AlsCharacter
{
	constexpr auto MinAimingYawAngleLimit{70.0f};
}

AAlsCharacter::AAlsCharacter(const FObjectInitializer& ObjectInitializer) : Super{
	ObjectInitializer.SetDefaultSubobjectClass<UAlsCharacterMovementComponent>(CharacterMovementComponentName)
}
{
	PrimaryActorTick.bCanEverTick = true;

	bUseControllerRotationYaw = false;
	bClientCheckEncroachmentOnNetUpdate = true; // Required for bSimGravityDisabled to be updated.

	GetCapsuleComponent()->InitCapsuleSize(30.0f, 90.0f);

	if (IsValid(GetMesh()))
	{
		GetMesh()->SetRelativeLocation_Direct({0.0f, 0.0f, -92.0f});
		GetMesh()->SetRelativeRotation_Direct({0.0f, -90.0f, 0.0f});

		GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickMontagesWhenNotRendered;
		GetMesh()->bEnableUpdateRateOptimizations = false;

		// Improves performance, but velocities of kinematic physical bodies will not be
		// calculated, so the ragdoll will not inherit the actor's velocity when activated.
		// 关掉这类优化后，kinematic 刚体的速度计算会受影响，
		// 从而在切换到 ragdoll 时“角色自身速度”不再能正确传递给物理骨骼。
		// TODO Wait until the FPhysScene_Chaos::UpdateKinematicsOnDeferredSkelMeshes() function will be fixed in future engine versions.
		// 这里是为了避免引擎版本差异导致的骨骼/物理 kinematic 更新时序问题。
		// GetMesh()->bDeferKinematicBoneUpdate = true;
	}

	AlsCharacterMovement = Cast<UAlsCharacterMovementComponent>(GetCharacterMovement());

	// This will prevent the editor from combining component details with actor details.
	// Component details can still be accessed from the actor's component hierarchy.

#if WITH_EDITOR
	//编辑器模式下，Actor拖入Level时，隐藏 Mesh/CapsuleComponent/CharacterMovement 属性
	StaticClass()->FindPropertyByName(FName{TEXTVIEW("Mesh")})->SetPropertyFlags(CPF_DisableEditOnInstance);
	StaticClass()->FindPropertyByName(FName{TEXTVIEW("CapsuleComponent")})->SetPropertyFlags(CPF_DisableEditOnInstance);
	StaticClass()->FindPropertyByName(FName{TEXTVIEW("CharacterMovement")})->SetPropertyFlags(CPF_DisableEditOnInstance);
#endif
}

#if WITH_EDITOR
bool AAlsCharacter::CanEditChange(const FProperty* Property) const
{
	// 用于限制编辑器对某些控制旋转相关属性的修改，避免破坏 ALS 的旋转逻辑假设。
	return Super::CanEditChange(Property) &&
	       Property->GetFName() != GET_MEMBER_NAME_STRING_VIEW_CHECKED(ThisClass, bUseControllerRotationPitch) &&
	       Property->GetFName() != GET_MEMBER_NAME_STRING_VIEW_CHECKED(ThisClass, bUseControllerRotationYaw) &&
	       Property->GetFName() != GET_MEMBER_NAME_STRING_VIEW_CHECKED(ThisClass, bUseControllerRotationRoll);
}
#endif

void AAlsCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	// 注册需要复制的“期望状态/输入/目标”字段；这里使用 push-based 并跳过拥有者（COND_SkipOwner）。
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	FDoRepLifetimeParams Parameters;
	Parameters.bIsPushBased = true;

	Parameters.Condition = COND_SkipOwner;
	DOREPLIFETIME_WITH_PARAMS_FAST(ThisClass, DesiredStance, Parameters)
	DOREPLIFETIME_WITH_PARAMS_FAST(ThisClass, DesiredGait, Parameters)
	DOREPLIFETIME_WITH_PARAMS_FAST(ThisClass, bDesiredAiming, Parameters)
	DOREPLIFETIME_WITH_PARAMS_FAST(ThisClass, DesiredRotationMode, Parameters)
	DOREPLIFETIME_WITH_PARAMS_FAST(ThisClass, ViewMode, Parameters)
	DOREPLIFETIME_WITH_PARAMS_FAST(ThisClass, OverlayMode, Parameters)

	DOREPLIFETIME_WITH_PARAMS_FAST(ThisClass, ReplicatedViewRotation, Parameters)
	DOREPLIFETIME_WITH_PARAMS_FAST(ThisClass, InputDirection, Parameters)
	DOREPLIFETIME_WITH_PARAMS_FAST(ThisClass, DesiredVelocityYawAngle, Parameters)
	DOREPLIFETIME_WITH_PARAMS_FAST(ThisClass, RagdollTargetLocation, Parameters)
}

void AAlsCharacter::PreRegisterAllComponents()
{
	// Set some default values here so that the animation instance and the
	// camera component can read the most up-to-date values during initialization.
	// 在组件真正开始注册/初始化前，把期望的旋转/站姿/步态先写入，
	// 避免动画实例或相机在“初始帧”读到旧值。

	RotationMode = bDesiredAiming ? AlsRotationModeTags::Aiming : DesiredRotationMode;
	Stance = DesiredStance;
	Gait = DesiredGait;

	Super::PreRegisterAllComponents();
}

void AAlsCharacter::PostRegisterAllComponents()
{
	Super::PostRegisterAllComponents();

	SetReplicatedViewRotation(Super::GetViewRotation().GetNormalized(), false);
	// 用当前视角初始化复制用的 ViewRotation，保证后续 network smoothing 的基准一致。

	ViewState.NetworkSmoothing.InitialRotation = ReplicatedViewRotation;
	ViewState.NetworkSmoothing.TargetRotation = ReplicatedViewRotation;
	ViewState.NetworkSmoothing.CurrentRotation = ReplicatedViewRotation;

	ViewState.Rotation = ReplicatedViewRotation;
	ViewState.PreviousYawAngle = UE_REAL_TO_FLOAT(ReplicatedViewRotation.Yaw);

	const auto YawAngle{UE_REAL_TO_FLOAT(GetActorRotation().Yaw)};

	SetTargetYawAngle(YawAngle);
	// 把初始 actor yaw 同步到目标 yaw（用于后续平滑/约束旋转系统）。

	LocomotionState.InputYawAngle = YawAngle;
	LocomotionState.VelocityYawAngle = YawAngle;
}

void AAlsCharacter::PostInitializeComponents()
{
	// Make sure the mesh and animation blueprint are ticking after the character so they can access the most up-to-date character state.
	// tick 先后顺序直接决定动画实例能否读取到“本帧最新”的角色状态。

	GetMesh()->AddTickPrerequisiteActor(this);

	AlsCharacterMovement->OnPhysicsRotation.AddUObject(this, &ThisClass::CharacterMovement_OnPhysicsRotation);

	// Pass current movement settings to the movement component.
	// movement component 需要在初始化完成后尽快拿到 MovementSettings，避免早期帧行为不一致。

	AlsCharacterMovement->SetMovementSettings(MovementSettings);

	AnimationInstance = Cast<UAlsAnimationInstance>(GetMesh()->GetAnimInstance());

	Super::PostInitializeComponents();
}

void AAlsCharacter::BeginPlay()
{
	ALS_ENSURE(IsValid(Settings));
	ALS_ENSURE(IsValid(MovementSettings));
	std::ignore = ALS_ENSURE(AnimationInstance.IsValid());

	ALS_ENSURE_MESSAGE(!bUseControllerRotationPitch && !bUseControllerRotationYaw && !bUseControllerRotationRoll, // NOLINT(clang-diagnostic-unused-value)
	                   TEXT("These settings are not allowed and must be turned off!"));

	Super::BeginPlay();

	if (GetLocalRole() >= ROLE_AutonomousProxy)
	{
		// 这里对 simulated proxies 的“传送/瞬移”检测使用组件 Transform 更新事件，
		// 避免与基底/复制回包时序冲突。

		GetCapsuleComponent()->TransformUpdated.AddWeakLambda(
			this, [this](USceneComponent*, const EUpdateTransformFlags, const ETeleportType TeleportType)
			{
				if (TeleportType != ETeleportType::None && AnimationInstance.IsValid())
				{
					AnimationInstance->MarkTeleported();
				}
			});
	}

	RefreshMeshProperties();

	ViewState.NetworkSmoothing.bEnabled |= IsValid(Settings) &&
		Settings->View.bEnableNetworkSmoothing && GetLocalRole() == ROLE_SimulatedProxy;
	// 只有在 simulated proxy 且开启配置时才启用网络平滑，减少无效计算。

	// Update states to use the initial desired values.

	ApplyDesiredStance();

	AlsCharacterMovement->SetStance(Stance);

	RefreshGait();

	RefreshRotationMode();

	AlsCharacterMovement->SetRotationMode(RotationMode);

	OnOverlayModeChanged(OverlayMode);
}

void AAlsCharacter::CalcCamera(const float DeltaTime, FMinimalViewInfo& ViewInfo)
{
	// 优先让蓝图/ALS 逻辑处理相机（OnCalculateCamera 返回 true 时不再走 Super::CalcCamera）。
	if (!OnCalculateCamera(DeltaTime, ViewInfo))
	{
		Super::CalcCamera(DeltaTime, ViewInfo);
	}
}

void AAlsCharacter::PostNetReceiveLocationAndRotation()
{
	// 该回调只会发生在 simulated proxies，因此 Rotation/Teleport 校正逻辑直接作用于它们。

	const auto PreviousLocation{GetActorLocation()};

	// Ignore server-replicated rotation on simulated proxies because ALS itself has full control over character rotation.

	GetReplicatedMovement_Mutable().Rotation = GetActorRotation();

	Super::PostNetReceiveLocationAndRotation();

	// Detect teleportation of simulated proxies.
	// 这里用 bSimGravityDisabled 作为“可能的传送信号”，再结合相对/绝对位置差超过阈值来判定。

	auto bTeleported{static_cast<bool>(bSimGravityDisabled)};

	if (!bTeleported && !ReplicatedBasedMovement.HasRelativeLocation())
	{
		const auto NewLocation{FRepMovement::RebaseOntoLocalOrigin(GetReplicatedMovement().Location, this)};

		bTeleported |= IsValid(Settings) &&
			FVector::DistSquared(PreviousLocation, NewLocation) > FMath::Square(Settings->TeleportDistanceThreshold);
	}

	if (bTeleported && AnimationInstance.IsValid())
	{
		AnimationInstance->MarkTeleported();
	}
}

void AAlsCharacter::OnRep_ReplicatedBasedMovement()
{
	// ACharacter::OnRep_ReplicatedBasedMovement() is only called on simulated proxies, so there is no need to check roles here.
	// ReplicatedBasedMovement 的旋转可能在基底相对空间中，需要根据 MovementBase 重建实际旋转。

	const auto PreviousLocation{GetActorLocation()};

	// Ignore server-replicated rotation on simulated proxies because ALS itself has full control over character rotation.

	if (ReplicatedBasedMovement.HasRelativeRotation())
	{
		FVector MovementBaseLocation;
		FQuat MovementBaseRotation;

		MovementBaseUtility::GetMovementBaseTransform(ReplicatedBasedMovement.MovementBase, ReplicatedBasedMovement.BoneName,
		                                              MovementBaseLocation, MovementBaseRotation);

		// 通过“基底旋转的逆 * 角色四元数”，把复制得到的相对旋转转换成 ALS 所需的最终旋转。
		ReplicatedBasedMovement.Rotation = (MovementBaseRotation.Inverse() * GetActorQuat()).Rotator();
	}
	else
	{
		ReplicatedBasedMovement.Rotation = GetActorRotation();
	}

	Super::OnRep_ReplicatedBasedMovement();

	// Detect teleportation of simulated proxies.

	auto bTeleported{static_cast<bool>(bSimGravityDisabled)};

	if (!bTeleported && BasedMovement.HasRelativeLocation())
	{
		const auto NewLocation{
			GetCharacterMovement()->OldBaseLocation + GetCharacterMovement()->OldBaseQuat.RotateVector(BasedMovement.Location)
		};

		bTeleported |= IsValid(Settings) &&
			FVector::DistSquared(PreviousLocation, NewLocation) > FMath::Square(Settings->TeleportDistanceThreshold);
	}

	if (bTeleported && AnimationInstance.IsValid())
	{
		AnimationInstance->MarkTeleported();
	}
}

void AAlsCharacter::Tick(const float DeltaTime)
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("AAlsCharacter::Tick"), STAT_AAlsCharacter_Tick, STATGROUP_Als)
	TRACE_CPUPROFILER_EVENT_SCOPE_STR(__FUNCTION__)
	// 用于 UE 的统计系统，记录本函数 Tick 的 CPU 开销。
	// 用于 Trace/Profiler，在捕获帧里定位 Tick 的采样区间。

	if (!IsValid(Settings) || !AnimationInstance.IsValid())
	{
		Super::Tick(DeltaTime);
		return;
	}

	RefreshMovementBase();
	// 更新 movement base（基底）数据，决定基底相对移动/旋转如何影响角色。
	RefreshMeshProperties();
	// 根据网络/控制/渲染可见性等条件调整 Mesh tick 与旋转同步策略。

	RefreshInput(DeltaTime);
	// 使用 DeltaTime 更新输入相关的内部状态，为 locomotion/view/rotation 提供依据。

	RefreshLocomotionEarly();
	// Locomotion 早期阶段，通常用于计算/准备中间量与目标分支。

	RefreshView(DeltaTime);
	// 刷新视角相关状态（包含网络平滑与视角相对旋转处理）。
	RefreshLocomotion();
	// Locomotion 主阶段，根据当前输入/状态机计算最终移动目标。
	RefreshGait();
	// 刷新步态（Gait），在期望与能力/环境限制之间选取最终步态。
	RefreshRotationMode();
	// 刷新旋转模式（RotationMode），决定角色朝向采用哪种旋转策略。

	RefreshGroundedRotation(DeltaTime);
	// 地面旋转刷新（grounded），对 yaw/约束做对应计算。
	RefreshInAirRotation(DeltaTime);
	// 空中旋转刷新（in-air），对跳跃/下落分支的朝向进行更新。

	StartMantlingInAir();
	// 尝试在空中启动攀爬（mantling），依赖状态允许性与 trace 判定。
	RefreshMantling();
	// 推进/维护攀爬状态机，使攀爬从开始到结束在多帧连续运行。
	RefreshRagdolling(DeltaTime);
	// 推进布娃娃状态机，处理物理切换、约束与恢复条件。
	RefreshRolling(DeltaTime);
	// 推进翻滚状态机，更新翻滚相关的动画/物理持续效果。
	// 转身 / 地面 MoveAmount 胶囊补偿在 UAlsAnimationInstance::NativePostUpdateAnimation 中执行，与当帧 Pose 曲线对齐。

	Super::Tick(DeltaTime);
	// 在完成 ALS 自定义刷新后再调用基类 Tick，确保 UE 默认更新也执行。

	RefreshLocomotionLate();
	// Locomotion 后期收尾阶段，把主阶段的计算结果落到最终可用状态/参数。
}

void AAlsCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	RefreshMeshProperties();

	// Enable view network smoothing on the listen server here because the remote role may not be valid yet during begin play.

	ViewState.NetworkSmoothing.bEnabled |= IsValid(Settings) && Settings->View.bEnableListenServerNetworkSmoothing &&
		IsNetMode(NM_ListenServer) && GetRemoteRole() == ROLE_AutonomousProxy;
}

void AAlsCharacter::Restart()
{
	Super::Restart();

	ApplyDesiredStance();
}

bool AAlsCharacter::OnCalculateCamera_Implementation(float DeltaTime, FMinimalViewInfo& ViewInfo)
{
	return false;
}

bool AAlsCharacter::SetAnimationInstanceSettings(UAlsAnimationInstanceSettings* NewSettings)
{
	if (!IsValid(NewSettings))
	{
		return false;
	}

	if (!AnimationInstance.IsValid())
	{
		AnimationInstance = Cast<UAlsAnimationInstance>(GetMesh()->GetAnimInstance());
	}

	if (!AnimationInstance.IsValid())
	{
		return false;
	}

	AnimationInstance->SetSettings(NewSettings);
	return true;
}

void AAlsCharacter::RefreshMeshProperties() const
{
	const auto bStandalone{IsNetMode(NM_Standalone)};
	const auto bDedicatedServer{IsNetMode(NM_DedicatedServer)};
	const auto bListenServer{IsNetMode(NM_ListenServer)};

	const auto bAuthority{GetLocalRole() >= ROLE_Authority};
	const auto bRemoteAutonomousProxy{GetRemoteRole() == ROLE_AutonomousProxy};
	const auto bLocallyControlled{IsLocallyControlled()};

	// 当该角色由“远端客户端”控制时（远端拥有 autonomous proxy），服务器端仍需要持续 tick 姿态(pose)，
	// 否则动画/骨骼与网络修正会不同步，常见表现就是 rolling 等动作时的抖动或时间错位。

	const auto DefaultTickOption{GetClass()->GetDefaultObject<ThisClass>()->GetMesh()->VisibilityBasedAnimTickOption};

	const auto TargetTickOption{
		!bStandalone && bAuthority && bRemoteAutonomousProxy
			? EVisibilityBasedAnimTickOption::AlwaysTickPose
			: EVisibilityBasedAnimTickOption::OnlyTickMontagesWhenNotRendered
	};

	// 即便我们希望更激进地更新动画，也要不超过 Mesh 默认的 tick 策略（避免不必要的性能开销）。
	GetMesh()->VisibilityBasedAnimTickOption = FMath::Min(TargetTickOption, DefaultTickOption);

	const auto bMeshIsTicking{
		GetMesh()->bRecentlyRendered || GetMesh()->VisibilityBasedAnimTickOption <= EVisibilityBasedAnimTickOption::AlwaysTickPose
	};

	// 使用绝对网格旋转（absolute mesh rotation）：
	// Mesh 的旋转将由代码/动画实例直接“按当前角色朝向”去精确同步，
	// 从而让旋转动画与角色朝向保持一致。

	// 当角色与动画实例的 tick 频率不一致时（例如网格降频/URO），如果不使用绝对旋转，
	// 旋转动画可能会和角色朝向出现轻微偏差；脚锁启用时偏差会被放大成“脚滑”。

	// 为了节省性能，只在真的需要时开启，例如：
	// 1) 启用了 URO 导致动画更新降频；或
	// 2) listen server 上存在远端 autonomous proxy，需要额外确保远端姿态稳定。

	const auto bUROActive{GetMesh()->AnimUpdateRateParams != nullptr && GetMesh()->AnimUpdateRateParams->UpdateRate > 1};
	const auto bAutonomousProxyOnListenServer{bListenServer && bRemoteAutonomousProxy};

	// 当角色站在“会持续转动的基底/物体”上时，不能启用绝对旋转：
	// 基底带来的相对旋转会被处理方式影响，容易造成持续的旋转抖动。
	// 这里的选择是在“旋转基底的相对关系”和“旋转动画同步”之间做折中。

	const auto bStandingOnRotatingObject{MovementBase.bHasRelativeRotation};

	const auto bUseAbsoluteRotation{
		bMeshIsTicking && !bDedicatedServer && !bLocallyControlled && !bStandingOnRotatingObject &&
		(bUROActive || bAutonomousProxyOnListenServer)
	};

	if (GetMesh()->IsUsingAbsoluteRotation() != bUseAbsoluteRotation)
	{
		//SetUsingAbsoluteRotation（旋转是否继承父组件）
		//SetRelativeRotation_Direct（直接设置相对旋转值，跳过内部的旋转更新逻辑和事件广播。）
		// 切换 absolute rotation 策略后必须立刻校正 Mesh 的相对旋转，
		// 否则本次 tick 中 Mesh 使用旧的相对旋转值会导致可见的跳变。
		GetMesh()->SetUsingAbsoluteRotation(bUseAbsoluteRotation);

		if (bUseAbsoluteRotation || !IsValid(GetMesh()->GetAttachParent()))
		{
			GetMesh()->SetRelativeRotation_Direct(
				GetMesh()->GetRelativeRotationCache().QuatToRotator(GetMesh()->GetComponentQuat()));
		}
		else
		{
			GetMesh()->SetRelativeRotation_Direct(
				GetMesh()->GetRelativeRotationCache().QuatToRotator(GetActorQuat().Inverse() * GetMesh()->GetComponentQuat()));
		}
	}

	if (!bMeshIsTicking)
	{
		AnimationInstance->MarkPendingUpdate();
	}
}

void AAlsCharacter::RefreshTurnInPlaceCurveOffset()
{
	// 只在服务器执行：由服务器权威移动胶囊，客户端通过复制结果跟随，避免多端各算各的。
	if (GetLocalRole() < ROLE_Authority || !bTurnInPlaceCurveOffsetActive)
	{
		return;
	}

	auto* const SkelMesh{GetMesh()};
	if (!IsValid(SkelMesh))
	{
		return;
	}

	auto* const AlsAnim{Cast<UAlsAnimationInstance>(SkelMesh->GetAnimInstance())};
	if (!IsValid(AlsAnim))
	{
		return;
	}

	static const FName FrameCurveName{TEXTVIEW("Frame")};
	static const FName BlendWeightCurveName{TEXTVIEW("BlendWeight")};
	static const FName MoveAmountXCurveName{TEXTVIEW("MoveAmount_X")};
	static const FName MoveAmountYCurveName{TEXTVIEW("MoveAmount_Y")};

	const float FrameCurve{AlsAnim->GetCurveValue(FrameCurveName)};
	const float BlendWeight{AlsAnim->GetCurveValue(BlendWeightCurveName)};
	if (BlendWeight <= UE_SMALL_NUMBER || FrameCurve <= UE_SMALL_NUMBER)
	{
		return;
	}

	const float MoveAmountX{AlsAnim->GetCurveValue(MoveAmountXCurveName)};
	const float MoveAmountY{AlsAnim->GetCurveValue(MoveAmountYCurveName)};

	const float DeltaSeconds{
		IsValid(GetWorld()) ? GetWorld()->GetDeltaSeconds() : 0.0f
	};
	if (DeltaSeconds <= UE_SMALL_NUMBER)
	{
		return;
	}

	// 位移缩放：Frame/BlendWeight、DeltaSeconds、与本次 Turn In Place 一致的 TurnInPlaceState.PlayRate。
	const float EffectiveRate{AlsAnim->GetTurnInPlaceEffectivePlayRate()};
	const float MontagePlayRate{EffectiveRate > UE_SMALL_NUMBER ? EffectiveRate : 1.0f};

	const float DisplacementScale{(FrameCurve / BlendWeight) * DeltaSeconds * MontagePlayRate};
	const FVector LocalMoveDelta{
		MoveAmountX * DisplacementScale,
		MoveAmountY * DisplacementScale,
		0.0f
	};

	// MoveAmount 为动画第 0 帧 Root 局部（Modifier Inverse Transform 烘焙）；TurnInPlaceCurveReferenceYaw 在 MulticastStartTurnInPlace 起转时锁定，与 Locomotion 映射一致。
	const FVector WorldDelta{
		FRotator{0.0f, TurnInPlaceCurveReferenceYaw, 0.0f}.RotateVector(LocalMoveDelta)
	};

	// 只补偿 XY：Z 交给 CharacterMovement/地面约束，防止坡地或台阶上出现竖直抖动。
	FVector HorizontalWorldDelta{WorldDelta};
	HorizontalWorldDelta.Z = 0.0f;

	// 防止曲线/分段切换导致的异常大位移（瞬时爆冲）。
	if (HorizontalWorldDelta.SizeSquared() > FMath::Square(80.0f))
	{
		return;
	}

	// 最终把补偿位移施加到胶囊（Sweep=true，避免穿墙）。
	AddActorWorldOffset(HorizontalWorldDelta, true);
}

void AAlsCharacter::RefreshLocomotionCurveOffset()
{
	// 仅服务器权威推胶囊；转身进行中不叠移动补偿，避免双算。
	if (GetLocalRole() < ROLE_Authority || bTurnInPlaceCurveOffsetActive)
	{
		return;
	}

	if (LocomotionMode != AlsLocomotionModeTags::Grounded || !LocomotionState.bMoving)
	{
		return;
	}

	auto* const SkelMesh{GetMesh()};
	if (!IsValid(SkelMesh))
	{
		return;
	}

	auto* const AnimInstance{SkelMesh->GetAnimInstance()};
	if (!IsValid(AnimInstance))
	{
		return;
	}

	const auto* const AlsAnim{Cast<UAlsAnimationInstance>(AnimInstance)};
	if (!IsValid(AlsAnim))
	{
		return;
	}

	static const FName FrameCurveName{TEXTVIEW("Frame")};
	static const FName BlendWeightCurveName{TEXTVIEW("BlendWeight")};
	static const FName MoveAmountXCurveName{TEXTVIEW("MoveAmount_X")};
	static const FName MoveAmountYCurveName{TEXTVIEW("MoveAmount_Y")};

	const float FrameCurve{AnimInstance->GetCurveValue(FrameCurveName)};
	const float BlendWeight{AnimInstance->GetCurveValue(BlendWeightCurveName)};
	if (BlendWeight <= UE_SMALL_NUMBER || FrameCurve <= UE_SMALL_NUMBER)
	{
		return;
	}

	const float MoveAmountX{AnimInstance->GetCurveValue(MoveAmountXCurveName)};
	const float MoveAmountY{AnimInstance->GetCurveValue(MoveAmountYCurveName)};
	if (FMath::IsNearlyZero(MoveAmountX) && FMath::IsNearlyZero(MoveAmountY))
	{
		return;
	}

	const float DeltaSeconds{
		IsValid(GetWorld()) ? GetWorld()->GetDeltaSeconds() : 0.0f
	};
	if (DeltaSeconds <= UE_SMALL_NUMBER)
	{
		return;
	}

	const float PlayRate{FMath::Max(AlsAnim->GetLocomotionCurveOffsetPlayRate(), UE_KINDA_SMALL_NUMBER)};

	const float DisplacementScale{(FrameCurve / BlendWeight) * DeltaSeconds * PlayRate};
	const FVector LocalMoveDelta{
		MoveAmountX * DisplacementScale,
		MoveAmountY * DisplacementScale,
		0.0f
	};

	// 用胶囊 ActorRotation 右乘 Yaw -90，与网格默认相对旋转（0,-90,0）对齐局部前向。
	const FRotator ActorRotation{GetActorRotation()};
	const float TargetReferenceYaw{
		UE_REAL_TO_FLOAT((ActorRotation.Quaternion() * FRotator{0.0f, -90.0f, 0.0f}.Quaternion()).Rotator().Yaw)
	};

	const FVector WorldDelta{
		FRotator{0.0f, TargetReferenceYaw, 0.0f}.RotateVector(LocalMoveDelta)
	};

	FVector HorizontalWorldDelta{WorldDelta};
	HorizontalWorldDelta.Z = 0.0f;

	if (HorizontalWorldDelta.SizeSquared() > FMath::Square(80.0f))
	{
		return;
	}

	AddActorWorldOffset(HorizontalWorldDelta, true);
}

void AAlsCharacter::RefreshMovementBase()
{
	if (BasedMovement.MovementBase != MovementBase.Primitive || BasedMovement.BoneName != MovementBase.BoneName)
	{
		MovementBase.Primitive = BasedMovement.MovementBase;
		MovementBase.BoneName = BasedMovement.BoneName;
		MovementBase.bBaseChanged = true;
	}
	else
	{
		MovementBase.bBaseChanged = false;
	}

	MovementBase.bHasRelativeLocation = BasedMovement.HasRelativeLocation();
	MovementBase.bHasRelativeRotation = MovementBase.bHasRelativeLocation && BasedMovement.bRelativeRotation;

	const auto PreviousRotation{MovementBase.Rotation};

	MovementBaseUtility::GetMovementBaseTransform(BasedMovement.MovementBase, BasedMovement.BoneName,
	                                              MovementBase.Location, MovementBase.Rotation);

	MovementBase.DeltaRotation = MovementBase.bHasRelativeLocation && !MovementBase.bBaseChanged
		                             ? (MovementBase.Rotation * PreviousRotation.Inverse()).Rotator()
		                             : FRotator::ZeroRotator;
}

void AAlsCharacter::SetViewMode(const FGameplayTag& NewViewMode)
{
	SetViewMode(NewViewMode, true);
}

void AAlsCharacter::SetViewMode(const FGameplayTag& NewViewMode, const bool bSendRpc)
{
	if (ViewMode == NewViewMode || GetLocalRole() < ROLE_AutonomousProxy)
	{
		return;
	}

	ViewMode = NewViewMode;

	MARK_PROPERTY_DIRTY_FROM_NAME(ThisClass, ViewMode, this)

	if (bSendRpc)
	{
		if (GetLocalRole() >= ROLE_Authority)
		{
			ClientSetViewMode(ViewMode);
		}
		else
		{
			ServerSetViewMode(ViewMode);
		}
	}
}

void AAlsCharacter::ClientSetViewMode_Implementation(const FGameplayTag& NewViewMode)
{
	SetViewMode(NewViewMode, false);
}

void AAlsCharacter::ServerSetViewMode_Implementation(const FGameplayTag& NewViewMode)
{
	SetViewMode(NewViewMode, false);
}

void AAlsCharacter::OnMovementModeChanged(const EMovementMode PreviousMovementMode, const uint8 PreviousCustomMode)
{
	// Use the character movement mode to set the locomotion mode to the right value. This allows you to have a
	// custom set of movement modes but still use the functionality of the default character movement component.

	switch (GetCharacterMovement()->MovementMode) // NOLINT(clang-diagnostic-switch-enum)
	{
		case MOVE_Walking:
		case MOVE_NavWalking:
			SetLocomotionMode(AlsLocomotionModeTags::Grounded);
			break;

		case MOVE_Falling:
			SetLocomotionMode(AlsLocomotionModeTags::InAir);
			break;

		default:
			SetLocomotionMode(FGameplayTag::EmptyTag);
			break;
	}

	Super::OnMovementModeChanged(PreviousMovementMode, PreviousCustomMode);
}

void AAlsCharacter::SetLocomotionMode(const FGameplayTag& NewLocomotionMode)
{
	if (LocomotionMode != NewLocomotionMode)
	{
		const auto PreviousLocomotionMode{LocomotionMode};

		LocomotionMode = NewLocomotionMode;

		NotifyLocomotionModeChanged(PreviousLocomotionMode);
	}
}

void AAlsCharacter::NotifyLocomotionModeChanged(const FGameplayTag& PreviousLocomotionMode)
{
	ApplyDesiredStance();

	if (LocomotionMode == AlsLocomotionModeTags::Grounded &&
	    PreviousLocomotionMode == AlsLocomotionModeTags::InAir)
	{
		if (Settings->Ragdolling.bStartRagdollingOnLand &&
		    LocomotionState.Velocity.Z <= -Settings->Ragdolling.RagdollingOnLandSpeedThreshold)
		{
			StartRagdolling();
		}
		else if (Settings->Rolling.bStartRollingOnLand &&
		         LocomotionState.Velocity.Z <= -Settings->Rolling.RollingOnLandSpeedThreshold)
		{
			static constexpr auto PlayRate{1.3f};

			StartRolling(PlayRate, LocomotionState.bHasVelocity
				                       ? LocomotionState.VelocityYawAngle
				                       : UE_REAL_TO_FLOAT(FMath::UnwindDegrees(GetActorRotation().Yaw)));
		}
		else
		{
			// Increase friction for a short period of time to prevent sliding on the ground after landing.

			static constexpr auto HasInputBrakingFrictionFactor{0.5f};
			static constexpr auto NoInputBrakingFrictionFactor{3.0f};

			GetCharacterMovement()->BrakingFrictionFactor = LocomotionState.bHasInput
				                                                ? HasInputBrakingFrictionFactor
				                                                : NoInputBrakingFrictionFactor;

			static constexpr auto ResetDelay{0.5f};

			GetWorldTimerManager().SetTimer(BrakingFrictionFactorResetTimer,
			                                FTimerDelegate::CreateWeakLambda(this, [this]
			                                {
				                                GetCharacterMovement()->BrakingFrictionFactor = 0.0f;
			                                }), ResetDelay, false);

			// Block rotation towards the last input direction after landing to prevent
			// legs from twisting into a spiral while the landing animation is playing.

			LocomotionState.bRotationTowardsLastInputDirectionBlocked = true;
		}
	}
	else if (LocomotionMode == AlsLocomotionModeTags::InAir &&
	         LocomotionAction == AlsLocomotionActionTags::Rolling &&
	         Settings->Rolling.bInterruptRollingWhenInAir)
	{
		// If the character is currently rolling, then enable ragdolling.

		StartRagdolling();
	}

	OnLocomotionModeChanged(PreviousLocomotionMode);
}

void AAlsCharacter::SetDesiredAiming(const bool bNewDesiredAiming)
{
	SetDesiredAiming(bNewDesiredAiming, true);
}

void AAlsCharacter::SetDesiredAiming(const bool bNewDesiredAiming, const bool bSendRpc)
{
	if (bDesiredAiming == bNewDesiredAiming || GetLocalRole() < ROLE_AutonomousProxy)
	{
		return;
	}

	bDesiredAiming = bNewDesiredAiming;

	MARK_PROPERTY_DIRTY_FROM_NAME(ThisClass, bDesiredAiming, this)

	OnDesiredAimingChanged(!bDesiredAiming);

	if (bSendRpc)
	{
		if (GetLocalRole() >= ROLE_Authority)
		{
			ClientSetDesiredAiming(bDesiredAiming);
		}
		else
		{
			ServerSetDesiredAiming(bDesiredAiming);
		}
	}
}

void AAlsCharacter::ClientSetDesiredAiming_Implementation(const bool bNewDesiredAiming)
{
	SetDesiredAiming(bNewDesiredAiming, false);
}

void AAlsCharacter::ServerSetDesiredAiming_Implementation(const bool bNewDesiredAiming)
{
	SetDesiredAiming(bNewDesiredAiming, false);
}

void AAlsCharacter::OnReplicated_DesiredAiming(const bool bPreviousDesiredAiming)
{
	OnDesiredAimingChanged(bPreviousDesiredAiming);
}

void AAlsCharacter::OnDesiredAimingChanged_Implementation(const bool bPreviousDesiredAiming) {}

void AAlsCharacter::SetDesiredRotationMode(const FGameplayTag& NewDesiredRotationMode)
{
	SetDesiredRotationMode(NewDesiredRotationMode, true);
}

void AAlsCharacter::SetDesiredRotationMode(const FGameplayTag& NewDesiredRotationMode, const bool bSendRpc)
{
	if (DesiredRotationMode == NewDesiredRotationMode || GetLocalRole() < ROLE_AutonomousProxy)
	{
		return;
	}

	DesiredRotationMode = NewDesiredRotationMode;

	MARK_PROPERTY_DIRTY_FROM_NAME(ThisClass, DesiredRotationMode, this)

	if (bSendRpc)
	{
		if (GetLocalRole() >= ROLE_Authority)
		{
			ClientSetDesiredRotationMode(DesiredRotationMode);
		}
		else
		{
			ServerSetDesiredRotationMode(DesiredRotationMode);
		}
	}
}

void AAlsCharacter::ClientSetDesiredRotationMode_Implementation(const FGameplayTag& NewDesiredRotationMode)
{
	SetDesiredRotationMode(NewDesiredRotationMode, false);
}

void AAlsCharacter::ServerSetDesiredRotationMode_Implementation(const FGameplayTag& NewDesiredRotationMode)
{
	SetDesiredRotationMode(NewDesiredRotationMode, false);
}

void AAlsCharacter::SetRotationMode(const FGameplayTag& NewRotationMode)
{
	AlsCharacterMovement->SetRotationMode(NewRotationMode);

	if (RotationMode != NewRotationMode)
	{
		const auto PreviousRotationMode{RotationMode};

		RotationMode = NewRotationMode;

		NotifyRotationModeChanged(PreviousRotationMode);
	}
}

void AAlsCharacter::NotifyRotationModeChanged(const FGameplayTag& PreviousRotationMode)
{
	// This prevents the actor from rotating in the last input direction after the
	// rotation mode has been changed and the actor is not moving at that moment.

	LocomotionState.bRotationTowardsLastInputDirectionBlocked = true;

	OnRotationModeChanged(PreviousRotationMode);
}

void AAlsCharacter::RefreshRotationMode()
{
	const auto bAiming{bDesiredAiming || DesiredRotationMode == AlsRotationModeTags::Aiming};
	const auto bSprinting{AlsCharacterMovement->GetMaxAllowedGait() == AlsGaitTags::Sprinting};

	if (ViewMode == AlsViewModeTags::FirstPerson)
	{
		if (LocomotionMode == AlsLocomotionModeTags::InAir)
		{
			if (bAiming && Settings->bAllowAimingWhenInAir)
			{
				SetRotationMode(AlsRotationModeTags::Aiming);
			}
			else
			{
				SetRotationMode(AlsRotationModeTags::ViewDirection);
			}

			return;
		}

		// Grounded and other locomotion modes.

		if (bAiming && (!bSprinting || !Settings->bSprintHasPriorityOverAiming))
		{
			SetRotationMode(AlsRotationModeTags::Aiming);
		}
		else
		{
			SetRotationMode(AlsRotationModeTags::ViewDirection);
		}

		return;
	}

	// Third person and other view modes.

	if (LocomotionMode == AlsLocomotionModeTags::InAir)
	{
		if (bAiming && Settings->bAllowAimingWhenInAir)
		{
			SetRotationMode(AlsRotationModeTags::Aiming);
		}
		else if (bAiming)
		{
			SetRotationMode(AlsRotationModeTags::ViewDirection);
		}
		else
		{
			SetRotationMode(DesiredRotationMode);
		}

		return;
	}

	// Grounded and other locomotion modes.

	if (bSprinting)
	{
		if (bAiming && !Settings->bSprintHasPriorityOverAiming)
		{
			SetRotationMode(AlsRotationModeTags::Aiming);
		}
		else if (Settings->bRotateToVelocityWhenSprinting)
		{
			SetRotationMode(AlsRotationModeTags::VelocityDirection);
		}
		else
		{
			SetRotationMode(DesiredRotationMode);
		}
	}
	else // Not sprinting.
	{
		if (bAiming)
		{
			SetRotationMode(AlsRotationModeTags::Aiming);
		}
		else
		{
			SetRotationMode(DesiredRotationMode);
		}
	}
}

void AAlsCharacter::SetDesiredStance(const FGameplayTag& NewDesiredStance)
{
	SetDesiredStance(NewDesiredStance, true);
}

void AAlsCharacter::SetDesiredStance(const FGameplayTag& NewDesiredStance, const bool bSendRpc)
{
	if (DesiredStance == NewDesiredStance || GetLocalRole() < ROLE_AutonomousProxy)
	{
		return;
	}

	DesiredStance = NewDesiredStance;

	MARK_PROPERTY_DIRTY_FROM_NAME(ThisClass, DesiredStance, this)

	if (bSendRpc)
	{
		if (GetLocalRole() >= ROLE_Authority)
		{
			ClientSetDesiredStance(DesiredStance);
		}
		else
		{
			ServerSetDesiredStance(DesiredStance);
		}
	}

	ApplyDesiredStance();
}

void AAlsCharacter::ClientSetDesiredStance_Implementation(const FGameplayTag& NewDesiredStance)
{
	SetDesiredStance(NewDesiredStance, false);
}

void AAlsCharacter::ServerSetDesiredStance_Implementation(const FGameplayTag& NewDesiredStance)
{
	SetDesiredStance(NewDesiredStance, false);
}

void AAlsCharacter::ApplyDesiredStance()
{
	if (!LocomotionAction.IsValid())
	{
		if (LocomotionMode == AlsLocomotionModeTags::Grounded)
		{
			if (DesiredStance == AlsStanceTags::Standing)
			{
				UnCrouch();
			}
			else if (DesiredStance == AlsStanceTags::Crouching)
			{
				Crouch();
			}
		}
		else if (LocomotionMode == AlsLocomotionModeTags::InAir)
		{
			UnCrouch();
		}
	}
	else if (LocomotionAction == AlsLocomotionActionTags::Rolling && Settings->Rolling.bCrouchOnStart)
	{
		Crouch();
	}
}

bool AAlsCharacter::CanCrouch() const
{
	// This allows the ACharacter::Crouch() function to execute properly when bIsCrouched is true.
	// TODO Wait for https://github.com/EpicGames/UnrealEngine/pull/9558 to be merged into the engine.

	return bIsCrouched || Super::CanCrouch();
}

void AAlsCharacter::OnStartCrouch(const float HalfHeightAdjust, const float ScaledHalfHeightAdjust)
{
	auto* PredictionData{GetCharacterMovement()->GetPredictionData_Client_Character()};

	if (PredictionData != nullptr && GetLocalRole() <= ROLE_SimulatedProxy &&
	    ScaledHalfHeightAdjust > 0.0f && IsPlayingNetworkedRootMotionMontage())
	{
		// The code below essentially undoes the changes that will be made later at the end of the
		// UCharacterMovementComponent::Crouch() function because they literally break network smoothing when crouching
		// while the root motion montage is playing, causing the  mesh to take an incorrect location for a while.
		// TODO Wait for https://github.com/EpicGames/UnrealEngine/pull/10373 to be merged into the engine.

		PredictionData->MeshTranslationOffset.Z += ScaledHalfHeightAdjust;
		PredictionData->OriginalMeshTranslationOffset = PredictionData->MeshTranslationOffset;
	}

	Super::OnStartCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);

	SetStance(AlsStanceTags::Crouching);
}

void AAlsCharacter::OnEndCrouch(const float HalfHeightAdjust, const float ScaledHalfHeightAdjust)
{
	auto* PredictionData{GetCharacterMovement()->GetPredictionData_Client_Character()};

	if (PredictionData != nullptr && GetLocalRole() <= ROLE_SimulatedProxy &&
	    ScaledHalfHeightAdjust > 0.0f && IsPlayingNetworkedRootMotionMontage())
	{
		// Same fix as in AAlsCharacter::OnStartCrouch().

		PredictionData->MeshTranslationOffset.Z -= ScaledHalfHeightAdjust;
		PredictionData->OriginalMeshTranslationOffset = PredictionData->MeshTranslationOffset;
	}

	Super::OnEndCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);

	SetStance(AlsStanceTags::Standing);
}

void AAlsCharacter::SetStance(const FGameplayTag& NewStance)
{
	AlsCharacterMovement->SetStance(NewStance);

	if (Stance != NewStance)
	{
		const auto PreviousStance{Stance};

		Stance = NewStance;

		OnStanceChanged(PreviousStance);
	}
}

void AAlsCharacter::OnStanceChanged_Implementation(const FGameplayTag& PreviousStance) {}

void AAlsCharacter::SetDesiredGait(const FGameplayTag& NewDesiredGait)
{
	SetDesiredGait(NewDesiredGait, true);
}

void AAlsCharacter::SetDesiredGait(const FGameplayTag& NewDesiredGait, const bool bSendRpc)
{
	if (DesiredGait == NewDesiredGait || GetLocalRole() < ROLE_AutonomousProxy)
	{
		return;
	}

	DesiredGait = NewDesiredGait;

	MARK_PROPERTY_DIRTY_FROM_NAME(ThisClass, DesiredGait, this)

	if (bSendRpc)
	{
		if (GetLocalRole() >= ROLE_Authority)
		{
			ClientSetDesiredGait(DesiredGait);
		}
		else
		{
			ServerSetDesiredGait(DesiredGait);
		}
	}
}

void AAlsCharacter::ClientSetDesiredGait_Implementation(const FGameplayTag& NewDesiredGait)
{
	SetDesiredGait(NewDesiredGait, false);
}

void AAlsCharacter::ServerSetDesiredGait_Implementation(const FGameplayTag& NewDesiredGait)
{
	SetDesiredGait(NewDesiredGait, false);
}

void AAlsCharacter::SetGait(const FGameplayTag& NewGait)
{
	if (Gait != NewGait)
	{
		const auto PreviousGait{Gait};

		Gait = NewGait;

		OnGaitChanged(PreviousGait);
	}
}

void AAlsCharacter::OnGaitChanged_Implementation(const FGameplayTag& PreviousGait) {}

void AAlsCharacter::RefreshGait()
{
	if (LocomotionMode != AlsLocomotionModeTags::Grounded)
	{
		return;
	}

	const auto MaxAllowedGait{CalculateMaxAllowedGait()};

	// Update the character max walk speed to the configured speeds based on the currently max allowed gait.

	AlsCharacterMovement->SetMaxAllowedGait(MaxAllowedGait);

	const auto ActualGait{CalculateActualGait(MaxAllowedGait)};

	SetGait(ActualGait);
}

FGameplayTag AAlsCharacter::CalculateMaxAllowedGait() const
{
	// Calculate the max allowed gait. This represents the maximum gait the character is currently allowed
	// to be in and can be determined by the desired gait, the rotation mode, the stance, etc. For example,
	// if you wanted to force the character into a walking state while indoors, this could be done here.

	if (DesiredGait != AlsGaitTags::Sprinting)
	{
		return DesiredGait;
	}

	if (CanSprint())
	{
		return AlsGaitTags::Sprinting;
	}

	return AlsGaitTags::Running;
}

FGameplayTag AAlsCharacter::CalculateActualGait(const FGameplayTag& MaxAllowedGait) const
{
	// Calculate the new gait. This is calculated by the actual movement of the character and so it can be
	// different from the desired gait or max allowed gait. For instance, if the max allowed gait becomes
	// walking, the new gait will still be running until the character decelerates to the walking speed.

	const auto& GaitSettings{AlsCharacterMovement->GetGaitSettings()};

	if (LocomotionState.Speed < GaitSettings.GetMaxWalkSpeed() + 10.0f)
	{
		return AlsGaitTags::Walking;
	}

	if (LocomotionState.Speed < GaitSettings.GetMaxRunSpeed() + 10.0f || MaxAllowedGait != AlsGaitTags::Sprinting)
	{
		return AlsGaitTags::Running;
	}

	return AlsGaitTags::Sprinting;
}

bool AAlsCharacter::CanSprint() const
{
	// Determine if the character can sprint based on the rotation mode and input direction.
	// If the character is in view direction rotation mode, only allow sprinting if there is
	// input and if the input direction is aligned with the view direction within 50 degrees.

	if (!LocomotionState.bHasInput || Stance != AlsStanceTags::Standing ||
	    ((bDesiredAiming || DesiredRotationMode == AlsRotationModeTags::Aiming) && !Settings->bSprintHasPriorityOverAiming))
	{
		return false;
	}

	if (ViewMode != AlsViewModeTags::FirstPerson &&
	    (DesiredRotationMode == AlsRotationModeTags::VelocityDirection || Settings->bRotateToVelocityWhenSprinting))
	{
		return true;
	}

	static constexpr auto ViewRelativeAngleThreshold{50.0f};

	if (FMath::Abs(FMath::UnwindDegrees(UE_REAL_TO_FLOAT(
		    LocomotionState.InputYawAngle - ViewState.Rotation.Yaw))) < ViewRelativeAngleThreshold)
	{
		return true;
	}

	return false;
}

void AAlsCharacter::SetOverlayMode(const FGameplayTag& NewOverlayMode)
{
	SetOverlayMode(NewOverlayMode, true);
}

void AAlsCharacter::SetOverlayMode(const FGameplayTag& NewOverlayMode, const bool bSendRpc)
{
	if (OverlayMode == NewOverlayMode || GetLocalRole() <= ROLE_SimulatedProxy)
	{
		return;
	}

	const auto PreviousOverlayMode{OverlayMode};

	OverlayMode = NewOverlayMode;

	MARK_PROPERTY_DIRTY_FROM_NAME(ThisClass, OverlayMode, this)

	OnOverlayModeChanged(PreviousOverlayMode);

	if (bSendRpc)
	{
		if (GetLocalRole() >= ROLE_Authority)
		{
			ClientSetOverlayMode(OverlayMode);
		}
		else
		{
			ServerSetOverlayMode(OverlayMode);
		}
	}
}

void AAlsCharacter::ClientSetOverlayMode_Implementation(const FGameplayTag& NewOverlayMode)
{
	SetOverlayMode(NewOverlayMode, false);
}

void AAlsCharacter::ServerSetOverlayMode_Implementation(const FGameplayTag& NewOverlayMode)
{
	SetOverlayMode(NewOverlayMode, false);
}

void AAlsCharacter::OnReplicated_OverlayMode(const FGameplayTag& PreviousOverlayMode)
{
	OnOverlayModeChanged(PreviousOverlayMode);
}

void AAlsCharacter::OnOverlayModeChanged_Implementation(const FGameplayTag& PreviousOverlayMode) {}

void AAlsCharacter::SetLocomotionAction(const FGameplayTag& NewLocomotionAction)
{
	if (LocomotionAction != NewLocomotionAction)
	{
		const auto PreviousLocomotionAction{LocomotionAction};

		LocomotionAction = NewLocomotionAction;

		NotifyLocomotionActionChanged(PreviousLocomotionAction);
	}
}

void AAlsCharacter::NotifyLocomotionActionChanged(const FGameplayTag& PreviousLocomotionAction)
{
	if (!LocomotionAction.IsValid())
	{
		AlsCharacterMovement->SetInputBlocked(false);
	}

	ApplyDesiredStance();

	OnLocomotionActionChanged(PreviousLocomotionAction);
}

FRotator AAlsCharacter::GetViewRotation() const
{
	return ViewState.Rotation;
}

void AAlsCharacter::SetInputDirection(FVector NewInputDirection)
{
	NewInputDirection = NewInputDirection.GetSafeNormal();

	COMPARE_ASSIGN_AND_MARK_PROPERTY_DIRTY(ThisClass, InputDirection, NewInputDirection, this)
}

void AAlsCharacter::RefreshInput(const float DeltaTime)
{
	if (GetLocalRole() >= ROLE_AutonomousProxy)
	{
		SetInputDirection(GetCharacterMovement()->GetCurrentAcceleration() / GetCharacterMovement()->GetMaxAcceleration());
	}

	LocomotionState.bHasInput = InputDirection.SizeSquared() > UE_KINDA_SMALL_NUMBER;

	if (LocomotionState.bHasInput)
	{
		LocomotionState.InputYawAngle = UE_REAL_TO_FLOAT(UAlsVector::DirectionToAngleXY(InputDirection));
	}
}

void AAlsCharacter::SetReplicatedViewRotation(const FRotator& NewViewRotation, const bool bSendRpc)
{
	if (!ReplicatedViewRotation.Equals(NewViewRotation))
	{
		ReplicatedViewRotation = NewViewRotation;

		MARK_PROPERTY_DIRTY_FROM_NAME(ThisClass, ReplicatedViewRotation, this)

		if (bSendRpc && GetLocalRole() == ROLE_AutonomousProxy)
		{
			ServerSetReplicatedViewRotation(ReplicatedViewRotation);
		}
	}
}

void AAlsCharacter::ServerSetReplicatedViewRotation_Implementation(const FRotator& NewViewRotation)
{
	SetReplicatedViewRotation(NewViewRotation, false);
}

void AAlsCharacter::OnReplicated_ReplicatedViewRotation()
{
	CorrectViewNetworkSmoothing(ReplicatedViewRotation, MovementBase.bHasRelativeRotation);
}

void AAlsCharacter::CorrectViewNetworkSmoothing(const FRotator& NewTargetRotation, const bool bRotationIsBaseRelative)
{
	// Based on UCharacterMovementComponent::SmoothCorrection().

	auto& NetworkSmoothing{ViewState.NetworkSmoothing};

	NetworkSmoothing.TargetRotation = bRotationIsBaseRelative
		                                  ? (MovementBase.Rotation * NewTargetRotation.Quaternion()).Rotator()
		                                  : NewTargetRotation.GetNormalized();

	if (!NetworkSmoothing.bEnabled)
	{
		NetworkSmoothing.InitialRotation = NetworkSmoothing.TargetRotation;
		NetworkSmoothing.CurrentRotation = NetworkSmoothing.TargetRotation;
		return;
	}

	const auto bListenServer{IsNetMode(NM_ListenServer)};

	const auto NewServerTime{
		bListenServer
			? GetCharacterMovement()->GetServerLastTransformUpdateTimeStamp()
			: GetReplicatedServerLastTransformUpdateTimeStamp()
	};

	if (NewServerTime <= 0.0f)
	{
		return;
	}

	NetworkSmoothing.InitialRotation = NetworkSmoothing.CurrentRotation;

	// Using server time lets us know how much time elapsed, regardless of packet lag variance.

	const auto ServerDeltaTime{NewServerTime - NetworkSmoothing.ServerTime};

	NetworkSmoothing.ServerTime = NewServerTime;

	// Don't let the client fall too far behind or run ahead of new server time.

	const auto MaxServerDeltaTime{GetDefault<AGameNetworkManager>()->MaxClientSmoothingDeltaTime};

	const auto MinServerDeltaTime{
		FMath::Min(MaxServerDeltaTime, bListenServer
			                               ? GetCharacterMovement()->ListenServerNetworkSimulatedSmoothLocationTime
			                               : GetCharacterMovement()->NetworkSimulatedSmoothLocationTime)
	};

	// Calculate how far behind we can be after receiving a new server time.

	const auto MinClientDeltaTime{FMath::Clamp(ServerDeltaTime * 1.25f, MinServerDeltaTime, MaxServerDeltaTime)};

	NetworkSmoothing.ClientTime = FMath::Clamp(NetworkSmoothing.ClientTime,
	                                           NetworkSmoothing.ServerTime - MinClientDeltaTime,
	                                           NetworkSmoothing.ServerTime);

	// Compute actual delta between new server time and client simulation.

	NetworkSmoothing.Duration = NetworkSmoothing.ServerTime - NetworkSmoothing.ClientTime;
}

void AAlsCharacter::RefreshView(const float DeltaTime)
{
	if (MovementBase.bHasRelativeRotation)
	{
		// Offset the rotations to keep them relative to the movement base.

		ViewState.Rotation.Pitch += MovementBase.DeltaRotation.Pitch;
		ViewState.Rotation.Yaw += MovementBase.DeltaRotation.Yaw;
		ViewState.Rotation.Normalize();
	}

	ViewState.PreviousYawAngle = UE_REAL_TO_FLOAT(ViewState.Rotation.Yaw);

	if (MovementBase.bHasRelativeRotation)
	{
		if (IsLocallyControlled())
		{
			// We can't depend on the view rotation sent by the character movement component
			// since it's in world space, so in this case we always send it ourselves.

			SetReplicatedViewRotation((MovementBase.Rotation.Inverse() * Super::GetViewRotation().Quaternion()).Rotator(), true);
		}
	}
	else
	{
		if (IsLocallyControlled() || (IsReplicatingMovement() && GetLocalRole() >= ROLE_Authority && IsValid(GetController())))
		{
			// The character movement component already sends the view rotation to the
			// server if movement is replicated, so we don't have to do this ourselves.

			SetReplicatedViewRotation(Super::GetViewRotation().GetNormalized(), !IsReplicatingMovement());
		}
	}

	RefreshViewNetworkSmoothing(DeltaTime);

	ViewState.Rotation = ViewState.NetworkSmoothing.CurrentRotation;

	// Set the yaw speed by comparing the current and previous view yaw angle, divided by
	// delta seconds. This represents the speed the camera is rotating from left to right.

	if (DeltaTime > UE_SMALL_NUMBER)
	{
		ViewState.YawSpeed = FMath::Abs(UE_REAL_TO_FLOAT(ViewState.Rotation.Yaw - ViewState.PreviousYawAngle)) / DeltaTime;
	}
}

void AAlsCharacter::RefreshViewNetworkSmoothing(const float DeltaTime)
{
	// Based on UCharacterMovementComponent::SmoothClientPosition_Interpolate()
	// and UCharacterMovementComponent::SmoothClientPosition_UpdateVisuals().

	auto& NetworkSmoothing{ViewState.NetworkSmoothing};

	if (!NetworkSmoothing.bEnabled ||
	    NetworkSmoothing.ClientTime >= NetworkSmoothing.ServerTime ||
	    NetworkSmoothing.Duration <= UE_SMALL_NUMBER ||
	    (MovementBase.bHasRelativeRotation && IsNetMode(NM_ListenServer)))
	{
		// Can't use network smoothing on the listen server when the character
		// is standing on a rotating object, as it causes constant rotation jitter.

		NetworkSmoothing.InitialRotation = MovementBase.bHasRelativeRotation
			                                   ? (MovementBase.Rotation * ReplicatedViewRotation.Quaternion()).Rotator()
			                                   : ReplicatedViewRotation;

		NetworkSmoothing.TargetRotation = NetworkSmoothing.InitialRotation;
		NetworkSmoothing.CurrentRotation = NetworkSmoothing.InitialRotation;

		return;
	}

	if (MovementBase.bHasRelativeRotation)
	{
		// Offset the rotations to keep them relative to the movement base.

		NetworkSmoothing.InitialRotation.Pitch += MovementBase.DeltaRotation.Pitch;
		NetworkSmoothing.InitialRotation.Yaw += MovementBase.DeltaRotation.Yaw;
		NetworkSmoothing.InitialRotation.Normalize();

		NetworkSmoothing.TargetRotation.Pitch += MovementBase.DeltaRotation.Pitch;
		NetworkSmoothing.TargetRotation.Yaw += MovementBase.DeltaRotation.Yaw;
		NetworkSmoothing.TargetRotation.Normalize();

		NetworkSmoothing.CurrentRotation.Pitch += MovementBase.DeltaRotation.Pitch;
		NetworkSmoothing.CurrentRotation.Yaw += MovementBase.DeltaRotation.Yaw;
		NetworkSmoothing.CurrentRotation.Normalize();
	}

	NetworkSmoothing.ClientTime += DeltaTime;

	const auto InterpolationAmount{
		UAlsMath::Clamp01(1.0f - (NetworkSmoothing.ServerTime - NetworkSmoothing.ClientTime) / NetworkSmoothing.Duration)
	};

	if (!FAnimWeight::IsFullWeight(InterpolationAmount))
	{
		NetworkSmoothing.CurrentRotation = UAlsRotation::LerpRotation(NetworkSmoothing.InitialRotation, NetworkSmoothing.TargetRotation,
		                                                              InterpolationAmount);
	}
	else
	{
		NetworkSmoothing.ClientTime = NetworkSmoothing.ServerTime;
		NetworkSmoothing.CurrentRotation = NetworkSmoothing.TargetRotation;
	}
}

void AAlsCharacter::SetDesiredVelocityYawAngle(const float NewVelocityYawAngle)
{
	COMPARE_ASSIGN_AND_MARK_PROPERTY_DIRTY(ThisClass, DesiredVelocityYawAngle, NewVelocityYawAngle, this)
}

void AAlsCharacter::RefreshLocomotionEarly()
{
	if (!LocomotionState.bMoving &&
	    RotationMode == AlsRotationModeTags::VelocityDirection &&
	    Settings->bInheritMovementBaseRotationInVelocityDirectionRotationMode)
	{
		DesiredVelocityYawAngle = FMath::UnwindDegrees(UE_REAL_TO_FLOAT(
			DesiredVelocityYawAngle + MovementBase.DeltaRotation.Yaw));

		LocomotionState.VelocityYawAngle = FMath::UnwindDegrees(UE_REAL_TO_FLOAT(
			LocomotionState.VelocityYawAngle + MovementBase.DeltaRotation.Yaw));
	}

	if (MovementBase.bHasRelativeRotation)
	{
		// Offset the rotations (the actor's rotation too) to keep them relative to the movement base.

		LocomotionState.TargetYawAngle = FMath::UnwindDegrees(UE_REAL_TO_FLOAT(
			LocomotionState.TargetYawAngle + MovementBase.DeltaRotation.Yaw));

		LocomotionState.ViewRelativeTargetYawAngle = FMath::UnwindDegrees(UE_REAL_TO_FLOAT(
			LocomotionState.ViewRelativeTargetYawAngle + MovementBase.DeltaRotation.Yaw));

		LocomotionState.SmoothTargetYawAngle = FMath::UnwindDegrees(UE_REAL_TO_FLOAT(
			LocomotionState.SmoothTargetYawAngle + MovementBase.DeltaRotation.Yaw));

		auto NewRotation{GetActorRotation()};
		NewRotation.Pitch += MovementBase.DeltaRotation.Pitch;
		NewRotation.Yaw += MovementBase.DeltaRotation.Yaw;
		NewRotation.Normalize();

		SetActorRotation(NewRotation);
	}

	LocomotionState.bAimingLimitAppliedThisFrame = false;
}

void AAlsCharacter::RefreshLocomotion()
{
	const auto bHadVelocity{LocomotionState.bHasVelocity};

	LocomotionState.Velocity = GetVelocity();

	// Determine if the character is moving by getting its speed. The speed equals the length
	// of the horizontal velocity, so it does not take vertical movement into account. If the
	// character is moving, update the last velocity rotation. This value is saved because it might
	// be useful to know the last orientation of a movement even after the character has stopped.

	LocomotionState.Speed = UE_REAL_TO_FLOAT(LocomotionState.Velocity.Size2D());

	static constexpr auto HasSpeedThreshold{1.0f};

	LocomotionState.bHasVelocity = LocomotionState.Speed >= HasSpeedThreshold;

	if (LocomotionState.bHasVelocity)
	{
		LocomotionState.VelocityYawAngle = UE_REAL_TO_FLOAT(UAlsVector::DirectionToAngleXY(LocomotionState.Velocity));
	}

	if (GetLocalRole() >= ROLE_AutonomousProxy)
	{
		auto bSendInitialVelocityYawAngle{LocomotionState.bHasVelocity && !bHadVelocity};
		auto VelocityYawAngleToSend{LocomotionState.VelocityYawAngle};

		if (Settings->bRotateTowardsDesiredVelocityInVelocityDirectionRotationMode)
		{
			FVector DesiredVelocity;
			if (AlsCharacterMovement->TryConsumePrePenetrationAdjustmentVelocity(DesiredVelocity) &&
			    DesiredVelocity.Size2D() >= HasSpeedThreshold)
			{
				bSendInitialVelocityYawAngle = !bHasDesiredVelocity;
				bHasDesiredVelocity = true;

				SetDesiredVelocityYawAngle(UE_REAL_TO_FLOAT(UAlsVector::DirectionToAngleXY(DesiredVelocity)));
			}
			else
			{
				bSendInitialVelocityYawAngle = LocomotionState.bHasVelocity && !bHasDesiredVelocity;
				bHasDesiredVelocity = LocomotionState.bHasVelocity;

				SetDesiredVelocityYawAngle(LocomotionState.VelocityYawAngle);
			}

			VelocityYawAngleToSend = DesiredVelocityYawAngle;
		}

		// Implicitly send the initial velocity yaw angle from the owning client to other clients,
		// since VelocityYawAngle changes are not always detected on the server for very short moves.

		if (bSendInitialVelocityYawAngle &&
		    (GetLocalRole() == ROLE_AutonomousProxy ||
		     GetRemoteRole() == ROLE_SimulatedProxy ||
		     (IsNetMode(NM_ListenServer) && IsLocallyControlled())))
		{
			ServerSetInitialVelocityYawAngle(VelocityYawAngleToSend);
		}
	}

	// Character is moving if has speed and current acceleration, or if the speed is greater than the moving speed threshold.

	LocomotionState.bMoving = (LocomotionState.bHasInput && LocomotionState.bHasVelocity) ||
	                          LocomotionState.Speed > Settings->MovingSpeedThreshold;
}

void AAlsCharacter::RefreshLocomotionLate()
{
	if (!LocomotionMode.IsValid() || LocomotionAction.IsValid())
	{
		RefreshTargetYawAngleUsingActorRotation();
	}

	LocomotionState.bResetAimingLimit = !LocomotionState.bAimingLimitAppliedThisFrame;
}

void AAlsCharacter::ServerSetInitialVelocityYawAngle_Implementation(const float NewVelocityYawAngle)
{
	MulticastSetInitialVelocityYawAngle(NewVelocityYawAngle);
}

void AAlsCharacter::MulticastSetInitialVelocityYawAngle_Implementation(const float NewVelocityYawAngle)
{
	if (GetLocalRole() != ROLE_AutonomousProxy)
	{
		DesiredVelocityYawAngle = NewVelocityYawAngle;
		LocomotionState.VelocityYawAngle = NewVelocityYawAngle;
		LocomotionState.bRotationTowardsLastInputDirectionBlocked = false;
	}
}

void AAlsCharacter::Jump()
{
	if (Stance == AlsStanceTags::Standing && !LocomotionAction.IsValid() &&
	    LocomotionMode == AlsLocomotionModeTags::Grounded)
	{
		Super::Jump();
	}
}

void AAlsCharacter::OnJumped_Implementation()
{
	Super::OnJumped_Implementation();

	if (GetLocalRole() == ROLE_AutonomousProxy)
	{
		OnJumpedNetworked();
	}
	else if (GetLocalRole() >= ROLE_Authority)
	{
		MulticastOnJumpedNetworked();
	}
}

void AAlsCharacter::MulticastOnJumpedNetworked_Implementation()
{
	if (GetLocalRole() != ROLE_AutonomousProxy)
	{
		OnJumpedNetworked();
	}
}

// ReSharper disable once CppMemberFunctionMayBeConst
void AAlsCharacter::OnJumpedNetworked()
{
	if (AnimationInstance.IsValid())
	{
		AnimationInstance->Jump();
	}
}

void AAlsCharacter::FaceRotation(const FRotator Rotation, const float DeltaTime)
{
	// Left empty intentionally. We ignore rotation changes from external sources because ALS itself has full control over actor rotation.
}

void AAlsCharacter::CharacterMovement_OnPhysicsRotation(const float DeltaTime)
{
	RefreshRollingPhysics(DeltaTime);
}

void AAlsCharacter::RefreshGroundedRotation(const float DeltaTime)
{
	if (LocomotionAction.IsValid() || LocomotionMode != AlsLocomotionModeTags::Grounded)
	{
		return;
	}

	if (HasAnyRootMotion())
	{
		RefreshTargetYawAngleUsingActorRotation();
		return;
	}

	if (!LocomotionState.bMoving)
	{
		// Not moving.

		ApplyRotationYawSpeedAnimationCurve(DeltaTime);

		if (RefreshCustomGroundedNotMovingRotation(DeltaTime))
		{
			return;
		}

		if (RotationMode == AlsRotationModeTags::VelocityDirection)
		{
			float TargetYawAngle;

			if (LocomotionState.bRotationTowardsLastInputDirectionBlocked)
			{
				// Rotate to the last target yaw angle, relative to the movement base or not.

				TargetYawAngle = LocomotionState.TargetYawAngle;

				if (MovementBase.bHasRelativeLocation && !MovementBase.bHasRelativeRotation &&
				    Settings->bInheritMovementBaseRotationInVelocityDirectionRotationMode)
				{
					TargetYawAngle = UE_REAL_TO_FLOAT(TargetYawAngle + MovementBase.DeltaRotation.Yaw);
				}
			}
			else
			{
				// Rotate to the last velocity direction. Rotation of the movement
				// base handled in the AAlsCharacter::RefreshLocomotionEarly() function.

				TargetYawAngle = Settings->bRotateTowardsDesiredVelocityInVelocityDirectionRotationMode
					                 ? DesiredVelocityYawAngle
					                 : LocomotionState.VelocityYawAngle;
			}

			static constexpr auto RotationInterpolationHalfLife{0.1f};
			static constexpr auto TargetYawAngleRotationSpeed{800.0f};

			SetRotationExtraSmooth(TargetYawAngle, DeltaTime, RotationInterpolationHalfLife, TargetYawAngleRotationSpeed);
			return;
		}

		if (RotationMode == AlsRotationModeTags::ViewDirection)
		{
			if ((!LocomotionState.bHasInput && LocomotionState.bRotationTowardsLastInputDirectionBlocked) ||
			    !Settings->bAutoRotateOnAnyInputWhileNotMovingInViewDirectionRotationMode)
			{
				RefreshTargetYawAngleUsingActorRotation();
				return;
			}

			// Rotate to the last view direction.

			const auto TargetYawAngle{
				LocomotionState.bHasInput ? UE_REAL_TO_FLOAT(ViewState.Rotation.Yaw) : LocomotionState.TargetYawAngle
			};

			const auto RotationInterpolationHalfLife{CalculateGroundedMovingRotationInterpolationHalfLife()};

			static constexpr auto TargetYawAngleRotationSpeed{500.0f};

			SetRotationExtraSmooth(TargetYawAngle, DeltaTime, RotationInterpolationHalfLife, TargetYawAngleRotationSpeed);
			return;
		}

		if (RotationMode == AlsRotationModeTags::Aiming || ViewMode == AlsViewModeTags::FirstPerson)
		{
			RefreshGroundedAimingRotation(DeltaTime);
			return;
		}

		RefreshTargetYawAngleUsingActorRotation();
		return;
	}

	// Moving.

	if (RefreshCustomGroundedMovingRotation(DeltaTime))
	{
		return;
	}

	if (RotationMode == AlsRotationModeTags::VelocityDirection &&
	    (LocomotionState.bHasInput || !LocomotionState.bRotationTowardsLastInputDirectionBlocked))
	{
		LocomotionState.bRotationTowardsLastInputDirectionBlocked = false;

		const auto TargetYawAngle{
			Settings->bRotateTowardsDesiredVelocityInVelocityDirectionRotationMode
				? DesiredVelocityYawAngle
				: LocomotionState.VelocityYawAngle
		};

		const auto RotationInterpolationHalfLife{CalculateGroundedMovingRotationInterpolationHalfLife()};

		static constexpr auto TargetYawAngleRotationSpeed{800.0f};

		SetRotationExtraSmooth(TargetYawAngle, DeltaTime, RotationInterpolationHalfLife, TargetYawAngleRotationSpeed);
		return;
	}

	if (RotationMode == AlsRotationModeTags::ViewDirection &&
	    (LocomotionState.bHasInput || !LocomotionState.bRotationTowardsLastInputDirectionBlocked))
	{
		LocomotionState.bRotationTowardsLastInputDirectionBlocked = false;

		float TargetYawAngle;

		if (Gait == AlsGaitTags::Sprinting)
		{
			TargetYawAngle = LocomotionState.VelocityYawAngle;
		}
		else
		{
			TargetYawAngle = UE_REAL_TO_FLOAT(
				ViewState.Rotation.Yaw + GetMesh()->GetAnimInstance()->GetCurveValue(UAlsConstants::RotationYawOffsetCurveName()));
		}

		const auto RotationInterpolationHalfLife{CalculateGroundedMovingRotationInterpolationHalfLife()};

		static constexpr auto TargetYawAngleRotationSpeed{500.0f};

		SetRotationExtraSmooth(TargetYawAngle, DeltaTime, RotationInterpolationHalfLife, TargetYawAngleRotationSpeed);
		return;
	}

	if (RotationMode == AlsRotationModeTags::Aiming)
	{
		RefreshGroundedAimingRotation(DeltaTime);
		return;
	}

	RefreshTargetYawAngleUsingActorRotation();
}

bool AAlsCharacter::RefreshCustomGroundedMovingRotation(const float DeltaTime)
{
	return false;
}

bool AAlsCharacter::RefreshCustomGroundedNotMovingRotation(const float DeltaTime)
{
	return false;
}

void AAlsCharacter::RefreshGroundedAimingRotation(const float DeltaTime)
{
	auto NewActorRotation{GetActorRotation()};

	if (!LocomotionState.bHasInput && !LocomotionState.bMoving)
	{
		// Not moving.

		SetTargetYawAngle(UE_REAL_TO_FLOAT(ViewState.Rotation.Yaw));

		if (!ConstrainAimingRotation(NewActorRotation, DeltaTime, true))
		{
			return;
		}
	}
	else
	{
		// Moving.

		static constexpr auto RotationInterpolationHalfLife{0.1f};
		static constexpr auto TargetYawAngleRotationSpeed{1000.0f};

		SetTargetYawAngleSmooth(UE_REAL_TO_FLOAT(ViewState.Rotation.Yaw), DeltaTime, TargetYawAngleRotationSpeed);

		NewActorRotation.Yaw = UAlsRotation::DamperExactAngle(UE_REAL_TO_FLOAT(FMath::UnwindDegrees(NewActorRotation.Yaw)),
		                                                      LocomotionState.SmoothTargetYawAngle,
		                                                      DeltaTime, RotationInterpolationHalfLife);

		if (ConstrainAimingRotation(NewActorRotation, DeltaTime))
		{
			// Cancel the extra smooth rotation, otherwise the actor will rotate too weirdly.
			LocomotionState.SmoothTargetYawAngle = LocomotionState.TargetYawAngle;
		}
	}

	SetActorRotation(NewActorRotation);
}

bool AAlsCharacter::ConstrainAimingRotation(FRotator& ActorRotation, const float DeltaTime, const bool bApplySecondaryConstraint)
{
	// Limit the actor's rotation when aiming to prevent situations where the lower body noticeably
	// fails to keep up with the rotation of the upper body when the camera is rotating very fast.

	LocomotionState.bAimingLimitAppliedThisFrame = true;

	if (LocomotionState.bResetAimingLimit)
	{
		LocomotionState.AimingYawAngleLimit = 180.0f;
	}

	auto ViewRelativeAngle{FMath::UnwindDegrees(UE_REAL_TO_FLOAT(ViewState.Rotation.Yaw - ActorRotation.Yaw))};

	if (FMath::Abs(ViewRelativeAngle) <= AlsCharacter::MinAimingYawAngleLimit + UE_KINDA_SMALL_NUMBER)
	{
		LocomotionState.AimingYawAngleLimit = AlsCharacter::MinAimingYawAngleLimit;
		return false;
	}

	ViewRelativeAngle = UAlsRotation::RemapAngleForCounterClockwiseRotation(ViewRelativeAngle);

	// Secondary constraint. Simply increases the actor's rotation speed. Typically only used when the actor is not moving.

	if (bApplySecondaryConstraint)
	{
		static constexpr auto RotationInterpolationHalfLife{0.1f};

		// Interpolate the angle only to the point where the constraints no longer apply to ensure a smoother completion of the rotation.

		const auto TargetViewRelativeAngle{
			FMath::Clamp(ViewRelativeAngle, -AlsCharacter::MinAimingYawAngleLimit, AlsCharacter::MinAimingYawAngleLimit)
		};

		const auto DeltaAngle{FMath::UnwindDegrees(TargetViewRelativeAngle - ViewRelativeAngle)};

		if (FMath::IsNearlyZero(DeltaAngle, UE_KINDA_SMALL_NUMBER))
		{
			ViewRelativeAngle = TargetViewRelativeAngle;
		}
		else
		{
			const auto InterpolationAmount{UAlsMath::DamperExactAlpha(DeltaTime, RotationInterpolationHalfLife)};
			ViewRelativeAngle = FMath::UnwindDegrees(ViewRelativeAngle + DeltaAngle * InterpolationAmount);
		}
	}

	// Primary constraint. Prevents the actor from rotating beyond a certain angle relative to the camera.

	if (FMath::Abs(ViewRelativeAngle) > LocomotionState.AimingYawAngleLimit + UE_KINDA_SMALL_NUMBER)
	{
		ViewRelativeAngle = FMath::Clamp(ViewRelativeAngle, -LocomotionState.AimingYawAngleLimit, LocomotionState.AimingYawAngleLimit);
	}
	else
	{
		LocomotionState.AimingYawAngleLimit = FMath::Max(FMath::Abs(ViewRelativeAngle), AlsCharacter::MinAimingYawAngleLimit);
	}

	const auto PreviousActorYawAngle{ActorRotation.Yaw};

	ActorRotation.Yaw = FMath::UnwindDegrees(UE_REAL_TO_FLOAT(ViewState.Rotation.Yaw - ViewRelativeAngle));

	// We use UE_KINDA_SMALL_NUMBER here because even if ViewRelativeAngle hasn't
	// changed, converting it back to ActorRotation.Yaw may introduce a rounding
	// error, and FMath::IsNearlyZero() with default arguments will return false.

	return !FMath::IsNearlyZero(FMath::UnwindDegrees(ActorRotation.Yaw - PreviousActorYawAngle), UE_KINDA_SMALL_NUMBER);
}

float AAlsCharacter::CalculateGroundedMovingRotationInterpolationHalfLife() const
{
	// Calculate the rotation speed by using the rotation speed curve in the movement gait settings. Using
	// the curve in conjunction with the gait amount gives you a high level of control over the rotation
	// rates for each speed. Increase the speed if the camera is rotating quickly for more responsive rotation.

	const auto* InterpolationSpeedCurve{AlsCharacterMovement->GetGaitSettings().RotationInterpolationSpeedCurve.Get()};

	static constexpr auto DefaultInterpolationHalfLife{0.2f};

	const auto InterpolationHalfLife{
		ALS_ENSURE(IsValid(InterpolationSpeedCurve))
			? InterpolationSpeedCurve->GetFloatValue(FMath::Max(1.0f, AlsCharacterMovement->GetGaitAmount()))
			: DefaultInterpolationHalfLife
	};

	static constexpr auto MinInterpolationHalfLifeMultiplier{0.333333f};
	static constexpr auto ReferenceViewYawSpeed{300.0f};

	return InterpolationHalfLife * UAlsMath::LerpClamped(1.0f, MinInterpolationHalfLifeMultiplier,
	                                                     ViewState.YawSpeed / ReferenceViewYawSpeed);
}

void AAlsCharacter::ApplyRotationYawSpeedAnimationCurve(const float DeltaTime)
{
	const auto DeltaYawAngle{GetMesh()->GetAnimInstance()->GetCurveValue(UAlsConstants::RotationYawSpeedCurveName()) * DeltaTime};
	if (FMath::Abs(DeltaYawAngle) > UE_SMALL_NUMBER)
	{
		auto NewRotation{GetActorRotation()};
		NewRotation.Yaw += DeltaYawAngle;

		SetActorRotation(NewRotation);

		RefreshTargetYawAngleUsingActorRotation();
	}
}

void AAlsCharacter::RefreshInAirRotation(const float DeltaTime)
{
	if (LocomotionAction.IsValid() || LocomotionMode != AlsLocomotionModeTags::InAir)
	{
		return;
	}

	if (RefreshCustomInAirRotation(DeltaTime))
	{
		return;
	}

	static constexpr auto RotationInterpolationHalfLife{0.2f};

	if (RotationMode == AlsRotationModeTags::VelocityDirection || RotationMode == AlsRotationModeTags::ViewDirection)
	{
		switch (Settings->InAirRotationMode) // NOLINT(clang-diagnostic-switch-enum)
		{
			case EAlsInAirRotationMode::RotateToVelocityOnJump:
				if (LocomotionState.bMoving)
				{
					SetRotationSmooth(LocomotionState.VelocityYawAngle, DeltaTime, RotationInterpolationHalfLife);
				}
				else
				{
					RefreshTargetYawAngleUsingActorRotation();
				}
				break;

			case EAlsInAirRotationMode::KeepRelativeRotation:
				SetRotationSmooth(UE_REAL_TO_FLOAT(ViewState.Rotation.Yaw - LocomotionState.ViewRelativeTargetYawAngle),
				                  DeltaTime, RotationInterpolationHalfLife);
				break;

			default:
				RefreshTargetYawAngleUsingActorRotation();
				break;
		}
	}
	else if (RotationMode == AlsRotationModeTags::Aiming)
	{
		RefreshInAirAimingRotation(DeltaTime);
	}
	else
	{
		RefreshTargetYawAngleUsingActorRotation();
	}
}

bool AAlsCharacter::RefreshCustomInAirRotation(const float DeltaTime)
{
	return false;
}

void AAlsCharacter::RefreshInAirAimingRotation(const float DeltaTime)
{
	static constexpr auto RotationInterpolationHalfLife{0.1f};

	SetTargetYawAngle(UE_REAL_TO_FLOAT(ViewState.Rotation.Yaw));

	auto NewRotation{GetActorRotation()};
	NewRotation.Yaw = UAlsRotation::DamperExactAngle(UE_REAL_TO_FLOAT(FMath::UnwindDegrees(NewRotation.Yaw)),
	                                                 LocomotionState.SmoothTargetYawAngle, DeltaTime, RotationInterpolationHalfLife);

	ConstrainAimingRotation(NewRotation, DeltaTime);

	SetActorRotation(NewRotation);
}

void AAlsCharacter::SetRotationSmooth(const float TargetYawAngle, const float DeltaTime, const float InterpolationHalfLife)
{
	SetTargetYawAngle(TargetYawAngle);

	auto NewRotation{GetActorRotation()};
	NewRotation.Yaw = UAlsRotation::DamperExactAngle(UE_REAL_TO_FLOAT(FMath::UnwindDegrees(NewRotation.Yaw)),
	                                                 LocomotionState.SmoothTargetYawAngle, DeltaTime, InterpolationHalfLife);

	SetActorRotation(NewRotation);
}

void AAlsCharacter::SetRotationExtraSmooth(const float TargetYawAngle, const float DeltaTime,
                                           const float InterpolationHalfLife, const float TargetYawAngleRotationSpeed)
{
	SetTargetYawAngleSmooth(TargetYawAngle, DeltaTime, TargetYawAngleRotationSpeed);

	auto NewRotation{GetActorRotation()};
	NewRotation.Yaw = UAlsRotation::DamperExactAngle(UE_REAL_TO_FLOAT(FMath::UnwindDegrees(NewRotation.Yaw)),
	                                                 LocomotionState.SmoothTargetYawAngle, DeltaTime, InterpolationHalfLife);

	SetActorRotation(NewRotation);
}

void AAlsCharacter::SetRotationInstant(const float TargetYawAngle, const ETeleportType Teleport)
{
	SetTargetYawAngle(TargetYawAngle);

	auto NewRotation{GetActorRotation()};
	NewRotation.Yaw = TargetYawAngle;

	SetActorRotation(NewRotation, Teleport);
}

void AAlsCharacter::RefreshTargetYawAngleUsingActorRotation()
{
	const auto YawAngle{UE_REAL_TO_FLOAT(GetActorRotation().Yaw)};

	SetTargetYawAngle(YawAngle);
}

void AAlsCharacter::SetTargetYawAngle(const float TargetYawAngle)
{
	LocomotionState.TargetYawAngle = FMath::UnwindDegrees(TargetYawAngle);

	LocomotionState.SmoothTargetYawAngle = LocomotionState.TargetYawAngle;

	RefreshViewRelativeTargetYawAngle();
}

void AAlsCharacter::SetTargetYawAngleSmooth(const float TargetYawAngle, const float DeltaTime, const float RotationSpeed)
{
	LocomotionState.TargetYawAngle = FMath::UnwindDegrees(TargetYawAngle);

	LocomotionState.SmoothTargetYawAngle = UAlsRotation::InterpolateAngleConstant(
		LocomotionState.SmoothTargetYawAngle, LocomotionState.TargetYawAngle, DeltaTime, RotationSpeed);

	RefreshViewRelativeTargetYawAngle();
}

void AAlsCharacter::RefreshViewRelativeTargetYawAngle()
{
	LocomotionState.ViewRelativeTargetYawAngle = FMath::UnwindDegrees(UE_REAL_TO_FLOAT(
		ViewState.Rotation.Yaw - LocomotionState.TargetYawAngle));
}
