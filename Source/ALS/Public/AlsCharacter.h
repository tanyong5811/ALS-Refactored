#pragma once

#include "GameFramework/Character.h"
#include "State/AlsLocomotionState.h"
#include "State/AlsMantlingState.h"
#include "State/AlsMovementBaseState.h"
#include "State/AlsRagdollingState.h"
#include "State/AlsRollingState.h"
#include "State/AlsViewState.h"
#include "Utility/AlsGameplayTags.h"
#include "AlsCharacter.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FAlsTurnInPlaceMontageFinishedSignature, bool, bInterrupted);

struct FAlsMantlingParameters;
struct FAlsMantlingTraceSettings;
class UAlsCharacterMovementComponent;
class UAlsCharacterSettings;
class UAlsMovementSettings;
class UAlsAnimationInstance;
class UAlsMantlingSettings;
class UAlsAnimationInstanceSettings;

UCLASS(AutoExpandCategories = ("Settings|Als Character", "Settings|Als Character|Desired State"))
class ALS_API AAlsCharacter : public ACharacter
{
	GENERATED_BODY()

	friend class UAlsAnimationInstance;

protected:
	// 角色使用的 ALS 移动组件（负责姿态/速度/旋转等核心运动逻辑）。
	UPROPERTY(BlueprintReadOnly, Category = "Als Character")
	TObjectPtr<UAlsCharacterMovementComponent> AlsCharacterMovement;

	// 角色主配置（ALS 的角色级设置，如视角/旋转/网络平滑相关）。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings|Als Character")
	TObjectPtr<UAlsCharacterSettings> Settings;

	// 移动相关的运行时设置（用于驱动 movement component 的具体参数）。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings|Als Character")
	TObjectPtr<UAlsMovementSettings> MovementSettings;

	// 期望瞄准状态：用于驱动“瞄准/非瞄准”的旋转与姿态切换（可复制）。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings|Als Character|Desired State",
		ReplicatedUsing = "OnReplicated_DesiredAiming")
	uint8 bDesiredAiming : 1 {false};

	// 期望旋转模式（如 ViewDirection / Aiming 等），由客户端/服务器同步。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings|Als Character|Desired State", Replicated)
	FGameplayTag DesiredRotationMode{AlsRotationModeTags::ViewDirection};

	// 期望站姿（Standing / Crouching 等），通过网络复制给其他实例。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings|Als Character|Desired State", Replicated)
	FGameplayTag DesiredStance{AlsStanceTags::Standing};

	// 期望步态（Walking/Running 等），用于驱动速度与动画状态选择。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings|Als Character|Desired State", Replicated)
	FGameplayTag DesiredGait{AlsGaitTags::Running};

	// 视图模式（例如 FirstPerson/ThirdPerson 等），影响旋转/瞄准策略。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings|Als Character|Desired State", Replicated)
	FGameplayTag ViewMode{AlsViewModeTags::ThirdPerson};

	// 叠加模式（Overlay），例如与手势/武器/动作叠加相关的状态（可复制）。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings|Als Character|Desired State",
		ReplicatedUsing = "OnReplicated_OverlayMode")
	FGameplayTag OverlayMode{AlsOverlayModeTags::Default};

	// 角色网格对应的动画实例（用于在 C++ 中驱动动画逻辑/状态更新）。
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State|Als Character", Transient, Meta = (ShowInnerProperties))
	TWeakObjectPtr<UAlsAnimationInstance> AnimationInstance;

	// 当前移动/动作模式（Grounded/InAir/Rolling 等），用于状态机分支。
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State|Als Character", Transient)
	FGameplayTag LocomotionMode{AlsLocomotionModeTags::Grounded};

	// 当前实际旋转模式（由 DesiredRotationMode 与瞄准/冲刺等条件计算得到）。
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State|Als Character", Transient)
	FGameplayTag RotationMode{AlsRotationModeTags::ViewDirection};

	// 当前实际站姿（Standing/Crouching 等），由期望与环境条件共同决定。
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State|Als Character", Transient)
	FGameplayTag Stance{AlsStanceTags::Standing};

	// 当前实际步态（Walking/Running 等），由期望步态与能力上限决定。
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State|Als Character", Transient)
	FGameplayTag Gait{AlsGaitTags::Walking};

	// 触发/选择的位移动作（如 Rolling/Mantling 等动作时的辅助状态）。
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State|Als Character", Transient)
	FGameplayTag LocomotionAction;

	// 当前移动基底（movement base）相关的状态，影响基底相对位移/旋转。
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State|Als Character", Transient)
	FAlsMovementBaseState MovementBase;

	// Replicated raw view rotation. Depending on the context, this rotation can be in world space, or in movement
	// base space. In most cases, it is better to use FAlsViewState::Rotation to take advantage of network smoothing.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State|Als Character", Transient,
		ReplicatedUsing = "OnReplicated_ReplicatedViewRotation")
	FRotator ReplicatedViewRotation{ForceInit};

	// 视图状态缓存（包含网络平滑/基底相对旋转等处理后的最终值）(动画层head/spine/aiming使用)。
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State|Als Character", Transient)
	FAlsViewState ViewState;

	// 输入方向（量化后的网络复制值），用于驱动朝向与动画计算。
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State|Als Character", Transient, Replicated)
	FVector_NetQuantizeNormal InputDirection{ForceInit};

	// 输入/速度相关的期望偏航角（用于旋转与动画中的 yaw 计算）。
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State|Als Character",
		Transient, Replicated, Meta = (ClampMin = -180, ClampMax = 180, ForceUnits = "deg"))
	float DesiredVelocityYawAngle{0.0f};

	// 是否存在有效的 DesiredVelocityYawAngle（避免使用无效/未初始化的值）。
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State|Als Character", Transient)
	uint8 bHasDesiredVelocity : 1 {false};

	// 位移/运动学参数聚合状态（与 Locomotion 更新阶段配套）。
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State|Als Character", Transient)
	FAlsLocomotionState LocomotionState;

	// 攀爬状态机数据（mantling 的起/中/止等中间状态）。
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State|Als Character", Transient)
	FAlsMantlingState MantlingState;

	// 布娃娃/受击目标落点（网络量化复制用）。
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State|Als Character", Transient, Replicated)
	FVector_NetQuantize RagdollTargetLocation{ForceInit};

	// 布娃娃状态机数据（ragdoll 的时序与约束相关状态）。
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State|Als Character", Transient)
	FAlsRagdollingState RagdollingState;

	// 翻滚状态机数据（rolling 的节奏/物理更新等）。
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State|Als Character", Transient)
	FAlsRollingState RollingState;

	// 用于在制动力/摩擦系数等参数恢复时的定时器句柄。
	FTimerHandle BrakingFrictionFactorResetTimer;

	// 服务器端转身位移补偿开关：手动触发 TurnInPlace 时启用，结束后关闭。
	bool bTurnInPlaceCurveOffsetActive{false};
	// 起转瞬间锁定：将 Modifier 烘焙的「动画第 0 帧 Root 局部」MoveAmount_X/Y 映射到世界水平面（与 RefreshLocomotionCurveOffset 相同：ActorRotation×(0,-90,0) 的 Yaw）。
	float TurnInPlaceCurveReferenceYaw{0.0f};

