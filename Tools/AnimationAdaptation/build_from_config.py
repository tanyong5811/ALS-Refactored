"""Build an ALS weapon locomotion profile from a JSON contract.

The marketplace directory is read-only. Generated assets live under the project's
animation tree and are reused incrementally. Run with UE 5.8 PythonScriptCommandlet.
"""
import json
import math
import os
import pathlib
import re
import shutil
import sys
import unreal

PROJECT = pathlib.Path(unreal.Paths.project_dir()).resolve()

def _config_path():
    prefix = "-AlsAdaptationConfig="
    value = next((arg[len(prefix):] for arg in sys.argv if arg.startswith(prefix)), None)
    if not value:
        command_line = unreal.SystemLibrary.get_command_line()
        match = re.search(r'(?:^|\s)-AlsAdaptationConfig=(?:"([^"]+)"|([^\s]+))', command_line)
        value = (match.group(1) or match.group(2)) if match else None
    value = value or os.environ.get("ALS_ADAPTATION_CONFIG")
    if not value:
        raise RuntimeError("Missing -AlsAdaptationConfig=<absolute-json-path>")
    path = pathlib.Path(value).resolve()
    if not path.is_file():
        raise RuntimeError("Adaptation config not found: " + str(path))
    return path

CONFIG_PATH = _config_path()
CONFIG = json.loads(CONFIG_PATH.read_text(encoding="utf-8-sig"))
if CONFIG.get("schema_version") != 1:
    raise RuntimeError("Unsupported schema_version; expected 1")
WEAPON = CONFIG["weapon"]
REPORT = PROJECT / CONFIG.get("report_directory", "Saved/ALSAnimationAdaptation/" + WEAPON)
OUT = CONFIG["output_root"].rstrip("/")
SOURCE = CONFIG["source_root"].rstrip("/")
SOURCE_MESH = CONFIG["source_mesh"]
TARGET_MESH = CONFIG.get("target_mesh", "/ALS/ALS/Character/SKM_Als")
RETARGET_CONFIG = CONFIG.get("retargeter", {})
RETARGETER = RETARGET_CONFIG.get("output")
RETARGETER_TEMPLATE = RETARGET_CONFIG.get("template")
if not RETARGETER_TEMPLATE:
    raise RuntimeError("retargeter.template is required because the source skeleton is pack-specific")
DEFAULT_TEMPLATES = {
    "standing_blueprint": "/ALS/ALSSampleTemplate/Blueprints/AB_Als_Standing_TemplateSource",
    "standing_idle_source": "/ALS/ALSSampleTemplate/Animations/Idle/A_Als_Template_Idle",
    "basepose_blueprint": "/ALS/ALSSampleTemplate/Blueprints/AB_Als_BasePose_TemplateSource",
    "base_pose_idle_source": "/ALS/ALSSampleTemplate/Poses/Idle/A_Als_Template_Idle_Pose",
    "overlay_blueprint": "/ALS/ALSSampleTemplate/Blueprints/AB_Als_Overlay_TemplateSource",
    "overlay_idle_source": "/ALS/ALSSampleTemplate/Animations/Idle/A_Als_Template_Idle_Overlay",
    "settings": "/ALS/ALSSampleTemplate/Settings/AIS_Als_Template",
    "move_blendspaces": {key: "/ALS/ALSSampleTemplate/BlendSpaces/Locomotion/BS_Als_Template_Move" + key
                         for key in ("F", "B", "L", "R")},
    "turn_blendspaces": {key: "/ALS/ALSSampleTemplate/BlendSpaces/Turn/BS_Als_Template_Turn" + key[0] + "_" + key[1:]
                         for key in ("L90", "R90", "L180", "R180")},
}
TEMPLATES = DEFAULT_TEMPLATES
TEMPLATES.update(CONFIG.get("templates", {}))
PROFILE = CONFIG.get("profile")
TABLE = PROFILE.get("data_table") if PROFILE else None
PROFILE_NAME = PROFILE.get("row_name") if PROFILE else WEAPON
PROFILE_TEMPLATE_ROW = PROFILE.get("template_row") if PROFILE else None
LIB = unreal.AnimationLibrary
EAL = unreal.EditorAssetLibrary
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
HELPER = unreal.AlsAnimationAdaptationLibrary

