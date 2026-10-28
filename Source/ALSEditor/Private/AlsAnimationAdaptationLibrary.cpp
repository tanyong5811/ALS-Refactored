#include "AlsAnimationAdaptationLibrary.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AlsAnimationAdaptationLibrary)

#include "AnimGraphNode_ModifyCurve.h"
#include "AnimGraphNode_Root.h"
#include "AnimGraphNode_Slot.h"
#include "Animation/AnimBlueprint.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Animation/BlendSpace.h"
#include "AnimationModifier.h"
#include "AnimationModifiersAssetUserData.h"
#include "EditorAnimUtils.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "UObject/UnrealType.h"

bool UAlsAnimationAdaptationLibrary::ConfigureStandingLayerWeights(UAnimBlueprint* Blueprint)
{
	if (!Blueprint)
	{
		return false;
	}

	TArray<UEdGraph*> Graphs;
	Blueprint->GetAllGraphs(Graphs);
	for (auto* Graph : Graphs)
	{
		if (Graph->GetFName() != TEXT("AnimGraph"))
		{
			continue;
		}

		for (const auto& Node : Graph->Nodes)
		{
			auto* Root{Cast<UAnimGraphNode_Root>(Node.Get())};
			if (!Root)
			{
				continue;
			}

			auto* Input{Root->FindPin(TEXT("Result"))};
			if (!Input || Input->LinkedTo.Num() != 1)
			{
				return false;
			}

			auto* Source{Input->LinkedTo[0]};
			if (const auto* Existing{Cast<UAnimGraphNode_ModifyCurve>(Source->GetOwningNode())};
				Existing && Existing->NodeComment == TEXT("ALS weapon locomotion layer weights"))
			{
				return true;
			}

			Blueprint->Modify();
			Graph->Modify();
			Root->Modify();
			FGraphNodeCreator<UAnimGraphNode_ModifyCurve> Creator{*Graph};
			auto* Curves{Creator.CreateNode()};
			auto* NodeProperty{FindFProperty<FStructProperty>(Curves->GetClass(), TEXT("Node"))};
			if (!NodeProperty)
			{
				return false;
			}

			auto& CurveNode{*NodeProperty->ContainerPtrToValuePtr<FAnimNode_ModifyCurve>(Curves)};
			CurveNode.CurveNames = {TEXT("LayerHead"), TEXT("LayerArmLeft"), TEXT("LayerArmRight"),
				TEXT("LayerSpine"), TEXT("LayerPelvis"), TEXT("LayerLegs")};
			CurveNode.CurveValues.Init(1.0f, 6);
			CurveNode.ApplyMode = EModifyCurveApplyMode::Blend;
			CurveNode.Alpha = 1.0f;
			Curves->NodeComment = TEXT("ALS weapon locomotion layer weights");
			Curves->NodePosX = Root->NodePosX - 250;
			Curves->NodePosY = Root->NodePosY;
			Creator.Finalize();
			Input->BreakLinkTo(Source);
			Source->MakeLinkTo(Curves->FindPinChecked(TEXT("SourcePose")));
			Curves->FindPinChecked(TEXT("Pose"))->MakeLinkTo(Input);
			FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
			FKismetEditorUtilities::CompileBlueprint(Blueprint);
			return Blueprint->Status == BS_UpToDate;
		}
	}
	return false;
}

void UAlsAnimationAdaptationLibrary::ApplyModifiers(UAnimSequence* Sequence)
{
	if (!Sequence)
	{
		return;
	}
	const auto* Data{Cast<UAnimationModifiersAssetUserData>(
		Sequence->GetAssetUserDataOfClass(UAnimationModifiersAssetUserData::StaticClass()))};
	if (!Data)
	{
		return;
	}
	for (const auto* Modifier : Data->GetAnimationModifierInstances())
	{
		if (Modifier)
		{
			Modifier->ApplyToAnimationSequence(Sequence);
		}
	}
	Sequence->MarkPackageDirty();
}