public:
	// 构造函数：使用 ALS 的自定义 movement component 子类。
	explicit AAlsCharacter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

#if WITH_EDITOR
	// 编辑器属性变更回调：用于限制不允许被编辑的属性（保证旋转/控制设置一致性）。
	virtual bool CanEditChange(const FProperty* Property) const override;
#endif

	// 网络复制注册：声明哪些“期望状态/输入/目标”等会被复制给其他客户端。
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// 预注册阶段：在组件注册前把期望站姿/步态/瞄准等写入初始状态，供初始化读取。
	virtual void PreRegisterAllComponents() override;

	// 注册后阶段：补齐网络视角旋转相关的初始数据（用于后续 network smoothing）。
	virtual void PostRegisterAllComponents() override;

	// 初始化阶段：确保 mesh/动画实例的 tick 时序正确，并把 movement 配置同步进去。
	virtual void PostInitializeComponents() override;

protected:
	// 开始播放：初始化运行时状态（应用期望 stance/gait/rotation mode 等）。
	virtual void BeginPlay() override;

	// 摄像机计算：允许蓝图/ALS 在 CalcCamera 流程中自定义相机参数。
	virtual void CalcCamera(float DeltaTime, FMinimalViewInfo& ViewInfo) override;

public:
	// 网络回包：模拟代理的“位置/旋转”接收后，进行 ALS 所需的校正处理。
	virtual void PostNetReceiveLocationAndRotation() override;

	// 网络回包：模拟代理的 BasedMovement 复制更新后，进行旋转/传送检测与动画标记。
	virtual void OnRep_ReplicatedBasedMovement() override;

	// Tick：每帧刷新 movement base、视图、位移/姿态等多套状态系统。
	virtual void Tick(float DeltaTime) override;

	// Possess：当控制器接管角色时触发（用于设置/刷新与网络相关状态）。
	virtual void PossessedBy(AController* NewController) override;

	// Restart：重新开始/重生时调用（用于恢复角色初始运动与视图状态）。
	virtual void Restart() override;

public:
	// 获取角色配置（Settings）。
	const UAlsCharacterSettings* GetSettings() const;

	// Runtime switch for current mesh anim instance ALS settings asset.
	UFUNCTION(BlueprintCallable, Category = "ALS|Character")
	bool SetAnimationInstanceSettings(UAlsAnimationInstanceSettings* NewSettings);

protected:
	// 计算相机（蓝图可覆写/扩展）：当返回值表示已处理时，C++ 将不会继续走 Super。
	UFUNCTION(BlueprintNativeEvent, Category = "Als Character", Meta = (ReturnDisplayName = "Handled"))
	bool OnCalculateCamera(float DeltaTime, FMinimalViewInfo& ViewInfo);

private:
	// 刷新 mesh/动画实例相关的渲染与动画参数。
	void RefreshMeshProperties() const;

	// 转身 MoveAmount_X/Y（动画第 0 帧 Root 局部）胶囊水平补偿；由 UAlsAnimationInstance::NativePostUpdateAnimation 调用。
	void RefreshTurnInPlaceCurveOffset();

	// 地面移动时按 MoveAmount_X/Y 补偿胶囊 XY；(Frame/BlendWeight)*DeltaSeconds*PlayRate；PlayRate 来自 UAlsAnimationInstance 的 Standing/CrouchingState；参考偏航为 ActorRotation 组合 -90° Yaw。
	void RefreshLocomotionCurveOffset();

	// 刷新 movement base（基底）相关数据，用于自定义旋转与相对运动。
	void RefreshMovementBase();

	// View Mode