IDLE = CONFIG["animations"]["idle"]
MOVES = {key: tuple(CONFIG["animations"]["moves"][key]) for key in ("F", "B", "L", "R")}
TURN_MAP = CONFIG["animations"]["turns"]
TURNS = [TURN_MAP[key] for key in ("L90", "R90", "L180", "R180")]
NAMES = [IDLE] + [name for pair in MOVES.values() for name in pair] + TURNS
RUN_NAMES = {pair[1] for pair in MOVES.values()}
STANDING_IDLE_CURVES = {
    "LayerHead": 1.0, "LayerHeadAdditive": 0.0,
    "LayerArmLeft": 1.0, "LayerArmLeftAdditive": 0.0, "LayerArmLeftLocalSpace": 0.0,
    "LayerArmRight": 1.0, "LayerArmRightAdditive": 0.0, "LayerArmRightLocalSpace": 0.0,
    "LayerSpine": 1.0, "LayerSpineAdditive": 0.0,
    "LayerPelvis": 1.0, "LayerLegs": 1.0,
}


def require(path):
    asset = unreal.load_asset(path)
    if not asset:
        raise RuntimeError("Missing asset: " + path)
    return asset


def save(asset):
    if not EAL.save_loaded_asset(asset, only_if_is_dirty=False):
        raise RuntimeError("Save failed: " + asset.get_path_name())


def duplicate(source, destination):
    if EAL.does_asset_exist(destination):
        return require(destination)
    folder, name = destination.rsplit("/", 1)
    asset = TOOLS.duplicate_asset(name, folder, require(source) if isinstance(source, str) else source)
    if not asset:
        raise RuntimeError("Duplicate failed: " + destination)
    save(asset)
    return asset


def backup_file(relative_path):
    source = PROJECT / "Content" / relative_path
    if source.exists():
        destination = REPORT / "Backup" / relative_path
        destination.parent.mkdir(parents=True, exist_ok=True)
        if not destination.exists():
            shutil.copy2(source, destination)


def setup_retargeter():
    return duplicate(RETARGETER_TEMPLATE, RETARGETER) if RETARGETER else require(RETARGETER_TEMPLATE)


def retarget():
    missing = [name for name in NAMES if not EAL.does_asset_exist(OUT + "/Animations/" + name)]
    if missing:
        inputs = unreal.IKRetargetBatchOperationInputs(
            assets_to_retarget=[EAL.find_asset_data(SOURCE + "/" + name) for name in missing],
            source_mesh=require(SOURCE_MESH), target_mesh=require(TARGET_MESH),
            ik_retarget_asset=setup_retargeter(), target_path=OUT + "/Animations",
            use_source_path=False, include_referenced_assets=False, overwrite_existing_files=False)
        results = unreal.IKRetargetBatchOperation.run_batch_retarget(inputs)
        if len(results) != len(missing):
            raise RuntimeError("Incomplete retarget batch: expected %d, got %d" % (len(missing), len(results)))
        for result in results:
            save(result.get_asset())
    return {name: require(OUT + "/Animations/" + name) for name in NAMES}


def world_point(sequence, frame, side, bone):
    chain = ["root", "pelvis", "thigh_" + side, "calf_" + side, "foot_" + side]
    if bone == "ball":
        chain.append("ball_" + side)
    poses = LIB.get_bone_poses_for_frame(sequence, chain, frame, False)
    value = poses[0]
    for pose in poses[1:]:
        value = pose * value
    p = value.translation
    return [p.x, p.y, p.z]


