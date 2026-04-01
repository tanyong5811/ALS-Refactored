#include "Modifiers/AlsAnimationModifier_RenameCurves.h"

#include "Animation/AnimData/IAnimationDataController.h"
#include "Animation/AnimData/IAnimationDataModel.h"
#include "Animation/AnimSequence.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AlsAnimationModifier_RenameCurves)

namespace AlsRenameCurvesInternal
{
	static bool RenameFloatCurveSimple(IAnimationDataController& Controller, const IAnimationDataModel* Model,
		const FName FromName, const FName ToName)
	{
		if (!Model || FromName.IsNone() || ToName.IsNone() || FromName == ToName)
		{
			return false;
		}

		const FAnimationCurveIdentifier FromId{FromName, ERawCurveTrackTypes::RCT_Float};
		if (!Model->FindCurve(FromId))
		{
			return false;
		}

		const FAnimationCurveIdentifier ToId{ToName, ERawCurveTrackTypes::RCT_Float};
		if (Model->FindCurve(ToId))
		{
			// Keep existing target curve untouched. We only migrate when destination is free.
			return false;
		}

		return Controller.RenameCurve(FromId, ToId, false);
	}
}

void UAlsAnimationModifier_RenameCurves::OnApply_Implementation(UAnimSequence* Sequence)
{
	Super::OnApply_Implementation(Sequence);

	IAnimationDataController& Controller{Sequence->GetController()};
	const IAnimationDataModel* Model{Controller.GetModel()};
	if (!Model)
	{
		return;
	}

	IAnimationDataController::FScopedBracket ScopedBracket(Controller, FText::FromString(TEXT("ALS Rename Curves")), false);

	for (const auto& Entry : CurveRenames)
	{
		AlsRenameCurvesInternal::RenameFloatCurveSimple(Controller, Model, Entry.OldName, Entry.NewName);
	}
}

void UAlsAnimationModifier_RenameCurves::OnRevert_Implementation(UAnimSequence* Sequence)
{
	Super::OnRevert_Implementation(Sequence);

	IAnimationDataController& Controller{Sequence->GetController()};
	const IAnimationDataModel* Model{Controller.GetModel()};
	if (!Model)
	{
		return;
	}

	IAnimationDataController::FScopedBracket ScopedBracket(Controller, FText::FromString(TEXT("ALS Revert Rename Curves")), false);

	for (const auto& Entry : CurveRenames)
	{
		AlsRenameCurvesInternal::RenameFloatCurveSimple(Controller, Model, Entry.NewName, Entry.OldName);
	}
}
