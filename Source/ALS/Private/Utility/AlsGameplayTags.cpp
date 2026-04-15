#include "Utility/AlsGameplayTags.h"

namespace AlsViewModeTags
{
	UE_DEFINE_GAMEPLAY_TAG(FirstPerson, FName{TEXTVIEW("Als.ViewMode.FirstPerson")})
	UE_DEFINE_GAMEPLAY_TAG(ThirdPerson, FName{TEXTVIEW("Als.ViewMode.ThirdPerson")})
}

namespace AlsLocomotionModeTags
{
	UE_DEFINE_GAMEPLAY_TAG(Grounded, FName{TEXTVIEW("Als.LocomotionMode.Grounded")})
	UE_DEFINE_GAMEPLAY_TAG(InAir, FName{TEXTVIEW("Als.LocomotionMode.InAir")})
}

namespace AlsRotationModeTags
{
	UE_DEFINE_GAMEPLAY_TAG(VelocityDirection, FName{TEXTVIEW("Als.RotationMode.VelocityDirection")})
	UE_DEFINE_GAMEPLAY_TAG(ViewDirection, FName{TEXTVIEW("Als.RotationMode.ViewDirection")})
	UE_DEFINE_GAMEPLAY_TAG(Aiming, FName{TEXTVIEW("Als.RotationMode.Aiming")})
}

namespace AlsStanceTags
{
	UE_DEFINE_GAMEPLAY_TAG(Standing, FName{TEXTVIEW("Als.Stance.Standing")})
	UE_DEFINE_GAMEPLAY_TAG(Crouching, FName{TEXTVIEW("Als.Stance.Crouching")})
}

namespace AlsGaitTags
{
	UE_DEFINE_GAMEPLAY_TAG(Walking, FName{TEXTVIEW("Als.Gait.Walking")})
	UE_DEFINE_GAMEPLAY_TAG(Running, FName{TEXTVIEW("Als.Gait.Running")})
	UE_DEFINE_GAMEPLAY_TAG(Sprinting, FName{TEXTVIEW("Als.Gait.Sprinting")})
}

namespace AlsOverlayModeTags
{
	UE_DEFINE_GAMEPLAY_TAG(Default, FName{TEXTVIEW("Als.OverlayMode.Default")})
	UE_DEFINE_GAMEPLAY_TAG(Masculine, FName{TEXTVIEW("Als.OverlayMode.Masculine")})
	UE_DEFINE_GAMEPLAY_TAG(Feminine, FName{TEXTVIEW("Als.OverlayMode.Feminine")})
	UE_DEFINE_GAMEPLAY_TAG(Injured, FName{TEXTVIEW("Als.OverlayMode.Injured")})
	UE_DEFINE_GAMEPLAY_TAG(HandsTied, FName{TEXTVIEW("Als.OverlayMode.HandsTied")})
	UE_DEFINE_GAMEPLAY_TAG(Rifle, FName{TEXTVIEW("Als.OverlayMode.Rifle")})
	UE_DEFINE_GAMEPLAY_TAG(PistolOneHanded, FName{TEXTVIEW("Als.OverlayMode.PistolOneHanded")})
	UE_DEFINE_GAMEPLAY_TAG(PistolTwoHanded, FName{TEXTVIEW("Als.OverlayMode.PistolTwoHanded")})
	UE_DEFINE_GAMEPLAY_TAG(Bow, FName{TEXTVIEW("Als.OverlayMode.Bow")})
	UE_DEFINE_GAMEPLAY_TAG(Torch, FName{TEXTVIEW("Als.OverlayMode.Torch")})
	UE_DEFINE_GAMEPLAY_TAG(Binoculars, FName{TEXTVIEW("Als.OverlayMode.Binoculars")})
	UE_DEFINE_GAMEPLAY_TAG(Box, FName{TEXTVIEW("Als.OverlayMode.Box")})
	UE_DEFINE_GAMEPLAY_TAG(Barrel, FName{TEXTVIEW("Als.OverlayMode.Barrel")})
}

namespace AlsLocomotionActionTags
{
	UE_DEFINE_GAMEPLAY_TAG(Mantling, FName{TEXTVIEW("Als.LocomotionAction.Mantling")})
	UE_DEFINE_GAMEPLAY_TAG(Ragdolling, FName{TEXTVIEW("Als.LocomotionAction.Ragdolling")})
	UE_DEFINE_GAMEPLAY_TAG(GettingUp, FName{TEXTVIEW("Als.LocomotionAction.GettingUp")})
	UE_DEFINE_GAMEPLAY_TAG(Rolling, FName{TEXTVIEW("Als.LocomotionAction.Rolling")})
}