def sample_sequence(sequence):
    count = LIB.get_num_frames(sequence)
    result = {"frames": count, "length": sequence.get_play_length(),
              "rate_scale": sequence.get_editor_property("rate_scale"),
              "root": [], "feet": {"l": [], "r": []}}
    for frame in range(count):
        root = LIB.get_bone_pose_for_frame(sequence, "root", frame, False).translation
        result["root"].append([root.x, root.y, root.z])
        for side in ("l", "r"):
            result["feet"][side].append({"foot": world_point(sequence, frame, side, "foot"),
                                         "ball": world_point(sequence, frame, side, "ball")})
    return result


def circular_delta(a, b, count):
    d = abs(a - b)
    return min(d, count - d)


def detect_contact(sample, side):
    """Find entry into the longest low, slow support interval using foot and toe."""
    points = sample["feet"][side]
    roots = sample["root"]
    count = len(points)
    z = [(p["foot"][2] + p["ball"][2]) * 0.5 for p in points]
    low = min(z) + max(1.0, (max(z) - min(z)) * 0.16)
    speeds = []
    for i in range(count):
        j = (i - 1) % count
        # Remove character root travel before measuring support-foot motion.
        dx = (points[i]["ball"][0] - roots[i][0]) - (points[j]["ball"][0] - roots[j][0])
        dy = (points[i]["ball"][1] - roots[i][1]) - (points[j]["ball"][1] - roots[j][1])
        dz = points[i]["ball"][2] - points[j]["ball"][2]
        speeds.append(math.sqrt(dx * dx + dy * dy + dz * dz))
    ordered = sorted(speeds)
    slow = ordered[max(0, min(count - 1, int(count * 0.45)))] * 1.35 + 0.01
    stable = [z[i] <= low and speeds[i] <= slow for i in range(count)]
    runs = []
    doubled = stable + stable
    start = None
    for i, value in enumerate(doubled):
        if value and start is None:
            start = i
        if start is not None and (not value or i == len(doubled) - 1):
            end = i if not value else i + 1
            if start < count:
                runs.append((start % count, min(end - start, count)))
            start = None
    if not runs:
        raise RuntimeError("No grounded support interval for " + side)
    contact, run_length = max(runs, key=lambda pair: pair[1])
    confidence = min(1.0, run_length / max(1.0, count * 0.18))
    return contact, confidence, {"low_threshold": low, "slow_threshold": slow, "support_frames": run_length}


def root_offset_options(sequence, is_turn=False):
    roots = [LIB.get_bone_pose_for_frame(sequence, "root", i, False).translation
             for i in range(LIB.get_num_frames(sequence))]
    ranges = {axis: max(getattr(p, axis) for p in roots) - min(getattr(p, axis) for p in roots)
              for axis in ("x", "y")}
    if is_turn:
        return {"create_turn_root_offset": max(ranges.values()) > 0.1}, ranges
    forward = sequence.get_name() in MOVES["F"] + MOVES["B"]
    use_x = forward and ranges["x"] > 0.1
    use_y = not forward and ranges["y"] > 0.1
    return {"create_root_offset": use_x or use_y,
            "create_root_offset_x": use_x, "create_root_offset_y": use_y}, ranges


def modifier(sequence, cls, properties):
    data = sequence.get_asset_user_data_of_class(unreal.AnimationModifiersAssetUserData)
    instances = list(data.animation_modifier_instances) if data else []
    matches = [item for item in instances if isinstance(item, cls)]
    if not matches:
        if not sequence.add_animation_modifier_of_class(cls):
            raise RuntimeError("Cannot add modifier: " + sequence.get_name())
        data = sequence.get_asset_user_data_of_class(unreal.AnimationModifiersAssetUserData)
        matches = [item for item in data.animation_modifier_instances if isinstance(item, cls)]
    if len(matches) != 1:
        raise RuntimeError("Duplicate/missing modifier: " + sequence.get_name())
    matches[0].set_editor_properties(properties)
    HELPER.apply_modifiers(sequence)