FString UAlsAnimationAdaptationLibrary::DescribeBlueprint(UAnimBlueprint* Blueprint)
{
	if (!Blueprint)
	{
		return {};
	}
	FString Result;
	if (Blueprint->GeneratedClass)
	{
		auto* Defaults{Blueprint->GeneratedClass->GetDefaultObject()};
		for (TFieldIterator<FStructProperty> It{Blueprint->GeneratedClass}; It; ++It)
		{
			if (It->Struct->GetName().Contains(TEXT("BlendSpacePlayer")) ||
				It->Struct->GetName().Contains(TEXT("SequencePlayer")) ||
				It->Struct->GetName().Contains(TEXT("SequenceEvaluator")))
			{
				FString Value;
				It->ExportText_InContainer(0, Value, Defaults, Defaults, nullptr, PPF_None);
				Result += TEXT("COMPILED ") + It->GetName() + TEXT(" ") + Value + TEXT("\n");
			}
		}
	}
	Result += TEXT("PARENT ") + GetPathNameSafe(Blueprint->ParentClass) + TEXT("\n");
	TArray<UEdGraph*> Graphs;
	Blueprint->GetAllGraphs(Graphs);
	for (const auto* Graph : Graphs)
	{
		Result += TEXT("GRAPH ") + Graph->GetName() + TEXT("\n");
		for (const auto& NodePtr : Graph->Nodes)
		{
			const auto* Node{NodePtr.Get()};
			Result += Node->GetClass()->GetName() + TEXT(" ") + Node->GetName() + TEXT("\n");
			for (TFieldIterator<FProperty> It{Node->GetClass()}; It; ++It)
			{
				if (It->GetName() == TEXT("Node"))
				{
					FString Value;
					It->ExportText_InContainer(0, Value, Node, Node, nullptr, PPF_None);
					Result += Value + TEXT("\n");
				}
			}
		}
	}
	return Result;
}

bool UAlsAnimationAdaptationLibrary::ReplaceBlueprintAnimation(
	UAnimBlueprint* Blueprint, UAnimationAsset* From, UAnimationAsset* To)
{
	if (!Blueprint || !From || !To)
	{
		return false;
	}
	if (From == To)
	{
		return true;
	}
	Blueprint->Modify();
	for (auto& Override : Blueprint->ParentAssetOverrides)
	{
		if (Override.NewAsset == From)
		{
			Override.NewAsset = To;
		}
	}
	TMap<UAnimationAsset*, UAnimationAsset*> Replacements;
	Replacements.Add(From, To);
	EditorAnimUtils::ReplaceReferredAnimationsInBlueprint(Blueprint, Replacements);
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	FKismetEditorUtilities::CompileBlueprint(Blueprint);
	return Blueprint->Status == BS_UpToDate;
}

int32 UAlsAnimationAdaptationLibrary::ReplaceBlueprintSlotName(
	UAnimBlueprint* Blueprint, const FName From, const FName To)
{
	if (!Blueprint || From.IsNone() || To.IsNone())
	{
		return -1;
	}
	int32 Changed{0};
	TArray<UEdGraph*> Graphs;
	Blueprint->GetAllGraphs(Graphs);
	for (auto* Graph : Graphs)
	{
		for (const auto& Node : Graph->Nodes)
		{
			auto* Slot{Cast<UAnimGraphNode_Slot>(Node.Get())};
			if (Slot && Slot->Node.SlotName == From)
			{
				Blueprint->Modify();
				Graph->Modify();
				Slot->Modify();
				Slot->Node.SlotName = To;
				++Changed;
			}
		}
	}
	if (Changed > 0)
	{
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
		FKismetEditorUtilities::CompileBlueprint(Blueprint);
		if (Blueprint->Status != BS_UpToDate)
		{
			return -1;
		}
	}
	return Changed;
}

int32 UAlsAnimationAdaptationLibrary::NormalizeMontageSlots(UAnimMontage* Montage, const FName SlotName)
{
	if (!Montage || SlotName.IsNone())
	{
		return -1;
	}
	int32 Changed{0};
	for (auto& Track : Montage->SlotAnimTracks)
	{
		if (Track.SlotName != SlotName)
		{
			Montage->Modify();
			Track.SlotName = SlotName;
			++Changed;
		}
	}
	if (Changed > 0)
	{
		Montage->PostEditChange();
		Montage->MarkPackageDirty();
	}
	return Changed;
}

void UAlsAnimationAdaptationLibrary::RebuildBlendSpace(UBlendSpace* BlendSpace)
{
	if (!BlendSpace)
	{
		return;
	}
	BlendSpace->Modify();
	BlendSpace->ValidateSampleData();
	BlendSpace->ResampleData();
	BlendSpace->PostEditChange();
	BlendSpace->MarkPackageDirty();
}