public:
	// 视角模式 getter（当前使用的 ViewMode gameplay tag）。
	const FGameplayTag& GetViewMode() const;

	// 设置视角模式（可选触发 RPC 同步给服务器/其他客户端）。
	UFUNCTION(BlueprintCallable, Category = "ALS|Character", Meta = (AutoCreateRefTerm = "NewViewMode"))
	void SetViewMode(const FGameplayTag& NewViewMode);

private:
	// 视角模式的内部实现（通过 bSendRpc 控制是否走网络同步）。
	void SetViewMode(const FGameplayTag& NewViewMode, bool bSendRpc);

	// 在客户端接收/应用视角模式更新。
	UFUNCTION(Client, Reliable)
	void ClientSetViewMode(const FGameplayTag& NewViewMode);

	// 在服务器接收并应用视角模式更新。
	UFUNCTION(Server, Reliable)
	void ServerSetViewMode(const FGameplayTag& NewViewMode);

	// Locomotion Mode

public:
	// UE 的 MovementMode 变化回调：用于把 UE 的运动状态映射到 ALS 的 LocomotionMode。
	virtual void OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode = 0) override;

public:
	// 获取当前 ALS 的 LocomotionMode。
	const FGameplayTag& GetLocomotionMode() const;

protected:
	// 设置 LocomotionMode（内部实现，通常配合网络复制或环境条件）。
	void SetLocomotionMode(const FGameplayTag& NewLocomotionMode);

	// LocomotionMode 变化通知：可由 C++/蓝图参与处理。
	virtual void NotifyLocomotionModeChanged(const FGameplayTag& PreviousLocomotionMode);

	// 蓝图事件：LocomotionMode 变化时触发。
	UFUNCTION(BlueprintImplementableEvent, Category = "Als Character")
	void OnLocomotionModeChanged(const FGameplayTag& PreviousLocomotionMode);

	// Desired Aiming

public:
	// 是否期望瞄准（Desired Aiming）的 getter。
	bool IsDesiredAiming() const;

	// 设置期望瞄准状态（会根据角色权限选择走本地或 RPC 同步）。
	UFUNCTION(BlueprintCallable, Category = "ALS|Character")
	void SetDesiredAiming(bool bNewDesiredAiming);

private:
	// 期望瞄准的内部实现（bSendRpc 控制是否同步到网络）。
	void SetDesiredAiming(bool bNewDesiredAiming, bool bSendRpc);

	// 客户端应用 DesiredAiming 更新。
	UFUNCTION(Client, Reliable)
	void ClientSetDesiredAiming(bool bNewDesiredAiming);

	// 服务器应用 DesiredAiming 更新。
	UFUNCTION(Server, Reliable)
	void ServerSetDesiredAiming(bool bNewDesiredAiming);

	// 复制通知：当 DesiredAiming 被复制更新时触发（用于更新本地状态机）。
	UFUNCTION()
	void OnReplicated_DesiredAiming(bool bPreviousDesiredAiming);

protected:
	// DesiredAiming 变化的蓝图原生事件（用于播放动画/触发额外逻辑）。
	UFUNCTION(BlueprintNativeEvent, Category = "Als Character")
	void OnDesiredAimingChanged(bool bPreviousDesiredAiming);

	// Desired Rotation Mode

public:
	// 获取期望旋转模式（Desired Rotation Mode）。
	const FGameplayTag& GetDesiredRotationMode() const;

	// 设置期望旋转模式（通常由视角模式/瞄准/冲刺等驱动）。
	UFUNCTION(BlueprintCallable, Category = "ALS|Character", Meta = (AutoCreateRefTerm = "NewDesiredRotationMode"))
	void SetDesiredRotationMode(const FGameplayTag& NewDesiredRotationMode);

private:
	// 期望旋转模式内部实现（bSendRpc 控制网络同步）。
	void SetDesiredRotationMode(const FGameplayTag& NewDesiredRotationMode, bool bSendRpc);

	// 客户端应用期望旋转模式更新。
	UFUNCTION(Client, Reliable)
	void ClientSetDesiredRotationMode(const FGameplayTag& NewDesiredRotationMode);

	// 服务器应用期望旋转模式更新。
	UFUNCTION(Server, Reliable)
	void ServerSetDesiredRotationMode(const FGameplayTag& NewDesiredRotationMode);

	// Rotation Mode

public:
	// 获取当前实际旋转模式（RotationMode）。
	const FGameplayTag& GetRotationMode() const;

protected:
	// 实际旋转模式内部切换实现（可能触发旋转模式变化通知）。
	void SetRotationMode(const FGameplayTag& NewRotationMode);

	// 旋转模式变化通知（用于更新动画/逻辑分支）。
	virtual void NotifyRotationModeChanged(const FGameplayTag& PreviousRotationMode);

	// 蓝图事件：旋转模式变化时触发。
	UFUNCTION(BlueprintImplementableEvent, Category = "Als Character")
	void OnRotationModeChanged(const FGameplayTag& PreviousRotationMode);

	// 根据当前瞄准/冲刺/视角模式等条件刷新 RotationMode。
	void RefreshRotationMode();

	// Desired Stance

