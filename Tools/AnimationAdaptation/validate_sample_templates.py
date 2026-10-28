"""Validate that plugin sample templates are complete and project-independent."""
import json
from pathlib import Path
import unreal

ROOT = "/ALS/ALSSampleTemplate"
LIB = unreal.AnimationLibrary
EAL = unreal.EditorAssetLibrary

POSE_SOURCES = {
    ROOT + "/Poses/Idle/A_Als_Template_Idle_Pose": ROOT + "/Animations/Idle/A_Als_Template_Idle",
}
POSE_SOURCES.update({
    ROOT + "/Poses/Locomotion/A_Als_Template_" + gait + direction + "_Pose":
        ROOT + "/Animations/Locomotion/A_Als_Template_" + gait + direction
    for direction in ("F", "B", "L", "R") for gait in ("Walk", "Run")
})


def transform_matches(a, b, tolerance=1e-4):
    def vector_matches(x, y):
        return (abs(x.x - y.x) <= tolerance and abs(x.y - y.y) <= tolerance and
                abs(x.z - y.z) <= tolerance)
    rotation_dot = abs(a.rotation.x * b.rotation.x + a.rotation.y * b.rotation.y +
                       a.rotation.z * b.rotation.z + a.rotation.w * b.rotation.w)
    return (vector_matches(a.translation, b.translation) and
            vector_matches(a.scale3d, b.scale3d) and abs(1.0 - rotation_dot) <= tolerance)


def is_frozen_at_first_frame(sequence):
    frame_count = LIB.get_num_frames(sequence)
    sample_frames = sorted(set((0, frame_count // 2, max(0, frame_count - 1))))
    for bone in LIB.get_animation_track_names(sequence):
        first = LIB.get_bone_pose_for_frame(sequence, bone, 0, False)
        for frame in sample_frames[1:]:
            if not transform_matches(first, LIB.get_bone_pose_for_frame(sequence, bone, frame, False)):
                return False
    return True
POSE_SOURCES.update({
    ROOT + "/Poses/Turn/A_Als_Template_Turn" + side + "_" + angle + "_Pose":
        ROOT + "/Animations/Turn/A_Als_Template_Turn" + side + "_" + angle
    for side in ("L", "R") for angle in ("90", "180")
})

REQUIRED = [
    ROOT + "/Animations/Idle/A_Als_Template_Idle",
    ROOT + "/Animations/Idle/A_Als_Template_Idle_Overlay",
    ROOT + "/Poses/Idle/A_Als_Template_Idle_Pose",
    ROOT + "/Blueprints/AB_Als_Standing_TemplateSource",
    ROOT + "/Blueprints/AB_Als_BasePose_TemplateSource",
    ROOT + "/Blueprints/AB_Als_Overlay_TemplateSource",
    ROOT + "/Settings/AIS_Als_Template",
    ROOT + "/Retargeting/IK_Als_Target",
    ROOT + "/Maps/L_AlsAnimationLab",
] + [ROOT + "/Animations/Locomotion/A_Als_Template_" + gait + direction
     for direction in ("F", "B", "L", "R") for gait in ("Walk", "Run")] + [
    ROOT + "/Poses/Locomotion/A_Als_Template_" + gait + direction + "_Pose"
    for direction in ("F", "B", "L", "R") for gait in ("Walk", "Run")] + [
    ROOT + "/BlendSpaces/Locomotion/BS_Als_Template_Move" + direction
    for direction in ("F", "B", "L", "R")] + [
    ROOT + "/Animations/Turn/A_Als_Template_Turn" + side + "_" + angle
    for side in ("L", "R") for angle in ("90", "180")] + [
    ROOT + "/Poses/Turn/A_Als_Template_Turn" + side + "_" + angle + "_Pose"
    for side in ("L", "R") for angle in ("90", "180")] + [
    ROOT + "/BlendSpaces/Turn/BS_Als_Template_Turn" + side + "_" + angle
    for side in ("L", "R") for angle in ("90", "180")]


def main():
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    registry.search_all_assets(True)
    errors = []
    missing = [path for path in REQUIRED if not unreal.load_asset(path)]
    if missing:
        errors.append("Missing assets: " + repr(missing))
    for path, source_path in POSE_SOURCES.items():
        pose = unreal.load_asset(path)
        source = unreal.load_asset(source_path)
        if pose and source and (LIB.get_num_frames(pose) != LIB.get_num_frames(source) or
                                LIB.get_animation_curve_names(pose, unreal.RawCurveTrackTypes.RCT_FLOAT)):
            errors.append("Invalid full-length frozen pose: " + path)
        elif pose and not is_frozen_at_first_frame(pose):
            errors.append("Pose is not frozen at its first frame: " + path)
    overlay = unreal.load_asset(ROOT + "/Animations/Idle/A_Als_Template_Idle_Overlay")
    if overlay and (LIB.get_num_frames(overlay) <= 1 or
                    LIB.get_animation_curve_names(overlay, unreal.RawCurveTrackTypes.RCT_FLOAT)):
        errors.append("Overlay Idle must be full-length and curve-free")
    for path in REQUIRED:
        asset = unreal.load_asset(path)
        if isinstance(asset, unreal.AnimBlueprint):
            unreal.BlueprintEditorLibrary.compile_blueprint(asset)
            if asset.get_editor_property("status") != unreal.BlueprintStatus.BS_UP_TO_DATE:
                errors.append("Blueprint compile failed: " + path)
    options = unreal.AssetRegistryDependencyOptions(include_hard_package_references=True,
                                                     include_soft_package_references=True)
    forbidden = {}
    for record in registry.get_assets_by_path(ROOT, recursive=True):
        package = str(record.package_name)
        bad = sorted(str(x) for x in (registry.get_dependencies(package, options) or [])
                     if str(x).startswith("/Game/"))
        if bad:
            forbidden[package] = bad
    if forbidden:
        errors.append("Plugin templates reference project content")
    result = {"required": len(REQUIRED), "missing": missing,
              "project_dependencies": forbidden, "errors": errors}
    report = Path(unreal.Paths.project_saved_dir()) / "ALSAnimationAdaptation" / "sample-template-validation.json"
    report.parent.mkdir(parents=True, exist_ok=True)
    report.write_text(json.dumps(result, indent=2), encoding="utf-8")
    if errors:
        raise RuntimeError("; ".join(errors))
    unreal.log("ALS_SAMPLE_TEMPLATE_VALIDATION_OK")


if __name__ == "__main__":
    main()
