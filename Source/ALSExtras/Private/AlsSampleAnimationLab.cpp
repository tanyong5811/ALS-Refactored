#include "AlsSampleAnimationLab.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AlsSampleAnimationLab)

#include "AlsAnimationInstance.h"
#include "Animation/AnimMontage.h"
#include "AlsCharacterMovementComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "HAL/IConsoleManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Settings/AlsCharacterSettings.h"
#include "Settings/AlsMovementSettings.h"
#include "UObject/ConstructorHelpers.h"

#if !UE_BUILD_SHIPPING
namespace
{
	TAutoConsoleVariable<int32> AutoPlaySampleMontage{
		TEXT("ALS.SampleLab.AutoPlayMontage"), 0,
		TEXT("Play the selected sample montage after the sample profile is initialized."), ECVF_Default};

	FAutoConsoleCommandWithWorld PlaySelectedSampleMontage{
		TEXT("ALS.SampleLab.PlaySelectedMontage"),
		TEXT("Play the montage currently selected by the ALS sample animation lab character."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			auto* Controller{World ? World->GetFirstPlayerController() : nullptr};
			if (auto* Character{Controller ? Cast<AAlsSampleAnimationLabCharacter>(Controller->GetPawn()) : nullptr})
			{
				Character->PlaySelectedMontage();
			}
		})};
}
#endif

AAlsSampleAnimationLabCharacter::AAlsSampleAnimationLabCharacter(const FObjectInitializer& Initializer) : Super(Initializer)
{
	PrimaryActorTick.bCanEverTick = true;
	static ConstructorHelpers::FObjectFinder<UAlsCharacterSettings> CharacterSettings{TEXT("/ALS/ALS/Data/Character/CS_Als_Default")};
	static ConstructorHelpers::FObjectFinder<UAlsMovementSettings> Movement{TEXT("/ALS/ALS/Data/Character/Movement/MS_Als_Normal")};
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> SampleMesh{TEXT("/ALS/ALS/Character/SKM_Als")};
	static ConstructorHelpers::FClassFinder<UAnimInstance> Animation{TEXT("/ALS/ALS/Character/AB_Als")};
	Settings = CharacterSettings.Object;
	MovementSettings = Movement.Object;
	GetMesh()->SetSkeletalMesh(SampleMesh.Object);
	GetMesh()->SetAnimInstanceClass(Animation.Class);
	GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	DesiredRotationMode = AlsRotationModeTags::ViewDirection;
	DesiredGait = AlsGaitTags::Running;
	ProfileTable = TSoftObjectPtr<UDataTable>{FSoftObjectPath{TEXT("/Game/ALSAnimationAdaptation/DT_AlsSampleProfiles.DT_AlsSampleProfiles")}};

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(GetRootComponent());
	CameraBoom->TargetArmLength = 450.0f;
	CameraBoom->SocketOffset = FVector{0.0, 45.0, 30.0};
	CameraBoom->bUsePawnControlRotation = true;
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(CameraBoom);
	Camera->FieldOfView = 80.0f;
}

void AAlsSampleAnimationLabCharacter::BeginPlay()
{
	Super::BeginPlay();
	if (auto* PlayerController{Cast<APlayerController>(GetController())})
	{
		PlayerController->SetControlRotation(FRotator{-12.0, 0.0, 0.0});
		PlayerController->SetInputMode(FInputModeGameOnly{});
		PlayerController->bShowMouseCursor = false;
	}
	RefreshProfiles();
	if (!ProfileNames.IsEmpty())
	{
		SelectProfile(0);
	#if !UE_BUILD_SHIPPING
		if (AutoPlaySampleMontage.GetValueOnGameThread() != 0 ||
			FParse::Param(FCommandLine::Get(), TEXT("AlsSampleLabAutoPlayMontage")))
		{
			PlaySelectedMontage();
		}
	#endif
	}
}

void AAlsSampleAnimationLabCharacter::RefreshProfiles()
{
	ProfileNames.Reset();
	if (const auto* Table{ProfileTable.LoadSynchronous()})
	{
		ProfileNames = Table->GetRowNames();
		ProfileNames.Sort(FNameLexicalLess{});
	}
	Status = ProfileNames.IsEmpty() ? TEXT("No generated profiles. Run ALS Animation Adaptation first.") : TEXT("Profiles loaded");
}