public:
	// 获取期望站姿（DesiredStance）。
	const FGameplayTag& GetDesiredStance() const;

	// 设置期望站姿（Standing/Crouching 等），并可同步网络。
	UFUNCTION(BlueprintCallable, Category = "ALS|Character", Meta = (AutoCreateRefTerm = "NewDesiredStance"))
	void SetDesiredStance(const FGameplayTag& NewDesiredStance);

private:
	// 期望站姿内部实现（bSendRpc 控制网络同步）。
	void SetDesiredStance(const FGameplayTag& NewDesiredStance, bool bSendRpc);

	// 客户端应用期望站姿更新。
	UFUNCTION(Client, Reliable)
	void ClientSetDesiredStance(const FGameplayTag& NewDesiredStance);

	// 服务器应用期望站姿更新。
	UFUNCTION(Server, Reliable)
	void ServerSetDesiredStance(const FGameplayTag& NewDesiredStance);

protected:
	// 应用期望站姿：根据 LocomotionMode 与环境条件执行 Crouch/UnCrouch。
	virtual void ApplyDesiredStance();

	// Stance

public:
	// 是否允许下蹲（Crouch 前的权限/条件判断）。
	virtual bool CanCrouch() const override;

	// Crouch 开始回调：用于调整相机/胶囊高度等额外逻辑。
	virtual void OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;

	// Crouch 结束回调：用于恢复相应的高度/状态。
	virtual void OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;

public:
	// 获取当前实际站姿（Stance）。
	const FGameplayTag& GetStance() const;

protected:
	// 实际站姿内部切换实现。
	void SetStance(const FGameplayTag& NewStance);

	// 站姿变化蓝图原生事件（用于播放动画/触发额外状态）。
	UFUNCTION(BlueprintNativeEvent, Category = "Als Character")
	void OnStanceChanged(const FGameplayTag& PreviousStance);

	// Desired Gait

public:
	// 获取期望步态（DesiredGait）。
	const FGameplayTag& GetDesiredGait() const;

	// 设置期望步态（Walking/Running 等），并可同步网络。
	UFUNCTION(BlueprintCallable, Category = "ALS|Character", Meta = (AutoCreateRefTerm = "NewDesiredGait"))
	void SetDesiredGait(const FGameplayTag& NewDesiredGait);

private:
	// 期望步态内部实现（bSendRpc 控制网络同步）。
	void SetDesiredGait(const FGameplayTag& NewDesiredGait, bool bSendRpc);

	// 客户端应用期望步态更新。
	UFUNCTION(Client, Reliable)
	void ClientSetDesiredGait(const FGameplayTag& NewDesiredGait);

	// 服务器应用期望步态更新。
	UFUNCTION(Server, Reliable)
	void ServerSetDesiredGait(const FGameplayTag& NewDesiredGait);

	// Gait

public:
	// 获取当前实际步态（Gait）。
	const FGameplayTag& GetGait() const;

protected:
	// 实际步态内部切换实现（由输入速度/能力上限等计算结果驱动）。
	void SetGait(const FGameplayTag& NewGait);

	// 步态变化蓝图原生事件（用于切换动画/速度曲线等）。
	UFUNCTION(BlueprintNativeEvent, Category = "Als Character")
	void OnGaitChanged(const FGameplayTag& PreviousGait);

private:
	// 根据当前速度/期望值重新计算 Gait，并同步到 Locomotion 系统。
	void RefreshGait();

	// 计算当前情况下允许的最大步态（受移动能力/环境影响）。
	FGameplayTag CalculateMaxAllowedGait() const;

	// 根据最大允许步态与期望步态计算最终实际步态。
	FGameplayTag CalculateActualGait(const FGameplayTag& MaxAllowedGait) const;

	// 判断当前是否允许冲刺（Sprint）状态。
	bool CanSprint() const;

	// Overlay Mode

public:
	// 获取当前叠加模式（OverlayMode）。
	const FGameplayTag& GetOverlayMode() const;

	// 设置叠加模式（用于与动画叠加/动作系统联动，并可通过网络同步）。
	UFUNCTION(BlueprintCallable, Category = "ALS|Character", Meta = (AutoCreateRefTerm = "NewOverlayMode"))
	void SetOverlayMode(const FGameplayTag& NewOverlayMode);

private:
	// 叠加模式内部实现（bSendRpc 控制网络同步）。
	void SetOverlayMode(const FGameplayTag& NewOverlayMode, bool bSendRpc);

	// 客户端应用 OverlayMode 更新。
	UFUNCTION(Client, Reliable)
	void ClientSetOverlayMode(const FGameplayTag& NewOverlayMode);

	// 服务器应用 OverlayMode 更新。
	UFUNCTION(Server, Reliable)
	void ServerSetOverlayMode(const FGameplayTag& NewOverlayMode);

	// 复制通知：OverlayMode 变化时触发。
	UFUNCTION()
	void OnReplicated_OverlayMode(const FGameplayTag& PreviousOverlayMode);

protected:
	// OverlayMode 变化通知（蓝图可扩展）。
	UFUNCTION(BlueprintNativeEvent, Category = "Als Character")
	void OnOverlayModeChanged(const FGameplayTag& PreviousOverlayMode);

	// Locomotion Action

