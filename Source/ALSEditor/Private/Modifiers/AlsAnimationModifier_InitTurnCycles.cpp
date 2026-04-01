#include "Modifiers/AlsAnimationModifier_InitTurnCycles.h"

#include "Animation/AnimSequence.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AlsAnimationModifier_InitTurnCycles)

void UAlsAnimationModifier_InitTurnCycles::OnApply_Implementation(UAnimSequence* Sequence)
{
	Super::OnApply_Implementation(Sequence);

	const auto* DataModel{Sequence->GetDataModel()};
	const auto NumFrames{Sequence->GetNumberOfSampledKeys()};
	const auto SequenceLength{Sequence->GetPlayLength()};
	const auto FrameRate{Sequence->GetSamplingFrameRate().AsDecimal()};
	const auto AbsRateScale{FMath::Abs(Sequence->RateScale)};

	// --- RotationYawSpeed Curve (always created) ---
	{
		EnsureCurveExists(Sequence, RotationYawSpeedCurveName);

		UAnimationBlueprintLibrary::AddFloatCurveKey(Sequence, RotationYawSpeedCurveName, 0.0f, 0.0f);

		for (auto i{1}; i < NumFrames; i++)
		{
			const auto Time{Sequence->GetTimeAtFrame(i)};

			auto CurrentPoseTransform{
				DataModel->GetBoneTrackTransform(RootBoneName, i + (Sequence->RateScale >= 0.0f ? -1 : 0))
			};

			auto NextPoseTransform{
				DataModel->GetBoneTrackTransform(RootBoneName, i + (Sequence->RateScale >= 0.0f ? 0 : -1))
			};

			const auto DeltaYaw{NextPoseTransform.Rotator().Yaw - CurrentPoseTransform.Rotator().Yaw};
			const auto YawSpeed{DeltaYaw * AbsRateScale * FrameRate};

			UAnimationBlueprintLibrary::AddFloatCurveKey(Sequence, RotationYawSpeedCurveName,
			                                             UE_REAL_TO_FLOAT(Time), UE_REAL_TO_FLOAT(YawSpeed));
		}
	}

	// --- Foot Lock Curves ---
	if (bCreateFootLock)
	{
		CreateFootLockCurve(Sequence, FootLockLeftCurveName, FootLockLeftMap);
		CreateFootLockCurve(Sequence, FootLockRightCurveName, FootLockRightMap);
	}

	// --- Root Offset Curves ---
	if (bCreateTurnRootOffset)
	{
		const auto Direction{Sequence->RateScale < 0.0f ? 1 : -1};

		// Frame curve
		{
			EnsureCurveExists(Sequence, FrameCurveName);

			if (NumFrames > 0)
			{
				const auto FrameRateValue{SequenceLength > SMALL_NUMBER ? static_cast<float>(NumFrames) / SequenceLength : 0.0f};
				UAnimationBlueprintLibrary::AddFloatCurveKey(Sequence, FrameCurveName, 0.0f, UE_REAL_TO_FLOAT(FrameRateValue));
			}
		}

		// BlendWeight curve
		{
			EnsureCurveExists(Sequence, BlendWeightCurveName);

			if (NumFrames > 0)
			{
				UAnimationBlueprintLibrary::AddFloatCurveKey(Sequence, BlendWeightCurveName, 0.0f, 1.0f);
			}
		}

		// RootOffset_X curve
		{
			EnsureCurveExists(Sequence, RootOffsetXCurveName);
			UAnimationBlueprintLibrary::AddFloatCurveKey(Sequence, RootOffsetXCurveName, 0.0f, 0.0f);

			for (auto i{1}; i <= NumFrames; i++)
			{
				const auto Time{Sequence->GetTimeAtFrame(i)};
				const auto CurLoc{DataModel->GetBoneTrackTransform(RootBoneName, i).GetLocation()};
				const auto AdjLoc{DataModel->GetBoneTrackTransform(RootBoneName, i + Direction).GetLocation()};

				const auto DeltaX{CurLoc.X - AdjLoc.X};
				const auto Value{DeltaX * AbsRateScale};

				UAnimationBlueprintLibrary::AddFloatCurveKey(Sequence, RootOffsetXCurveName,
				                                             UE_REAL_TO_FLOAT(Time), UE_REAL_TO_FLOAT(Value));
			}
		}

		// RootOffset_Y curve
		{
			EnsureCurveExists(Sequence, RootOffsetYCurveName);
			UAnimationBlueprintLibrary::AddFloatCurveKey(Sequence, RootOffsetYCurveName, 0.0f, 0.0f);

			for (auto i{1}; i <= NumFrames; i++)
			{
				const auto Time{Sequence->GetTimeAtFrame(i)};
				const auto CurLoc{DataModel->GetBoneTrackTransform(RootBoneName, i).GetLocation()};
				const auto AdjLoc{DataModel->GetBoneTrackTransform(RootBoneName, i + Direction).GetLocation()};

				const auto DeltaY{CurLoc.Y - AdjLoc.Y};
				const auto Value{DeltaY * AbsRateScale};

				UAnimationBlueprintLibrary::AddFloatCurveKey(Sequence, RootOffsetYCurveName,
				                                             UE_REAL_TO_FLOAT(Time), UE_REAL_TO_FLOAT(Value));
			}
		}

	}
}

void UAlsAnimationModifier_InitTurnCycles::CreateFootLockCurve(UAnimSequence* Sequence, const FName& CurveName,
                                                               const TMap<float, bool>& FootLockMap) const
{
	EnsureCurveExists(Sequence, CurveName);

	// Initial key: foot starts locked
	UAnimationBlueprintLibrary::AddFloatCurveKey(Sequence, CurveName, 0.0f, 1.0f);

	for (const auto& [Time, bIsLocked] : FootLockMap)
	{
		UAnimationBlueprintLibrary::AddFloatCurveKey(Sequence, CurveName, Time, bIsLocked ? 1.0f : 0.0f);
	}
}

void UAlsAnimationModifier_InitTurnCycles::EnsureCurveExists(UAnimSequence* Sequence, const FName& CurveName) const
{
	if (UAnimationBlueprintLibrary::DoesCurveExist(Sequence, CurveName, ERawCurveTrackTypes::RCT_Float))
	{
		UAnimationBlueprintLibrary::RemoveCurve(Sequence, CurveName);
	}

	UAnimationBlueprintLibrary::AddCurve(Sequence, CurveName);
}