namespace AlsGroundedEntryModeTags
{
	UE_DEFINE_GAMEPLAY_TAG(FromRoll, FName{TEXTVIEW("Als.GroundedEntryMode.FromRoll")})
}

namespace AlsCurveTags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Curves, "Als.Curves", "ALS 调试曲线白名单根标签。")

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(AllowAiming, "Als.Curves.AllowAiming", "是否启用瞄准层。")
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(AllowTransitions, "Als.Curves.AllowTransitions", "是否允许移动状态下的姿态过渡。")
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(FeetCrossing, "Als.Curves.FeetCrossing", "步态循环混合时的双脚交叉程度。")
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(FootLeftIk, "Als.Curves.FootLeftIk", "左脚 IK 混合权重。")
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(FootLeftLock, "Als.Curves.FootLeftLock", "左脚锁定权重。")
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(FootPlanted, "Als.Curves.FootPlanted", "脚掌着地状态权重。")
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(FootRightIk, "Als.Curves.FootRightIk", "右脚 IK 混合权重。")
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(FootRightLock, "Als.Curves.FootRightLock", "右脚锁定权重。")
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(FootstepSoundBlock, "Als.Curves.FootstepSoundBlock", "阻止脚步声触发。")
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(GroundPredictionBlock, "Als.Curves.GroundPredictionBlock", "阻止空中阶段的落地预测逻辑。")
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(HandLeftIk, "Als.Curves.HandLeftIk", "左手 IK 混合权重。")
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(HandRightIk, "Als.Curves.HandRightIk", "右手 IK 混合权重。")
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(HipsDirectionLock, "Als.Curves.HipsDirectionLock", "瞄准时锁定髋部朝向。")

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(LayerArmLeft, "Als.Curves.LayerArmLeft", "左臂分层权重。")
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(LayerArmLeftAdditive, "Als.Curves.LayerArmLeftAdditive", "左臂加法分层权重。")
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(LayerArmLeftLocalSpace, "Als.Curves.LayerArmLeftLocalSpace", "左臂局部空间加法权重。")
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(LayerArmLeftSlot, "Als.Curves.LayerArmLeftSlot", "左臂插槽混合权重。")
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(LayerArmRight, "Als.Curves.LayerArmRight", "右臂分层权重。")
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(LayerArmRightAdditive, "Als.Curves.LayerArmRightAdditive", "右臂加法分层权重。")
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(LayerArmRightLocalSpace, "Als.Curves.LayerArmRightLocalSpace", "右臂局部空间加法权重。")
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(LayerArmRightSlot, "Als.Curves.LayerArmRightSlot", "右臂插槽混合权重。")
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(LayerHandLeft, "Als.Curves.LayerHandLeft", "左手分层权重。")
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(LayerHandRight, "Als.Curves.LayerHandRight", "右手分层权重。")
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(LayerHead, "Als.Curves.LayerHead", "头部分层权重。")
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(LayerHeadAdditive, "Als.Curves.LayerHeadAdditive", "头部加法分层权重。")
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(LayerHeadSlot, "Als.Curves.LayerHeadSlot", "头部插槽混合权重。")
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(LayerLegs, "Als.Curves.LayerLegs", "腿部分层权重。")
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(LayerLegsSlot, "Als.Curves.LayerLegsSlot", "腿部插槽混合权重。")
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(LayerPelvis, "Als.Curves.LayerPelvis", "骨盆分层权重。")
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(LayerPelvisSlot, "Als.Curves.LayerPelvisSlot", "骨盆插槽混合权重。")
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(LayerSpine, "Als.Curves.LayerSpine", "脊柱分层权重。")
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(LayerSpineAdditive, "Als.Curves.LayerSpineAdditive", "脊柱加法分层权重。")
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(LayerSpineSlot, "Als.Curves.LayerSpineSlot", "脊柱插槽混合权重。")

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(PoseCrouching, "Als.Curves.PoseCrouching", "下蹲姿态权重。")
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(PoseGait, "Als.Curves.PoseGait", "步态姿态权重。")
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(PoseGrounded, "Als.Curves.PoseGrounded", "地面状态姿态权重。")
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(PoseInAir, "Als.Curves.PoseInAir", "空中状态姿态权重。")
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(PoseMoving, "Als.Curves.PoseMoving", "移动状态姿态权重。")
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(PoseStanding, "Als.Curves.PoseStanding", "站立姿态权重。")

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(RotationYawOffset, "Als.Curves.RotationYawOffset", "转身/加法系统使用的偏航偏移量。")
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(RotationYawSpeed, "Als.Curves.RotationYawSpeed", "旋转混合使用的偏航速度。")
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(SprintBlock, "Als.Curves.SprintBlock", "在需要时阻止冲刺状态。")
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(ViewBlock, "Als.Curves.ViewBlock", "阻止由视角驱动的动画效果。")
}