bool AAlsSampleAnimationLabCharacter::SelectProfile(const int32 Index)
{
	const auto* Table{ProfileTable.LoadSynchronous()};
	if (!Table || !ProfileNames.IsValidIndex(Index))
	{
		Status = TEXT("Profile index or table is invalid");
		return false;
	}
	const auto* Row{Table->FindRow<FAlsSampleAnimationProfileRow>(ProfileNames[Index], TEXT("ALS Sample Lab"))};
	auto* Animation{GetMesh()->GetAnimInstance()};
	if (!Row || !Animation || !Row->MovementSettings || !Row->AnimationInstanceSettings ||
		!Row->StandingAnimInstance || !Row->BasePoseAnimInstance || !Row->OverlayAnimInstance)
	{
		Status = TEXT("Profile is incomplete");
		return false;
	}
	MovementSettings = Row->MovementSettings;
	CastChecked<UAlsCharacterMovementComponent>(GetCharacterMovement())->SetMovementSettings(Row->MovementSettings);
	SetAnimationInstanceSettings(Row->AnimationInstanceSettings);
	Animation->LinkAnimGraphByTag(TEXT("Standing"), Row->StandingAnimInstance);
	Animation->LinkAnimClassLayers(Row->OverlayAnimInstance);
	Animation->LinkAnimClassLayers(Row->BasePoseAnimInstance);
	Animation->Montage_Stop(0.1f);
	ProfileIndex = Index;
	CurrentProfile = ProfileNames[Index];
	Montages = Row->Montages;
	MontageIndex = Montages.IsEmpty() ? INDEX_NONE : 0;
	Status = Montages.IsEmpty()
		? FString::Printf(TEXT("Profile ready: %s | no montages configured"), *CurrentProfile.ToString())
		: FString::Printf(TEXT("Profile ready: %s | montage: %s"), *CurrentProfile.ToString(), *GetSelectedMontageName());
	UE_LOG(LogTemp, Display, TEXT("ALS Sample Animation Lab selected %s (%d profiles)"),
		*CurrentProfile.ToString(), ProfileNames.Num());
	return true;
}

void AAlsSampleAnimationLabCharacter::SelectMontageOffset(const int32 Offset)
{
	if (!Montages.IsEmpty())
	{
		MontageIndex = (MontageIndex + Offset + Montages.Num()) % Montages.Num();
		Status = FString::Printf(TEXT("Montage selected: %s"), *GetSelectedMontageName());
	}
	else
	{
		Status = TEXT("No montages configured for this profile");
	}
}

bool AAlsSampleAnimationLabCharacter::PlaySelectedMontage()
{
	auto* Animation{GetMesh()->GetAnimInstance()};
	if (!Animation || !Montages.IsValidIndex(MontageIndex))
	{
		Status = TEXT("No montage selected");
		return false;
	}
	auto* Montage{Montages[MontageIndex].LoadSynchronous()};
	if (!Montage)
	{
		Status = FString::Printf(TEXT("Montage failed to load: %s"), *GetSelectedMontageName());
		return false;
	}
	for (const auto& Track : Montage->SlotAnimTracks)
	{
		if (Track.SlotName != TEXT("PostLocomotion"))
		{
			Status = FString::Printf(TEXT("Montage uses noncanonical slot %s: %s"),
				*Track.SlotName.ToString(), *Montage->GetName());
			return false;
		}
	}
	Animation->Montage_Stop(0.1f);
	if (Animation->Montage_Play(Montage) <= 0.0f)
	{
		Status = FString::Printf(TEXT("Montage failed to play: %s"), *Montage->GetName());
		return false;
	}
	Status = FString::Printf(TEXT("Playing montage: %s"), *Montage->GetName());
	UE_LOG(LogTemp, Display, TEXT("ALS Sample Animation Lab playing montage %s for profile %s"),
		*Montage->GetPathName(), *CurrentProfile.ToString());
	return true;
}

FString AAlsSampleAnimationLabCharacter::GetSelectedMontageName() const
{
	return Montages.IsValidIndex(MontageIndex) ? Montages[MontageIndex].ToSoftObjectPath().GetAssetName() : TEXT("None");
}

void AAlsSampleAnimationLabCharacter::SelectProfileOffset(const int32 Offset)
{
	if (!ProfileNames.IsEmpty())
	{
		SelectProfile((ProfileIndex + Offset + ProfileNames.Num()) % ProfileNames.Num());
	}
}

void AAlsSampleAnimationLabCharacter::MoveCameraRelative(const float Forward, const float Right)
{
	if (!GetController())
	{
		return;
	}
	const FVector2D Input{FVector2D{Forward, Right}.GetClampedToMaxSize(1.0)};
	const FRotator Yaw{0.0, GetController()->GetControlRotation().Yaw, 0.0};
	AddMovementInput(Yaw.Vector(), Input.X);
	AddMovementInput(FRotationMatrix{Yaw}.GetUnitAxis(EAxis::Y), Input.Y);
}