def trim_curves(sequence, allowed):
    allowed = {name.lower() for name in allowed}
    for kind in (unreal.RawCurveTrackTypes.RCT_FLOAT, unreal.RawCurveTrackTypes.RCT_TRANSFORM):
        for name in LIB.get_animation_curve_names(sequence, kind):
            if kind != unreal.RawCurveTrackTypes.RCT_FLOAT or str(name).lower() not in allowed:
                LIB.remove_curve(sequence, name, False)


def set_constant_curve(sequence, name, value):
    if LIB.does_curve_exist(sequence, name, unreal.RawCurveTrackTypes.RCT_FLOAT):
        LIB.remove_curve(sequence, name, False)
    LIB.add_curve(sequence, name)
    LIB.add_float_curve_key(sequence, name, 0.0, value)


def make_overlay_idle(idle):
    """Full looping Idle for Overlay; never substitute the frozen BasePose."""
    overlay_idle = duplicate(idle, OUT + "/Animations/" + WEAPON + "_Idle_Overlay")
    LIB.remove_all_curve_data(overlay_idle)
    overlay_idle.set_editor_properties({"enable_root_motion": False, "force_root_lock": True})
    save(overlay_idle)
    return overlay_idle


def make_pose(sequence):
    destination = OUT + "/Poses/" + sequence.get_name() + "_Pose"
    pose = duplicate(sequence, destination)
    controller = pose.controller
    controller.open_bracket("ALS full-length first-frame frozen pose", False)
    try:
        frame_count = LIB.get_num_frames(sequence)
        controller.set_number_of_frames(unreal.FrameNumber(frame_count), False)
        for bone in LIB.get_animation_track_names(sequence):
            value = LIB.get_bone_pose_for_frame(sequence, bone, 0, False)
            translations = [value.translation for _ in range(frame_count)]
            rotations = [value.rotation for _ in range(frame_count)]
            scales = [value.scale3d for _ in range(frame_count)]
            if not controller.set_bone_track_keys(bone, translations, rotations, scales, False):
                raise RuntimeError("Pose track write failed: " + str(bone))
        LIB.remove_all_curve_data(pose)
        LIB.remove_all_animation_notify_tracks(pose)
        pose.set_editor_property("asset_user_data", [])
        pose.set_editor_properties({"enable_root_motion": False, "force_root_lock": True})
    finally:
        controller.close_bracket(False)
    save(pose)
    return pose


def prepare_sequences(sequences):
    audit = {}
    poses = {}
    overlay_idle = make_overlay_idle(sequences[IDLE])
    for name, sequence in sequences.items():
        sequence.set_editor_property("rate_scale", 1.0)
        sample = sample_sequence(sequence)
        entry = {"sample": sample}
        if name in [item for pair in MOVES.values() for item in pair]:
            left, lc, ld = detect_contact(sample, "l")
            right, rc, rd = detect_contact(sample, "r")
            separation = circular_delta(left, right, sample["frames"]) / max(1, sample["frames"])
            if min(lc, rc) < 0.55 or separation < 0.20:
                raise RuntimeError("Low-confidence contact detection for %s: L=%s R=%s sep=%.3f" %
                                   (name, (left, lc), (right, rc), separation))
            options, ranges = root_offset_options(sequence)
            properties = dict(options, weight_gait_value=2.0 if name in RUN_NAMES else 1.0,
                              left_feet_down_time=LIB.get_time_at_frame(sequence, left),
                              right_feet_down_time=LIB.get_time_at_frame(sequence, right),
                              foot_fall_look_back_time=0.12)
            modifier(sequence, unreal.AlsAnimationModifier_InitMoveCycles, properties)
            allowed = {"MoveSpeed", "PoseGait", "FootPlanted", "Frame", "BlendWeight",
                       "MoveAmount_X", "MoveAmount_Y"} if options["create_root_offset"] else {
                       "MoveSpeed", "PoseGait", "FootPlanted"}
            trim_curves(sequence, allowed)
            entry.update({"left_contact_frame": left, "right_contact_frame": right,
                          "left_confidence": lc, "right_confidence": rc,
                          "left_detection": ld, "right_detection": rd,
                          "contact_separation": separation, "root_ranges": ranges,
                          "modifier": properties})
        elif name in TURNS:
            options, ranges = root_offset_options(sequence, True)
            properties = dict(options, create_foot_lock=False,
                              foot_lock_left_map={}, foot_lock_right_map={})
            modifier(sequence, unreal.AlsAnimationModifier_InitTurnCycles, properties)
            allowed = {"RotationYawSpeed"}
            if options["create_turn_root_offset"]:
                allowed.update(("Frame", "BlendWeight", "MoveAmount_X", "MoveAmount_Y"))
            trim_curves(sequence, allowed)
            entry.update({"root_ranges": ranges, "modifier": properties})
        else:
            trim_curves(sequence, set())
            for curve_name, curve_value in STANDING_IDLE_CURVES.items():
                set_constant_curve(sequence, curve_name, curve_value)
        sequence.set_editor_properties({"enable_root_motion": name not in TURNS,
                                        "force_root_lock": True})
        save(sequence)
        poses[name] = make_pose(sequence)
        audit[name] = entry
    (REPORT / "input-and-contact-audit.json").write_text(json.dumps(audit, indent=2), encoding="utf-8")
    return poses, overlay_idle


