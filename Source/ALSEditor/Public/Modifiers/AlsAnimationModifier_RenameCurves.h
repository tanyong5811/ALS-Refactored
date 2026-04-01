#pragma once

#include "AnimationModifier.h"
#include "AlsAnimationModifier_RenameCurves.generated.h"

USTRUCT(BlueprintType)
struct ALSEDITOR_API FAlsCurveRenameEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ALS")
	FName OldName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ALS")
	FName NewName;
};

UCLASS(DisplayName = "Als Rename Curves Animation Modifier")
class ALSEDITOR_API UAlsAnimationModifier_RenameCurves : public UAnimationModifier
{
	GENERATED_BODY()

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings")
	TArray<FAlsCurveRenameEntry> CurveRenames
	{
		{.OldName = FName{TEXT("Layering_Arm_L")},       .NewName = FName{TEXT("LayerArmLeft")}},
		{.OldName = FName{TEXT("Layering_Arm_L_Add")},    .NewName = FName{TEXT("LayerArmLeftAdditive")}},
		{.OldName = FName{TEXT("Layering_Arm_L_LS")},     .NewName = FName{TEXT("LayerArmLeftLocalSpace")}},
		{.OldName = FName{TEXT("Layering_Arm_R")},        .NewName = FName{TEXT("LayerArmRight")}},
		{.OldName = FName{TEXT("Layering_Arm_R_Add")},    .NewName = FName{TEXT("LayerArmRightAdditive")}},
		{.OldName = FName{TEXT("Layering_Arm_R_LS")},     .NewName = FName{TEXT("LayerArmRightLocalSpace")}},
		{.OldName = FName{TEXT("Layering_Hand_L")},       .NewName = FName{TEXT("LayerHandLeft")}},
		{.OldName = FName{TEXT("Layering_Hand_R")},       .NewName = FName{TEXT("LayerHandRight")}},
		{.OldName = FName{TEXT("Layering_Head")},          .NewName = FName{TEXT("LayerHead")}},
		{.OldName = FName{TEXT("Layering_Head_Add")},     .NewName = FName{TEXT("LayerHeadAdditive")}},
		{.OldName = FName{TEXT("Layering_Legs")},          .NewName = FName{TEXT("LayerLegs")}},
		{.OldName = FName{TEXT("Layering_Pelvis")},        .NewName = FName{TEXT("LayerPelvis")}},
		{.OldName = FName{TEXT("Layering_Spine")},         .NewName = FName{TEXT("LayerSpine")}},
		{.OldName = FName{TEXT("Layering_Spine_Add")},    .NewName = FName{TEXT("LayerSpineAdditive")}},
		{.OldName = FName{TEXT("RotationAmount")},    .NewName = FName{TEXT("RotationYawSpeed")}},
		{.OldName = FName{TEXT("FootLock_L")},    .NewName = FName{TEXT("FootLeftLock")}},
		{.OldName = FName{TEXT("FootLock_R")},    .NewName = FName{TEXT("FootRightLock")}},
		{.OldName = FName{TEXT("Frame")},    .NewName = FName{TEXT("Frame")}},
		{.OldName = FName{TEXT("BlendWeight")},    .NewName = FName{TEXT("BlendWeight")}},
		{.OldName = FName{TEXT("MoveAmount_X")},    .NewName = FName{TEXT("MoveAmount_X")}},
		{.OldName = FName{TEXT("MoveAmount_Y")},    .NewName = FName{TEXT("MoveAmount_Y")}},
		{.OldName = FName{TEXT("Enable_HandIK_L")},    .NewName = FName{TEXT("HandLeftIk")}},
		{.OldName = FName{TEXT("Enable_HandIK_R")},    .NewName = FName{TEXT("HandRightIk")}},
		{.OldName = FName{TEXT("Feet_Position")},    .NewName = FName{TEXT("FootPlanted")}},
		{.OldName = FName{TEXT("Weight_Gait")},    .NewName = FName{TEXT("PoseGait")}},
		{.OldName = FName{TEXT("Weight_Moving")},    .NewName = FName{TEXT("PoseMoving")}},
		{.OldName = FName{TEXT("Weight_Standing")},    .NewName = FName{TEXT("PoseStanding")}},
		{.OldName = FName{TEXT("Weight_Crouching")},    .NewName = FName{TEXT("PoseCrouching")}},
		{.OldName = FName{TEXT("Weight_InAir")},    .NewName = FName{TEXT("PoseInAir")}},
		{.OldName = FName{TEXT("Weight_Grounded")},    .NewName = FName{TEXT("PoseGrounded")}},
	};
	
public:
	virtual void OnApply_Implementation(UAnimSequence* Sequence) override;

	virtual void OnRevert_Implementation(UAnimSequence* Sequence) override;
};
