"""Verify the project-wide PostLocomotion montage slot contract."""
import json
from pathlib import Path
import unreal


ROOT = "/Game"
BLUEPRINT = "/Game/BloodlineWarband/Characters/Animations/Shared/Blueprints/AB_Als_Base"
TARGET_SLOT = "PostLocomotion"


def main():
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    registry.search_all_assets(True)
    invalid = {}
    scanned = 0
    for record in registry.get_assets_by_path(ROOT, recursive=True):
        if str(record.asset_class_path.asset_name) != "AnimMontage":
            continue
        scanned += 1
        path = str(record.package_name)
        montage = unreal.load_asset(path)
        slots = [str(track.get_editor_property("slot_name"))
                 for track in montage.get_editor_property("slot_anim_tracks")]
        bad = [slot for slot in slots if slot != TARGET_SLOT]
        if bad:
            invalid[path] = bad

    blueprint = unreal.load_asset(BLUEPRINT)
    description = unreal.AlsAnimationAdaptationLibrary.describe_blueprint(blueprint) if blueprint else ""
    blueprint_valid = bool(blueprint and TARGET_SLOT in description and "Grounded Slot" not in description)
    errors = []
    if invalid:
        errors.append("Project montages contain non-PostLocomotion slots")
    if not blueprint_valid:
        errors.append("AB_Als_Base does not consume PostLocomotion exclusively")
    result = {"montages_scanned": scanned, "target_slot": TARGET_SLOT,
              "invalid_montages": invalid, "blueprint_valid": blueprint_valid, "errors": errors}
    report = Path(unreal.Paths.project_saved_dir()) / "ALSAnimationAdaptation" / "post-locomotion-validation.json"
    report.parent.mkdir(parents=True, exist_ok=True)
    report.write_text(json.dumps(result, indent=2, ensure_ascii=False), encoding="utf-8")
    if errors:
        raise RuntimeError("; ".join(errors))
    unreal.log("ALS_POST_LOCOMOTION_SLOT_VALIDATION_OK")


if __name__ == "__main__":
    main()
