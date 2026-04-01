#pragma once

#include "AlsTurnInPlaceSettings.generated.h"

class UAnimSequenceBase;

UCLASS(BlueprintType, EditInlineNew)
class ALS_API UAlsTurnInPlaceSettings : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings")
	TObjectPtr<UAnimSequenceBase> Sequence;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings", Meta = (ClampMin = 0, ForceUnits = "x"))
	float PlayRate{1.2f};

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings")
	uint8 bScalePlayRateByAnimatedTurnAngle : 1 {true};

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings", Meta = (ClampMin = 0.0001, ClampMax = 180, ForceUnits = "deg"))
	float AnimatedTurnAngle{0.0f};
};

USTRUCT(BlueprintType)
struct ALS_API FAlsGeneralTurnInPlaceSettings
{
	GENERATED_BODY()

	// 自动原地转身总开关：
	// 仅影响“自动判定触发”的路径（例如根据视角偏差与速度自动发起转身）。
	// 不影响外部手动调用转身接口（手动触发仍可使用转身资源）。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ALS")
	uint8 bAllowAutomaticTurnInPlace : 1 {true};

	// 自动转身的视角偏差角阈值（度）：
	// 当 |ViewYawAngle| 小于等于该值时，不会进入自动转身判定。
	// 同时也参与激活延迟映射的角度区间起点。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ALS",
		Meta = (ClampMin = 0, ClampMax = 180, ForceUnits = "deg"))
	float ViewYawAngleThreshold{45.0f};

	// 自动转身的视角角速度阈值（度/秒）：
	// 当镜头旋转速度高于该值时，自动转身不会触发，
	// 用于避免快速甩镜头时频繁或突兀地进入转身。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ALS",
		Meta = (EditCondition = "bAllowAutomaticTurnInPlace", ClampMin = 0, ForceUnits = "deg/s"))
	float ViewYawSpeedThreshold{50.0f};

	// 自动转身的激活延迟映射（秒）：
	// 根据当前偏差角把“等待多久才触发自动转身”映射到一个范围。
	// 一般角度越大，延迟可设置得越短。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ALS",
		Meta = (EditCondition = "bAllowAutomaticTurnInPlace", ClampMin = 0))
	FVector2f ViewYawAngleToActivationDelay{0.0f, 0.75f};

	// 90/180 转身资源分界角（度）：
	// 当当前偏差角绝对值小于该值时使用 90 度转身资源，
	// 否则使用 180 度转身资源（自动与手动路径都会用到该判定）。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ALS",
		DisplayName = "Turn 180 Angle Threshold",
		Meta = (ClampMin = 0, ClampMax = 180, ForceUnits = "deg"))
	float Turn180AngleThreshold{130.0f};

	// 转身蒙太奇混合时长（秒）：
	// 用于控制转身动画的混入/混出平滑度（自动与手动路径均可能使用）。
	// 值越大过渡越平滑，值越小响应越直接。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ALS",
		Meta = (ClampMin = 0, ForceUnits = "s"))
	float BlendDuration{0.2f};

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ALS", Instanced, DisplayName = "Standing Turn 90 Left")
	TObjectPtr<UAlsTurnInPlaceSettings> StandingTurn90Left;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ALS", Instanced, DisplayName = "Standing Turn 90 Right")
	TObjectPtr<UAlsTurnInPlaceSettings> StandingTurn90Right;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ALS", Instanced, DisplayName = "Standing Turn 180 Left")
	TObjectPtr<UAlsTurnInPlaceSettings> StandingTurn180Left;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ALS", Instanced, DisplayName = "Standing Turn 180 Right")
	TObjectPtr<UAlsTurnInPlaceSettings> StandingTurn180Right;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ALS", Instanced, DisplayName = "Crouching Turn 90 Left")
	TObjectPtr<UAlsTurnInPlaceSettings> CrouchingTurn90Left;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ALS", Instanced, DisplayName = "Crouching Turn 90 Right")
	TObjectPtr<UAlsTurnInPlaceSettings> CrouchingTurn90Right;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ALS", Instanced, DisplayName = "Crouching Turn 180 Left")
	TObjectPtr<UAlsTurnInPlaceSettings> CrouchingTurn180Left;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ALS", Instanced, DisplayName = "Crouching Turn 180 Right")
	TObjectPtr<UAlsTurnInPlaceSettings> CrouchingTurn180Right;
};
