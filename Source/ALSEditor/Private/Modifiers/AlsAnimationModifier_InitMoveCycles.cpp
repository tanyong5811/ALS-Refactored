#include "Modifiers/AlsAnimationModifier_InitMoveCycles.h"

#include "Animation/AnimSequence.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AlsAnimationModifier_InitMoveCycles)

void UAlsAnimationModifier_InitMoveCycles::OnApply_Implementation(UAnimSequence* Sequence)
{
	Super::OnApply_Implementation(Sequence);

	const auto* DataModel{Sequence->GetDataModel()};
	const auto NumFrames{Sequence->GetNumberOfSampledKeys()};
	const auto SequenceLength{Sequence->GetPlayLength()};
	const auto AbsRateScale{FMath::Abs(Sequence->RateScale)};

	const auto WrapTime = [SequenceLength](float Time)
	{
		if (SequenceLength <= UE_SMALL_NUMBER)
		{
			return Time;
		}
		while (Time < 0.0f)
		{
			Time += SequenceLength;
		}
		while (Time > SequenceLength)
		{
			Time -= SequenceLength;
		}
		return Time;
	};
	const float LeftFeetFallTime = WrapTime(LeftFeetDownTime - FMath::Max(0.0f, FootFallLookBackTime));
	const float RightFeetFallTime = WrapTime(RightFeetDownTime - FMath::Max(0.0f, FootFallLookBackTime));

	// Set interpolation to linear
	Sequence->Interpolation = EAnimInterpolationType::Linear;

	// --- MoveSpeed Curve ---
	{
		EnsureCurveExists(Sequence, MoveSpeedCurveName);

		// Get root bone transforms at frame 0 and last frame (with root motion extracted)
		const auto PoseFirst{DataModel->GetBoneTrackTransform(RootBoneName, 0)};
		const auto PoseLast{DataModel->GetBoneTrackTransform(RootBoneName, NumFrames)};

		const auto Forward{PoseFirst.GetRotation().GetForwardVector()};
		const auto Right{PoseFirst.GetRotation().GetRightVector()};

		const auto Displacement{PoseLast.GetLocation() - PoseFirst.GetLocation()};

		const auto ForwardDist{FMath::Abs(FVector::DotProduct(Displacement, Forward))};
		const auto RightDist{FMath::Abs(FVector::DotProduct(Displacement, Right))};

		const auto Distance{FMath::Max(ForwardDist, RightDist)};

		// Speed ~ Distance / (SequenceLength/|RateScale|); faster playback => higher matched MoveSpeed.
		const auto Speed{SequenceLength > SMALL_NUMBER ? Distance * AbsRateScale / SequenceLength : 0.0f};

		UAnimationBlueprintLibrary::AddFloatCurveKey(Sequence, MoveSpeedCurveName, 0.0f, UE_REAL_TO_FLOAT(Speed));
	}

	// --- Weight Gait Curve ---
	{
		EnsureCurveExists(Sequence, WeightGaitCurveName);
		UAnimationBlueprintLibrary::AddFloatCurveKey(Sequence, WeightGaitCurveName, 0.0f, WeightGaitValue);
	}

	// --- Feet Position Curve + Sync Markers ---
	{
		EnsureCurveExists(Sequence, FeetPositionCurveName);

		// Manage foot sync notify track
		if (UAnimationBlueprintLibrary::IsValidAnimNotifyTrackName(Sequence, FeetTrackChannelName))
		{
			UAnimationBlueprintLibrary::RemoveAnimationNotifyTrack(Sequence, FeetTrackChannelName);
		}
		UAnimationBlueprintLibrary::AddAnimationNotifyTrack(Sequence, FeetTrackChannelName, FLinearColor::White);

		// Add sync markers
		UAnimationBlueprintLibrary::AddAnimationSyncMarker(Sequence, FName{TEXT("Left")}, LeftFeetDownTime, FeetTrackChannelName);
		UAnimationBlueprintLibrary::AddAnimationSyncMarker(Sequence, FName{TEXT("Right")}, RightFeetDownTime, FeetTrackChannelName);

		// Add feet position curve keys.
		// Requested mapping:
		// - fully planted time -> -1 (left), +1 (right)
		// - look-back time     -> -LookBack (left), +LookBack (right)
		const float FootFallCurveValue = FMath::Clamp(FootFallLookBackTime, 0.0f, 1.0f);

		// Loop seam: linear segment from last key time (in cycle order) across t=SequenceLength to first key time (t=0).
		// Without an explicit key at t=0, the engine extrapolates before the first internal key and frame 0 can match
		// the value at the first key (e.g. same as RightFeetFallTime) instead of matching the last frame.
		struct FFootKey
		{
			float Time;
			float Value;
		};

		TArray<FFootKey> FootKeys;
		FootKeys.Reserve(4);
		FootKeys.Add({RightFeetFallTime, FootFallCurveValue});
		FootKeys.Add({RightFeetDownTime, 1.0f});
		FootKeys.Add({LeftFeetFallTime, -FootFallCurveValue});
		FootKeys.Add({LeftFeetDownTime, -1.0f});

		FootKeys.Sort([](const FFootKey& A, const FFootKey& B)
		{
			return A.Time < B.Time;
		});

		// Loop seam: the last planted foot stays planted through the end of the cycle and
		// back to the start until the next fall event. So both t=0 and t=SequenceLength must
		// hold the last key's value (no linear interpolation across the wrap boundary).
		const float LoopBoundaryValue{FootKeys.Last().Value};

		UAnimationBlueprintLibrary::AddFloatCurveKey(Sequence, FeetPositionCurveName, 0.0f, LoopBoundaryValue);

		for (const auto& Key : FootKeys)
		{
			UAnimationBlueprintLibrary::AddFloatCurveKey(Sequence, FeetPositionCurveName,
			                                             UE_REAL_TO_FLOAT(Key.Time), UE_REAL_TO_FLOAT(Key.Value));
		}

		UAnimationBlueprintLibrary::AddFloatCurveKey(Sequence, FeetPositionCurveName,
		                                             UE_REAL_TO_FLOAT(SequenceLength), LoopBoundaryValue);

		UAnimationBlueprintLibrary::FinalizeBoneAnimation(Sequence);
	}

	// --- Root Offset Curves ---
	if (bCreateRootOffset)
	{
		// Frame curve
		{
			EnsureCurveExists(Sequence, FrameCurveName);

			if (NumFrames > 0)
			{
				const auto FrameRate{SequenceLength > SMALL_NUMBER ? static_cast<float>(NumFrames) / SequenceLength : 0.0f};
				UAnimationBlueprintLibrary::AddFloatCurveKey(Sequence, FrameCurveName, 0.0f, UE_REAL_TO_FLOAT(FrameRate));
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

		const auto Direction{Sequence->RateScale < 0.0f ? 1 : -1};

		// RootOffset_X curve
		if (bCreateRootOffsetX)
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
		if (bCreateRootOffsetY)
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

void UAlsAnimationModifier_InitMoveCycles::EnsureCurveExists(UAnimSequence* Sequence, const FName& CurveName) const
{
	if (UAnimationBlueprintLibrary::DoesCurveExist(Sequence, CurveName, ERawCurveTrackTypes::RCT_Float))
	{
		UAnimationBlueprintLibrary::RemoveCurve(Sequence, CurveName);
	}

	UAnimationBlueprintLibrary::AddCurve(Sequence, CurveName);
}
