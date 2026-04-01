#pragma once

#include "AnimationModifier.h"
#include "Utility/AlsConstants.h"
#include "AlsAnimationModifier_InitMoveCycles.generated.h"

UCLASS(DisplayName = "Als Init Move Cycles Animation Modifier")
class ALSEDITOR_API UAlsAnimationModifier_InitMoveCycles : public UAnimationModifier
{
	GENERATED_BODY()

protected:
	// Move Speed

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Move|Speed")
	FName MoveSpeedCurveName{TEXT("MoveSpeed")};

	UPROPERTY(BlueprintReadOnly, Category = "Move|Speed")
	FName RootBoneName{UAlsConstants::RootBoneName()};

	// Weight Gait

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Move|Weight Group")
	FName WeightGaitCurveName{TEXT("PoseGait")};

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Move|Weight Group")
	float WeightGaitValue{1.0f};

	// Feet

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Move|Feet")
	FName FeetTrackChannelName{TEXT("FootSyncMarkers")};

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Move|Feet")
	FName FeetPositionCurveName{TEXT("FootPlanted")};

	// 左脚完全踩下
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Move|Feet")
	float LeftFeetDownTime{0.0f};
	// 右脚完全踩下
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Move|Feet")
	float RightFeetDownTime{0.0f};
	// FallTime = DownTime - FootFallLookBackTime (looping-safe).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Move|Feet", Meta = (ClampMin = 0.0, ClampMax = 1.0))
	float FootFallLookBackTime{0.2f};

	// Root Offset

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Move|Root Offset")
	uint8 bCreateRootOffset : 1 {false};

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Move|Root Offset", Meta = (EditCondition = "bCreateRootOffset"))
	uint8 bCreateRootOffsetX : 1 {true};

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Move|Root Offset", Meta = (EditCondition = "bCreateRootOffset"))
	uint8 bCreateRootOffsetY : 1 {true};

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Move|Root Offset", Meta = (EditCondition = "bCreateRootOffset"))
	FName FrameCurveName{TEXT("Frame")};

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Move|Root Offset", Meta = (EditCondition = "bCreateRootOffset"))
	FName BlendWeightCurveName{TEXT("BlendWeight")};

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Move|Root Offset", Meta = (EditCondition = "bCreateRootOffset"))
	FName RootOffsetXCurveName{TEXT("MoveAmount_X")};

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Move|Root Offset", Meta = (EditCondition = "bCreateRootOffset"))
	FName RootOffsetYCurveName{TEXT("MoveAmount_Y")};

public:
	virtual void OnApply_Implementation(UAnimSequence* Sequence) override;

private:
	void EnsureCurveExists(UAnimSequence* Sequence, const FName& CurveName) const;
};