def make_blendspaces(sequences, poses):
    result = {}
    for direction, (walk, run) in MOVES.items():
        blend = duplicate(TEMPLATES["move_blendspaces"][direction],
                          OUT + "/BlendSpaces/BS_" + WEAPON + "_Move" + direction)
        samples = []
        for animation, x, y in ((poses[walk], 0, 0), (poses[run], 0, 1),
                                (sequences[walk], 1, 0), (sequences[run], 1, 1)):
            value = unreal.BlendSample()
            value.set_editor_properties({"animation": animation,
                                         "sample_value": unreal.Vector(x, y, 0)})
            samples.append(value)
        blend.set_editor_property("sample_data", samples)
        HELPER.rebuild_blend_space(blend)
        save(blend)
        result["Move" + direction] = blend
    for name in TURNS:
        turn_key = next(key for key, value in TURN_MAP.items() if value == name)
        old_suffix = "Turn" + turn_key[0] + "_" + turn_key[1:]
        angle = int(name[-3:]) if name.endswith("180") else 90
        blend = duplicate(TEMPLATES["turn_blendspaces"][turn_key],
                          OUT + "/BlendSpaces/BS_" + WEAPON + "_" + old_suffix)
        samples = []
        for animation, x in ((poses[name], 0), (sequences[name], angle)):
            value = unreal.BlendSample()
            value.set_editor_properties({"animation": animation,
                                         "sample_value": unreal.Vector(x, 0, 0)})
            samples.append(value)
        blend.set_editor_property("sample_data", samples)
        HELPER.rebuild_blend_space(blend)
        save(blend)
        result[old_suffix] = blend
    return result