void AAlsSampleAnimationLabCharacter::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	auto* PlayerController{Cast<APlayerController>(GetController())};
	if (!PlayerController || !PlayerController->IsLocalController())
	{
		return;
	}
	MoveCameraRelative(float(PlayerController->IsInputKeyDown(EKeys::W)) - float(PlayerController->IsInputKeyDown(EKeys::S)),
		float(PlayerController->IsInputKeyDown(EKeys::D)) - float(PlayerController->IsInputKeyDown(EKeys::A)));
	float MouseX, MouseY;
	PlayerController->GetInputMouseDelta(MouseX, MouseY);
	auto View{PlayerController->GetControlRotation()};
	View.Yaw += MouseX * MouseLookSensitivity;
	View.Pitch = FMath::Clamp(FRotator::NormalizeAxis(View.Pitch) + MouseY * MouseLookSensitivity, -65.0f, 35.0f);
	PlayerController->SetControlRotation(View);
	SetDesiredGait(PlayerController->IsInputKeyDown(EKeys::LeftControl) ? AlsGaitTags::Walking :
		(PlayerController->IsInputKeyDown(EKeys::LeftShift) ? AlsGaitTags::Sprinting : AlsGaitTags::Running));
	SetDesiredAiming(PlayerController->IsInputKeyDown(EKeys::RightMouseButton));
	const bool Previous{PlayerController->IsInputKeyDown(EKeys::Q)};
	const bool Next{PlayerController->IsInputKeyDown(EKeys::E)};
	if (Previous && !bPreviousProfileKey) SelectProfileOffset(-1);
	if (Next && !bNextProfileKey) SelectProfileOffset(1);
	bPreviousProfileKey = Previous;
	bNextProfileKey = Next;
	static const FKey NumberKeys[]{EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five,
		EKeys::Six, EKeys::Seven, EKeys::Eight, EKeys::Nine};
	for (int32 Index{0}; Index < FMath::Min(9, ProfileNames.Num()); ++Index)
	{
		if (PlayerController->WasInputKeyJustPressed(NumberKeys[Index])) SelectProfile(Index);
	}
	if (PlayerController->WasInputKeyJustPressed(EKeys::LeftBracket)) SelectMontageOffset(-1);
	if (PlayerController->WasInputKeyJustPressed(EKeys::RightBracket)) SelectMontageOffset(1);
	if (PlayerController->WasInputKeyJustPressed(EKeys::Enter)) PlaySelectedMontage();
	if (PlayerController->WasInputKeyJustPressed(EKeys::C))
	{
		SetDesiredStance(GetDesiredStance() == AlsStanceTags::Standing ? AlsStanceTags::Crouching : AlsStanceTags::Standing);
	}
	if (GetActorLocation().Z < -1000.0f)
	{
		SetActorLocationAndRotation(FVector{0.0, 0.0, 110.0}, FRotator::ZeroRotator, false, nullptr, ETeleportType::TeleportPhysics);
	}
}

void AAlsSampleAnimationLabHud::DrawHUD()
{
	Super::DrawHUD();
	const auto* Character{PlayerOwner ? Cast<AAlsSampleAnimationLabCharacter>(PlayerOwner->GetPawn()) : nullptr};
	if (!Character || !Canvas)
	{
		return;
	}
	DrawRect(FLinearColor{0.015f, 0.025f, 0.04f, 0.85f}, 12.0f, 12.0f, FMath::Min(980.0f, float(Canvas->SizeX) - 24.0f), 168.0f);
	DrawText(TEXT("ALS Sample Animation Lab | Q/E or 1-9: switch generated profile"), FLinearColor{0.4f, 0.85f, 1.0f}, 24.0f, 22.0f);
	DrawText(FString::Printf(TEXT("Profile %d/%d: %s"), Character->GetProfileIndex() + 1, Character->GetProfileCount(),
		*Character->CurrentProfile.ToString()), FLinearColor::White, 24.0f, 48.0f);
	DrawText(TEXT("WASD move | Mouse look | Ctrl walk | Shift sprint | RMB aim | C crouch"), FLinearColor::White, 24.0f, 74.0f);
	DrawText(FString::Printf(TEXT("Montage %d/%d: %s | [ / ] select | Enter play"),
		Character->GetMontageIndex() + 1, Character->GetMontageCount(), *Character->GetSelectedMontageName()),
		FLinearColor{1.0f, 0.8f, 0.35f}, 24.0f, 100.0f);
	DrawText(Character->Status, FLinearColor{0.65f, 1.0f, 0.65f}, 24.0f, 126.0f);
}

AAlsSampleAnimationLabGameMode::AAlsSampleAnimationLabGameMode()
{
	DefaultPawnClass = AAlsSampleAnimationLabCharacter::StaticClass();
	HUDClass = AAlsSampleAnimationLabHud::StaticClass();
}
