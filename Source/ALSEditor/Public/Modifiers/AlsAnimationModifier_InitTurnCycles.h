#pragma once

#include "AnimationModifier.h"
#include "Utility/AlsConstants.h"
#include "AlsAnimationModifier_InitTurnCycles.generated.h"

UCLASS(DisplayName = "Als Init Turn Cycles Animation Modifier")
class ALSEDITOR_API UAlsAnimationModifier_InitTurnCycles : public UAnimationModifier
{
	GENERATED_BODY()

protected:
	// Turn

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Turn")
	FName RotationYawSpeedCurveName{UAlsConstants::RotationYawSpeedCurveName()};

	// Root Offset

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Root Offset")
	uint8 bCreateTurnRootOffset : 1 {false};

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Root Offset", Meta = (EditCondition = "bCreateTurnRootOffset"))
	FName RootBoneName{UAlsConstants::RootBoneName()};

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Root Offset", Meta = (EditCondition = "bCreateTurnRootOffset"))
	FName FrameCurveName{TEXT("Frame")};

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Root Offset", Meta = (EditCondition = "bCreateTurnRootOffset"))
	FName BlendWeightCurveName{TEXT("BlendWeight")};

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Root Offset", Meta = (EditCondition = "bCreateTurnRootOffset"))
	FName RootOffsetXCurveName{TEXT("MoveAmount_X")};

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Root Offset", Meta = (EditCondition = "bCreateTurnRootOffset"))
	FName RootOffsetYCurveName{TEXT("MoveAmount_Y")};

	// Foot Lock

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Foot Lock")
	uint8 bCreateFootLock : 1 {false};

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Foot Lock", Meta = (EditCondition = "bCreateFootLock"))
	FName FootLockLeftCurveName{UAlsConstants::FootLeftLockCurveName()};

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Foot Lock", Meta = (EditCondition = "bCreateFootLock"))
	FName FootLockRightCurveName{UAlsConstants::FootRightLockCurveName()};

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Foot Lock", Meta = (EditCondition = "bCreateFootLock"))
	TMap<float, bool> FootLockLeftMap;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Foot Lock", Meta = (EditCondition = "bCreateFootLock"))
	TMap<float, bool> FootLockRightMap;

public:
	virtual void OnApply_Implementation(UAnimSequence* Sequence) override;

private:
	void EnsureCurveExists(UAnimSequence* Sequence, const FName& CurveName) const;

	void CreateFootLockCurve(UAnimSequence* Sequence, const FName& CurveName, const TMap<float, bool>& FootLockMap) const;
};