def build_layers(sequences, poses, blends, overlay_idle):
    result = {}
    idle_pose = poses[IDLE]
    for layer in ("Standing", "BasePose", "Overlay"):
        destination = OUT + "/Blueprints/AB_Als_" + layer + "_" + WEAPON
        if layer == "Standing":
            blueprint = duplicate(TEMPLATES["standing_blueprint"], destination)
        else:
            blueprint = duplicate(TEMPLATES[layer.lower() + "_blueprint"], destination)
        replacements = []
        if layer == "Standing":
            if not HELPER.configure_standing_layer_weights(blueprint):
                raise RuntimeError("Standing layer-weight setup failed")
            for suffix, direction in (("Forward", "F"), ("Backward", "B"),
                                      ("Left_Forward", "L"), ("Left_Backward", "L"),
                                      ("Right_Forward", "R"), ("Right_Backward", "R")):
                HELPER.replace_blueprint_animation(blueprint,
                    require(DEFAULT_TEMPLATES["move_blendspaces"][direction]), blends["Move" + direction])
                if not HELPER.replace_blueprint_animation(blueprint,
                    require("/ALS/ALS/Animations/Grounded/WalkRun/BS_Als_WalkRun_" + suffix),
                    blends["Move" + direction]):
                    raise RuntimeError("Move template replacement failed: " + suffix)
            for side, name in (("Left", TURN_MAP["L90"]), ("Right", TURN_MAP["R90"])):
                template_turn = "/ALS/ALSSampleTemplate/Animations/Turn/A_Als_Template_Turn" + (
                    "L_90" if side == "Left" else "R_90")
                HELPER.replace_blueprint_animation(blueprint, require(template_turn), sequences[name])
                if not HELPER.replace_blueprint_animation(blueprint,
                    require("/ALS/ALS/Animations/RotateInPlace/A_Als_Rotate_90_" + side), sequences[name]):
                    raise RuntimeError("Rotate template replacement failed: " + side)
            HELPER.replace_blueprint_animation(blueprint,
                require(TEMPLATES.get("standing_idle_source", "/ALS/ALS/Animations/Base/A_Als_Stand_Pose")), sequences[IDLE])
            replacements = []
        elif layer == "BasePose":
            replacements = [(TEMPLATES["base_pose_idle_source"], idle_pose)]
        else:
            replacements = [(TEMPLATES["overlay_idle_source"], overlay_idle)]
        for old, new in replacements:
            if not HELPER.replace_blueprint_animation(blueprint, require(old), new):
                raise RuntimeError("Blueprint replacement failed: " + old)
        unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
        if blueprint.get_editor_property("status") != unreal.BlueprintStatus.BS_UP_TO_DATE:
            raise RuntimeError("Blueprint compile failed: " + blueprint.get_path_name())
        save(blueprint)
        result[layer] = blueprint
        (REPORT / (layer + "-compiled.txt")).write_text(HELPER.describe_blueprint(blueprint), encoding="utf-8")
    return result


def install_profile(sequences, layers):
    settings = duplicate(TEMPLATES["settings"], OUT + "/Settings/AIS_Als_" + WEAPON)
    general = settings.get_editor_property("general")
    general.set_editor_property("use_foot_ik_bones", False)
    settings.set_editor_property("general", general)
    standing = settings.get_editor_property("standing")
    def measured_speed(name):
        sequence = sequences[name]
        roots = [LIB.get_bone_pose_for_frame(sequence, "root", i, False).translation
                 for i in range(LIB.get_num_frames(sequence))]
        # This source mannequin travels along local Y. Use full trajectory range,
        # not only the first/last samples, so a clipped loop cannot report zero.
        distance = max(p.y for p in roots) - min(p.y for p in roots)
        return distance / sequence.get_play_length()
    standing.set_editor_properties({"animated_walk_speed": measured_speed(MOVES["F"][0]),
                                    "animated_run_speed": measured_speed(MOVES["F"][1])})
    settings.set_editor_property("standing", standing)
    turn = settings.get_editor_property("turn_in_place")
    turn.set_editor_property("allow_automatic_turn_in_place", True)
    for field, name in zip(("standing_turn90_left", "standing_turn90_right",
                            "standing_turn180_left", "standing_turn180_right"), TURNS):
        value = turn.get_editor_property(field)
        value.set_editor_property("sequence", sequences[name])
        value.set_editor_property("scale_play_rate_by_animated_turn_angle", False)
    settings.set_editor_property("turn_in_place", turn)
    save(settings)

    if PROFILE:
        table = require(TABLE)
        before = json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(table))
        rows = json.loads(json.dumps(before))
        source = next(row for row in rows if row["Name"] == PROFILE_TEMPLATE_ROW)
        row = dict(source)
        row["Name"] = PROFILE_NAME
        for key, blueprint in (("AlsStandingAnimInstance", layers["Standing"]),
                               ("AlsBasePoseAnimInstance", layers["BasePose"]),
                               ("AlsOverlayAnimInstance", layers["Overlay"])):
            row[key] = blueprint.generated_class().get_path_name()
        row["AlsAnimationInstanceSettings"] = settings.get_path_name()
        rows = [item for item in rows if item["Name"] != row["Name"]] + [row]
        if not unreal.DataTableFunctionLibrary.fill_data_table_from_json_string(table, json.dumps(rows)):
            raise RuntimeError("Data table update failed")
        save(table)
        after = json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(table))
        if [r for r in before if r["Name"] != row["Name"]] != [r for r in after if r["Name"] != row["Name"]]:
            raise RuntimeError("Unrelated data table rows changed")
        (REPORT / "table-after.json").write_text(json.dumps(after, indent=2), encoding="utf-8")
    install_sample_lab_profile(settings, layers)


