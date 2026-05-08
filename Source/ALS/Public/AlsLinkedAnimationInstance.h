#pragma once

#include "Animation/AnimInstance.h"
#include "State/AlsControlRigInput.h"
#include "AlsLinkedAnimationInstance.generated.h"

class AAlsCharacter;

UCLASS()
class ALS_API UAlsLinkedAnimationInstance : public UAnimInstance
{
	GENERATED_BODY()

protected:
	UPROPERTY(VisibleAnywhere, Category = "State", Transient)
	TWeakObjectPtr<UAlsAnimationInstance> Parent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State", Transient)
	TObjectPtr<AAlsCharacter> Character;

public:
	UAlsLinkedAnimationInstance();

	virtual void NativeInitializeAnimation() override;

	virtual void NativeBeginPlay() override;

protected:
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;

	// Be very careful when using this function to read your custom variables using the property access system. It is
	// safe to use this function to read variables that change only inside UAlsAnimationInstance::NativeUpdateAnimation()
	// because it is guaranteed that this function will be called before parallel animation evaluation. Reading
	// variables that change in other functions can be dangerous because they can be changed in the game thread
	// at the same time as being read in the worker thread, which can lead to undefined behavior or even a crash.
	UFUNCTION(BlueprintPure, Category = "ALS|Linked Animation Instance",
		Meta = (BlueprintThreadSafe, ReturnDisplayName = "Parent"))
	UAlsAnimationInstance* GetParent() const;

	// 透传父级 UAlsAnimationInstance->Settings->General.bUseCustomControlRig。
	// 让 AnimGraph / PropertyAccess 在 LinkedAnimInstance 子类里也能读到该开关，
	// 用于 Blend Poses by bool 切换 ALS 默认 ControlRig 与自定义 ControlRig 路径。
	UFUNCTION(BlueprintPure, Category = "ALS|Linked Animation Instance",
		Meta = (BlueprintThreadSafe, ReturnDisplayName = "Use Custom Control Rig"))
	bool IsUsingCustomControlRig() const;

	// 透传父级 UAlsAnimationInstance::GetControlRigInput()。
	// 让 AnimGraph / PropertyAccess 在 LinkedAnimInstance 子类里也能拿到完整 RigInput
	// （含 SpineYawAngle / Foot IK / PelvisOffsetAmount / VelocityBlend 等所有字段）。
	UFUNCTION(BlueprintPure, Category = "ALS|Linked Animation Instance",
		Meta = (BlueprintThreadSafe, ReturnDisplayName = "Rig Input"))
	FAlsControlRigInput GetControlRigInput() const;

	UFUNCTION(BlueprintCallable, Category = "ALS|Linked Animation Instance", Meta = (BlueprintThreadSafe))
	void InitializeHead();

	UFUNCTION(BlueprintCallable, Category = "ALS|Linked Animation Instance", Meta = (BlueprintThreadSafe))
	void RefreshHead();

	UFUNCTION(BlueprintCallable, Category = "ALS|Linked Animation Instance", Meta = (BlueprintThreadSafe))
	void InitializeLean();

	UFUNCTION(BlueprintCallable, Category = "ALS|Linked Animation Instance", Meta = (BlueprintThreadSafe))
	void InitializeGrounded();

	UFUNCTION(BlueprintCallable, Category = "ALS|Linked Animation Instance", Meta = (BlueprintThreadSafe))
	void RefreshGrounded();

	UFUNCTION(BlueprintCallable, Category = "ALS|Linked Animation Instance", Meta = (BlueprintThreadSafe))
	void ResetGroundedEntryMode();

	UFUNCTION(BlueprintCallable, Category = "ALS|Linked Animation Instance", Meta = (BlueprintThreadSafe))
	void RefreshGroundedMovement();

	UFUNCTION(BlueprintCallable, Category = "ALS|Linked Animation Instance", Meta = (BlueprintThreadSafe))
	void SetHipsDirection(EAlsHipsDirection NewHipsDirection);

	UFUNCTION(BlueprintCallable, Category = "ALS|Linked Animation Instance", Meta = (BlueprintThreadSafe))
	void InitializeStandingMovement();

	UFUNCTION(BlueprintCallable, Category = "ALS|Linked Animation Instance", Meta = (BlueprintThreadSafe))
	void RefreshStandingMovement();

	UFUNCTION(BlueprintCallable, Category = "ALS|Linked Animation Instance", Meta = (BlueprintThreadSafe))
	void ResetPivot();

	UFUNCTION(BlueprintCallable, Category = "ALS|Linked Animation Instance", Meta = (BlueprintThreadSafe))
	void RefreshCrouchingMovement();

	UFUNCTION(BlueprintCallable, Category = "ALS|Linked Animation Instance", Meta = (BlueprintThreadSafe))
	void RefreshInAir();

	UFUNCTION(BlueprintCallable, Category = "ALS|Linked Animation Instance", Meta = (BlueprintThreadSafe))
	void RefreshDynamicTransitions();

	UFUNCTION(BlueprintCallable, Category = "ALS|Linked Animation Instance", Meta = (BlueprintThreadSafe))
	void RefreshRotateInPlace();

	UFUNCTION(BlueprintCallable, Category = "ALS|Linked Animation Instance", Meta = (BlueprintThreadSafe))
	void InitializeTurnInPlace();

	UFUNCTION(BlueprintCallable, Category = "ALS|Linked Animation Instance", Meta = (BlueprintThreadSafe))
	void RefreshTurnInPlace();
};

inline UAlsAnimationInstance* UAlsLinkedAnimationInstance::GetParent() const
{
	return Parent.Get();
}