public:
	// 获取当前 Locomotion Action（用于动作/能力触发的辅助标记）。
	const FGameplayTag& GetLocomotionAction() const;

	// 设置 Locomotion Action。
	void SetLocomotionAction(const FGameplayTag& NewLocomotionAction);

protected:
	// Locomotion Action 变化通知（用于驱动滚动/攀爬等动作逻辑）。
	virtual void NotifyLocomotionActionChanged(const FGameplayTag& PreviousLocomotionAction);

	// 蓝图事件：Locomotion Action 变化时触发。
	UFUNCTION(BlueprintImplementableEvent, Category = "Als Character")
	void OnLocomotionActionChanged(const FGameplayTag& PreviousLocomotionAction);

	// Input

public:
	// 获取输入方向（量化网络复制值）。
	const FVector& GetInputDirection() const;

protected:
	// 输入方向内部设置。
	void SetInputDirection(FVector NewInputDirection);

	// 输入刷新：结合 DeltaTime 与当前输入进行平滑/更新。
	virtual void RefreshInput(float DeltaTime);

	// View

public:
	// 获取最终用于渲染/动画的视角旋转（通常包含网络平滑后的修正）。
	virtual FRotator GetViewRotation() const override;

private:
	// 设置复制用的视角旋转（同时控制是否通过 RPC 同步）。
	void SetReplicatedViewRotation(const FRotator& NewViewRotation, bool bSendRpc);

	// 服务器接收视角旋转更新（Unreliable 用于高频输入）。
	UFUNCTION(Server, Unreliable)
	void ServerSetReplicatedViewRotation(const FRotator& NewViewRotation);

	// 复制通知：ReplicatedViewRotation 更新时触发。
	UFUNCTION()
	void OnReplicated_ReplicatedViewRotation();

public:
	// 修正视角网络平滑：当目标旋转属于“基底相对”或“世界相对”时采用不同处理。
	void CorrectViewNetworkSmoothing(const FRotator& NewTargetRotation, bool bRotationIsBaseRelative);

public:
	// 获取 ViewState（包含网络平滑与最终视角数据）。
	const FAlsViewState& GetViewState() const;

private:
	// 刷新视图状态（包括旋转平滑逻辑）。
	void RefreshView(float DeltaTime);

	// 专门刷新视图网络平滑部分。
	void RefreshViewNetworkSmoothing(float DeltaTime);

	// Locomotion

public:
	// 获取位移/动作系统用的 LocomotionState。
	const FAlsLocomotionState& GetLocomotionState() const;

private:
	// 设置期望速度的偏航角（用于旋转/动画计算）。
	void SetDesiredVelocityYawAngle(float NewVelocityYawAngle);

	// Locomotion 更新（早期阶段）：准备需要的中间量。
	void RefreshLocomotionEarly();

	// Locomotion 更新（主阶段）：根据当前状态机/输入计算期望目标。
	void RefreshLocomotion();

	// Locomotion 更新（后期阶段）：应用与收尾处理。
	void RefreshLocomotionLate();

	// 服务器设置初始速度偏航角（用于网络一致性）。
	UFUNCTION(Server, Reliable)
	void ServerSetInitialVelocityYawAngle(float NewVelocityYawAngle);

	// 多播设置初始速度偏航角（同步给所有客户端/实例）。
	UFUNCTION(NetMulticast, Reliable)
	void MulticastSetInitialVelocityYawAngle(float NewVelocityYawAngle);

	// Jumping
public:
	// 跳跃入口：触发 ALS 的跳跃逻辑（并最终驱动动画/网络同步）。
	virtual void Jump() override;

	// 跳跃回调实现：当角色进入跳跃状态后执行附加逻辑（如网络多播触发）。
	virtual void OnJumped_Implementation() override;

private:
	// 跳跃网络广播：将“已跳跃”的事件同步到其他客户端/实例。
	UFUNCTION(NetMulticast, Reliable)
	void MulticastOnJumpedNetworked();

	// 跳跃网络事件的本地处理（通常由多播回调转入）。
	void OnJumpedNetworked();

	// Rotation
public:
	// 角色旋转：Override UE 的 FaceRotation 以适配 ALS 的平滑/约束规则。
	virtual void FaceRotation(FRotator Rotation, float DeltaTime) override final;

	// 物理旋转回调：CharacterMovement 驱动旋转时的专用处理入口。
	void CharacterMovement_OnPhysicsRotation(float DeltaTime);

private:
	// 地面旋转刷新：依据移动状态选择相应旋转策略。
	void RefreshGroundedRotation(float DeltaTime);

protected:
	// 地面“移动中”自定义旋转刷新：允许扩展不同移动子状态的朝向逻辑。
	virtual bool RefreshCustomGroundedMovingRotation(float DeltaTime);

	// 地面“静止中”自定义旋转刷新：用于处理驻足/慢速等场景。
	virtual bool RefreshCustomGroundedNotMovingRotation(float DeltaTime);

	// 地面旋转平滑半衰期：用于控制插值速度的衰减曲线。
	float CalculateGroundedMovingRotationInterpolationHalfLife() const;

	// 地面瞄准旋转刷新：把 Aiming 影响叠加进最终朝向。
	void RefreshGroundedAimingRotation(float DeltaTime);

	// 瞄准角约束：对 ActorRotation 做主/次约束裁剪，避免过度旋转。
	bool ConstrainAimingRotation(FRotator& ActorRotation, float DeltaTime, bool bApplySecondaryConstraint = false);