def install_sample_lab_profile(settings, layers):
    """Register generated assets in the plugin sample map without project-specific gameplay code."""
    table_path = "/Game/ALSAnimationAdaptation/DT_AlsSampleProfiles"
    table = unreal.load_asset(table_path)
    if not table:
        factory = unreal.DataTableFactory()
        factory.set_editor_property("struct", unreal.AlsSampleAnimationProfileRow.static_struct())
        table = TOOLS.create_asset("DT_AlsSampleProfiles", "/Game/ALSAnimationAdaptation", unreal.DataTable, factory)
        if not table:
            raise RuntimeError("Could not create sample-lab profile table")
    rows = json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(table))
    montage_paths = []
    for path in CONFIG.get("sample_lab", {}).get("montages", []):
        montage = require(path)
        if not isinstance(montage, unreal.AnimMontage):
            raise RuntimeError("sample_lab.montages entry is not an AnimMontage: " + path)
        invalid_slots = [str(track.get_editor_property("slot_name"))
                         for track in montage.get_editor_property("slot_anim_tracks")
                         if str(track.get_editor_property("slot_name")) != "PostLocomotion"]
        if invalid_slots:
            raise RuntimeError("sample_lab montage must use only PostLocomotion slots: %s -> %s" %
                               (path, invalid_slots))
        montage_paths.append(montage.get_path_name())
    row = {
        "Name": WEAPON,
        "MovementSettings": CONFIG.get("sample_lab", {}).get(
            "movement_settings", "/ALS/ALS/Data/Character/Movement/MS_Als_Normal.MS_Als_Normal"),
        "AnimationInstanceSettings": settings.get_path_name(),
        "StandingAnimInstance": layers["Standing"].generated_class().get_path_name(),
        "BasePoseAnimInstance": layers["BasePose"].generated_class().get_path_name(),
        "OverlayAnimInstance": layers["Overlay"].generated_class().get_path_name(),
        "Montages": montage_paths,
    }
    rows = [item for item in rows if item["Name"] != WEAPON] + [row]
    if not unreal.DataTableFunctionLibrary.fill_data_table_from_json_string(table, json.dumps(rows)):
        raise RuntimeError("Could not update sample-lab profile table")
    save(table)


def main():
    REPORT.mkdir(parents=True, exist_ok=True)
    unreal.AssetRegistryHelpers.get_asset_registry().search_all_assets(True)
    sequences = retarget()
    poses, overlay_idle = prepare_sequences(sequences)
    blends = make_blendspaces(sequences, poses)
    layers = build_layers(sequences, poses, blends, overlay_idle)
    install_profile(sequences, layers)
    manifest = {"schema_version": 1, "tool": "ALS/AnimationAdaptation", "config": str(CONFIG_PATH),
                "source": SOURCE, "output": OUT, "source_read_only": True,
                "animations": NAMES, "profile": PROFILE_NAME, "status": "success"}
    (REPORT / "manifest.json").write_text(json.dumps(manifest, indent=2), encoding="utf-8")
    unreal.log("ALS_ANIMATION_ADAPTATION_OK")


if __name__ == "__main__":
    main()
