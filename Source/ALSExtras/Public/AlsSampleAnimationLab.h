#pragma once

#include "AlsCharacter.h"
#include "Engine/DataTable.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/HUD.h"
#include "AlsSampleAnimationLab.generated.h"

class UAlsAnimationInstanceSettings;
class UAlsMovementSettings;
class UAnimMontage;
class UCameraComponent;
class USpringArmComponent;

USTRUCT(BlueprintType)
struct ALSEXTRAS_API FAlsSampleAnimationProfileRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<UAlsMovementSettings> MovementSettings;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<UAlsAnimationInstanceSettings> AnimationInstanceSettings;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<UAnimInstance> StandingAnimInstance;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<UAnimInstance> BasePoseAnimInstance;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<UAnimInstance> OverlayAnimInstance;

	/** Optional profile-specific actions exposed by the sample map. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<TSoftObjectPtr<UAnimMontage>> Montages;
};

/** Plugin-owned playable character for testing generated animation profiles. */
UCLASS()
class ALSEXTRAS_API AAlsSampleAnimationLabCharacter : public AAlsCharacter
{
	GENERATED_BODY()

public:
	AAlsSampleAnimationLabCharacter(const FObjectInitializer& Initializer = FObjectInitializer::Get());
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ALS Sample Lab")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ALS Sample Lab")
	TObjectPtr<UCameraComponent> Camera;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ALS Sample Lab")
	TSoftObjectPtr<UDataTable> ProfileTable;

	UPROPERTY(BlueprintReadOnly, Category = "ALS Sample Lab")
	FName CurrentProfile;

	UPROPERTY(BlueprintReadOnly, Category = "ALS Sample Lab")
	FString Status;

	UFUNCTION(BlueprintCallable, Category = "ALS Sample Lab")
	bool SelectProfile(int32 Index);

	UFUNCTION(BlueprintCallable, Category = "ALS Sample Lab")
	void SelectProfileOffset(int32 Offset);

	UFUNCTION(BlueprintCallable, Category = "ALS Sample Lab")
	void SelectMontageOffset(int32 Offset);

	UFUNCTION(BlueprintCallable, Category = "ALS Sample Lab")
	bool PlaySelectedMontage();

	UFUNCTION(BlueprintPure, Category = "ALS Sample Lab")
	int32 GetProfileCount() const { return ProfileNames.Num(); }

	UFUNCTION(BlueprintPure, Category = "ALS Sample Lab")
	int32 GetProfileIndex() const { return ProfileIndex; }

	UFUNCTION(BlueprintPure, Category = "ALS Sample Lab")
	int32 GetMontageCount() const { return Montages.Num(); }

	UFUNCTION(BlueprintPure, Category = "ALS Sample Lab")
	int32 GetMontageIndex() const { return MontageIndex; }

	UFUNCTION(BlueprintPure, Category = "ALS Sample Lab")
	FString GetSelectedMontageName() const;

private:
	void RefreshProfiles();
	void MoveCameraRelative(float Forward, float Right);
	TArray<FName> ProfileNames;
	TArray<TSoftObjectPtr<UAnimMontage>> Montages;
	int32 ProfileIndex{INDEX_NONE};
	int32 MontageIndex{INDEX_NONE};
	bool bPreviousProfileKey{false};
	bool bNextProfileKey{false};
	float MouseLookSensitivity{0.32f};
};

UCLASS()
class ALSEXTRAS_API AAlsSampleAnimationLabHud : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;
};

UCLASS()
class ALSEXTRAS_API AAlsSampleAnimationLabGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AAlsSampleAnimationLabGameMode();
};