private:
	// 旋转速度动画曲线应用：用曲线影响旋转速度参数。
	void ApplyRotationYawSpeedAnimationCurve(float DeltaTime);

	// 空中旋转刷新：空中时使用与地面不同的旋转策略/约束。
	void RefreshInAirRotation(float DeltaTime);

protected:
	// 空中自定义旋转刷新：允许针对具体空中子状态覆盖默认行为。
	virtual bool RefreshCustomInAirRotation(float DeltaTime);

	// 空中瞄准旋转刷新：将瞄准影响应用到空中朝向目标。
	void RefreshInAirAimingRotation(float DeltaTime);

	// 平滑旋转：以半衰期控制的插值方式设置目标 yaw。
	void SetRotationSmooth(float TargetYawAngle, float DeltaTime, float InterpolationHalfLife);

	// 更平滑旋转：在平滑旋转基础上额外增加旋转速度控制参数。
	void SetRotationExtraSmooth(float TargetYawAngle, float DeltaTime, float InterpolationHalfLife, float TargetYawAngleRotationSpeed);

	// 立即旋转：直接将 yaw 设置到目标角（可用于传送/瞬时更新）。
	void SetRotationInstant(float TargetYawAngle, ETeleportType Teleport = ETeleportType::None);

	// 使用 ActorRotation 反推/刷新目标 yaw（通常用于保持一致性）。
	void RefreshTargetYawAngleUsingActorRotation();

	// 直接设置目标 yaw（不立即应用到 Actor，留给后续流程）。
	void SetTargetYawAngle(float TargetYawAngle);

	// 目标 yaw 平滑到指定角：用于把期望转成最终朝向。
	void SetTargetYawAngleSmooth(float TargetYawAngle, float DeltaTime, float RotationSpeed);

	// 视角相对目标 yaw 刷新：把当前视角差换算进目标旋转。
	void RefreshViewRelativeTargetYawAngle();

	// Turn In Place（脚本 / AI）：使用与自动转身相同的蒙太奇与插槽；结束时广播 OnTurnInPlaceMontageFinished（bInterrupted 表示被打断）。
	// 需在 ViewDirection 旋转模式与第三人称下与 ALS 动画配置一致；建议在行为树中先绑定委托再调用 Request。
public:
	UPROPERTY(BlueprintAssignable, Category = "ALS|Character")
	FAlsTurnInPlaceMontageFinishedSignature OnTurnInPlaceMontageFinished;

	UFUNCTION(BlueprintCallable, Category = "ALS|Character", Meta = (AdvancedDisplay = "bFireEventIfNoTurnNeeded"))
	bool RequestTurnInPlaceTowardWorldLocation(const FVector& TargetWorldLocation, bool bFireEventIfNoTurnNeeded = true);

	UFUNCTION(BlueprintCallable, Category = "ALS|Character", Meta = (AdvancedDisplay = "bFireEventIfNoTurnNeeded"))
	bool RequestTurnInPlaceTowardActor(AActor* Target, bool bFireEventIfNoTurnNeeded = true);

	UFUNCTION(BlueprintCallable, Category = "ALS|Character", Meta = (AdvancedDisplay = "bFireEventIfNoTurnNeeded"))
	bool RequestTurnInPlaceTowardAngle(float TargetAngle, bool bFireEventIfNoTurnNeeded = true);

private:
	UFUNCTION(Server, Reliable)
	void ServerStartTurnInPlace(float TargetYaw, bool bFireEventIfNoTurnNeeded);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastStartTurnInPlace(float TargetYaw, bool bFireEventIfNoTurnNeeded);

	bool StartTurnInPlaceImplementation(float TargetYaw, bool bFireEventIfNoTurnNeeded);

	// Rolling
public:
	// 开始翻滚（Blueprint 可调用）：触发 rolling 动画与逻辑。
	UFUNCTION(BlueprintCallable, Category = "ALS|Character")
	void StartRolling(float PlayRate = 1.0f);

	// 选择翻滚蒙太奇：为不同条件选择对应的 Roll 动画。
	UFUNCTION(BlueprintNativeEvent, Category = "Als Character")
	UAnimMontage* SelectRollMontage();

	// 判断是否允许开始翻滚（基于当前状态与所选蒙太奇）。
	bool IsRollingAllowedToStart(const UAnimMontage* Montage) const;

private:
	// 翻滚内部实现：带初始/目标 yaw 的更细粒度入口。
	void StartRolling(float PlayRate, float TargetYawAngle);

	// 翻滚服务器入口：由客户端请求后执行并广播给全局。
	UFUNCTION(Server, Reliable)
	void ServerStartRolling(UAnimMontage* Montage, float PlayRate, float InitialYawAngle, float TargetYawAngle);

	// 翻滚多播：让所有实例播放同一段 rolling。
	UFUNCTION(NetMulticast, Reliable)
	void MulticastStartRolling(UAnimMontage* Montage, float PlayRate, float InitialYawAngle, float TargetYawAngle);

	// 翻滚实现：真正驱动蒙太奇/参数更新的核心逻辑。
	void StartRollingImplementation(UAnimMontage* Montage, float PlayRate, float InitialYawAngle, float TargetYawAngle);

	// 翻滚刷新：每帧/定期更新 rolling 状态机与参数。
	void RefreshRolling(float DeltaTime);

	// 翻滚物理刷新：根据物理/位移情况持续更新 rolling 的效果。
	void RefreshRollingPhysics(float DeltaTime);

	// Mantling
