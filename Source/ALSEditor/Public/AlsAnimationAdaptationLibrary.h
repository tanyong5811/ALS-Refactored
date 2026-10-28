#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "AlsAnimationAdaptationLibrary.generated.h"

class UAnimationAsset;
class UAnimBlueprint;
class UAnimMontage;
class UAnimSequence;
class UBlendSpace;

/** Portable editor operations used by the ALS animation-adaptation tools. */
UCLASS()
class ALSEDITOR_API UAlsAnimationAdaptationLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "ALS|Animation Adaptation")
	static bool ConfigureStandingLayerWeights(UAnimBlueprint* Blueprint);

	UFUNCTION(BlueprintCallable, Category = "ALS|Animation Adaptation")
	static void ApplyModifiers(UAnimSequence* Sequence);

	UFUNCTION(BlueprintCallable, Category = "ALS|Animation Adaptation")
	static FString DescribeBlueprint(UAnimBlueprint* Blueprint);

	UFUNCTION(BlueprintCallable, Category = "ALS|Animation Adaptation")
	static bool ReplaceBlueprintAnimation(UAnimBlueprint* Blueprint, UAnimationAsset* From, UAnimationAsset* To);

	/** Replace matching Slot nodes and compile the animation Blueprint. Returns the number changed, or -1 on compile failure. */
	UFUNCTION(BlueprintCallable, Category = "ALS|Animation Adaptation")
	static int32 ReplaceBlueprintSlotName(UAnimBlueprint* Blueprint, FName From, FName To);

	/** Set every track in a montage to the canonical slot. Returns the number changed. */
	UFUNCTION(BlueprintCallable, Category = "ALS|Animation Adaptation")
	static int32 NormalizeMontageSlots(UAnimMontage* Montage, FName SlotName);

	UFUNCTION(BlueprintCallable, Category = "ALS|Animation Adaptation")
	static void RebuildBlendSpace(UBlendSpace* BlendSpace);
};