public:
	// 攀爬开始条件（允许性判断）：满足条件时才允许启动攀爬。
	UFUNCTION(BlueprintNativeEvent, Category = "Als Character")
	bool IsMantlingAllowedToStart() const;

	// 攀爬开始（地面版本）：执行 grounded mantling 的启动流程。
	UFUNCTION(BlueprintCallable, Category = "ALS|Character", Meta = (ReturnDisplayName = "Success"))
	bool StartMantlingGrounded();

private:
	// 攀爬开始（空中版本）：在空中/下落阶段尝试启动攀爬。
	bool StartMantlingInAir();

	// 攀爬开始（通用入口）：通过 TraceSettings/检测结果驱动启动。
	bool StartMantling(const FAlsMantlingTraceSettings& TraceSettings);

	// 攀爬服务器入口：由客户端请求并由服务器发起网络同步。
	UFUNCTION(Server, Reliable)
	void ServerStartMantling(const FAlsMantlingParameters& Parameters);

	// 攀爬多播：让所有实例播放一致的攀爬蒙太奇/参数。
	UFUNCTION(NetMulticast, Reliable)
	void MulticastStartMantling(const FAlsMantlingParameters& Parameters);

	// 攀爬实现：真正应用参数并驱动蒙太奇/状态机。
	void StartMantlingImplementation(const FAlsMantlingParameters& Parameters);

protected:
	// 选择攀爬设置：根据攀爬类型选择不同的 mantling 配置资源。
	UFUNCTION(BlueprintNativeEvent, Category = "Als Character")
	UAlsMantlingSettings* SelectMantlingSettings(EAlsMantlingType MantlingType);

	// 计算攀爬起始时间：把攀爬高度与配置映射到启动时间偏移。
	float CalculateMantlingStartTime(const UAlsMantlingSettings* MantlingSettings, float MantlingHeight) const;

	// 攀爬开始通知：用于蓝图扩展攀爬开始的附加逻辑。
	UFUNCTION(BlueprintNativeEvent, Category = "Als Character")
	void OnMantlingStarted(const FAlsMantlingParameters& Parameters);

private:
	// 攀爬刷新：更新攀爬过程中的状态与参数。
	void RefreshMantling();

	// 结束攀爬：可选择是否停止蒙太奇。
	void StopMantling(bool bStopMontage = false);

protected:
	// 攀爬结束通知：用于蓝图扩展攀爬结束后的后处理。
	UFUNCTION(BlueprintNativeEvent, Category = "Als Character")
	void OnMantlingEnded();

	// Ragdolling
public:
	// 获取布娃娃状态机数据（ragdolling state）。
	const FAlsRagdollingState& GetRagdollingState() const;

	// 是否允许开始布娃娃：基于当前状态/条件判断能否进入 ragdoll。
	bool IsRagdollingAllowedToStart() const;

	// 开始布娃娃：进入 ragdoll 状态并驱动动画/物理切换。
	UFUNCTION(BlueprintCallable, Category = "ALS|Character")
	void StartRagdolling();

private:
	// 布娃娃服务器入口：由客户端发起，请求服务器开始 ragdoll。
	UFUNCTION(Server, Reliable)
	void ServerStartRagdolling();

	// 布娃娃多播：让所有实例同步开始 ragdoll。
	UFUNCTION(NetMulticast, Reliable)
	void MulticastStartRagdolling();

	// 布娃娃实现：执行 ragdoll 的关键参数/蒙太奇与物理切换。
	void StartRagdollingImplementation();

protected:
	// 布娃娃开始通知：用于蓝图扩展 ragdoll started 的逻辑。
	UFUNCTION(BlueprintNativeEvent, Category = "Als Character")
	void OnRagdollingStarted();

public:
	// 是否允许停止布娃娃：判断是否满足从 ragdoll 恢复的条件。
	bool IsRagdollingAllowedToStop() const;

	// 停止布娃娃：触发 get-up 恢复流程（并切回角色动画）。
	UFUNCTION(BlueprintCallable, Category = "ALS|Character", Meta = (ReturnDisplayName = "Success"))
	bool StopRagdolling();

private:
	// 停止布娃娃服务器入口。
	UFUNCTION(Server, Reliable)
	void ServerStopRagdolling();

	// 停止布娃娃多播：让所有实例同步恢复。
	UFUNCTION(NetMulticast, Reliable)
	void MulticastStopRagdolling();

	// 停止布娃娃实现：实际执行从物理恢复到动画的切换。
	void StopRagdollingImplementation();

protected:
	// 选择起身蒙太奇：根据面向方向选择 get-up 动画。
	UFUNCTION(BlueprintNativeEvent, Category = "Als Character")
	UAnimMontage* SelectGetUpMontage(bool bRagdollFacingUpward);

	// 起身/布娃娃结束通知：用于蓝图扩展结束后的逻辑。
	UFUNCTION(BlueprintNativeEvent, Category = "Als Character")
	void OnRagdollingEnded();

private:
	// 设置布娃娃目标落点：用于约束恢复位置或朝向。
	void SetRagdollTargetLocation(const FVector& NewTargetLocation);

	// 布娃娃落点服务器同步（不要求可靠性但用于更新目标）。
	UFUNCTION(Server, Unreliable)
	void ServerSetRagdollTargetLocation(const FVector_NetQuantize& NewTargetLocation);

	// ragdoll 刷新：更新物理约束、落地检测与状态推进。
	void RefreshRagdolling(float DeltaTime);

	// ragdoll 地面探测：trace 地面并输出是否落在地面。
	FVector RagdollTraceGround(bool& bGrounded) const;

	// 限制 ragdoll 速度：避免速度过大导致不合理物理表现。
	void ConstraintRagdollSpeed() const;

	// Debug
public:
	// 在 UE 的调试画面上展示 ALS 内部状态（含曲线/状态/trace 等）。
	virtual void DisplayDebug(UCanvas* Canvas, const FDebugDisplayInfo& DisplayInfo, float& Unused, float& VerticalLocation) override;

private:
	// Debug 标题绘制：在屏幕上输出调试信息的头部格式。
	static void DisplayDebugHeader(const UCanvas* Canvas, const FText& HeaderText, const FLinearColor& HeaderColor,
	                               float Scale, float HorizontalLocation, float& VerticalLocation);

	// Debug 曲线绘制：把用于动画/旋转计算的曲线以可视化方式输出。
	void DisplayDebugCurves(const UCanvas* Canvas, float Scale, float HorizontalLocation, float& VerticalLocation) const;

	// Debug 状态绘制：输出当前 Locomotion/View 等内部状态值。
	void DisplayDebugState(const UCanvas* Canvas, float Scale, float HorizontalLocation, float& VerticalLocation) const;

	// Debug 形体绘制：可视化物理/碰撞/约束等形体信息。
	void DisplayDebugShapes(const UCanvas* Canvas, float Scale, float HorizontalLocation, float& VerticalLocation) const;

	// Debug trace 绘制：可视化射线/探测结果。
	void DisplayDebugTraces(const UCanvas* Canvas, float Scale, float HorizontalLocation, float& VerticalLocation) const;

	// Debug 攀爬可视化：输出 mantling 相关 trace/参数。
	void DisplayDebugMantling(const UCanvas* Canvas, float Scale, float HorizontalLocation, float& VerticalLocation) const;
};

// 获取角色设置对象（Settings），用于读取 ALS 的配置参数。
inline const UAlsCharacterSettings* AAlsCharacter::GetSettings() const
{
	return Settings;
}

// 获取当前视角模式（ViewMode）。
inline const FGameplayTag& AAlsCharacter::GetViewMode() const
{
	return ViewMode;
}

// 获取当前移动/位移模式（LocomotionMode）。
inline const FGameplayTag& AAlsCharacter::GetLocomotionMode() const
{
	return LocomotionMode;
}

// 判断当前是否处于“期望瞄准（DesiredAiming）”状态。
inline bool AAlsCharacter::IsDesiredAiming() const
{
	return bDesiredAiming;
}

// 获取期望旋转模式（DesiredRotationMode）。
inline const FGameplayTag& AAlsCharacter::GetDesiredRotationMode() const
{
	return DesiredRotationMode;
}

// 获取当前实际旋转模式（RotationMode）。
inline const FGameplayTag& AAlsCharacter::GetRotationMode() const
{
	return RotationMode;
}

// 获取期望站姿（DesiredStance）。
inline const FGameplayTag& AAlsCharacter::GetDesiredStance() const
{
	return DesiredStance;
}

// 获取当前实际站姿（Stance）。
inline const FGameplayTag& AAlsCharacter::GetStance() const
{
	return Stance;
}

// 获取期望步态（DesiredGait）。
inline const FGameplayTag& AAlsCharacter::GetDesiredGait() const
{
	return DesiredGait;
}

// 获取当前实际步态（Gait）。
inline const FGameplayTag& AAlsCharacter::GetGait() const
{
	return Gait;
}

// 获取当前叠加模式（OverlayMode）。
inline const FGameplayTag& AAlsCharacter::GetOverlayMode() const
{
	return OverlayMode;
}

// 获取当前 LocomotionAction（位移动作标记）。
inline const FGameplayTag& AAlsCharacter::GetLocomotionAction() const
{
	return LocomotionAction;
}

// 获取当前输入方向向量（InputDirection）。
inline const FVector& AAlsCharacter::GetInputDirection() const
{
	return InputDirection;
}

// 获取 ViewState（含网络平滑后的最终视角状态）。
inline const FAlsViewState& AAlsCharacter::GetViewState() const
{
	return ViewState;
}

// 获取 LocomotionState（位移/动作系统内部状态）。
inline const FAlsLocomotionState& AAlsCharacter::GetLocomotionState() const
{
	return LocomotionState;
}

// 获取布娃娃状态机数据（RagdollingState）。
inline const FAlsRagdollingState& AAlsCharacter::GetRagdollingState() const
{
	return RagdollingState;
}
