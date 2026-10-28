"""Migrate project montages and AB_Als_Base to the canonical PostLocomotion slot."""
import json
from pathlib import Path
import unreal


ROOT = "/Game"
BLUEPRINT = "/Game/BloodlineWarband/Characters/Animations/Shared/Blueprints/AB_Als_Base"
OLD_SLOT = "Grounded Slot"
TARGET_SLOT = "PostLocomotion"
EAL = unreal.EditorAssetLibrary
HELPER = unreal.AlsAnimationAdaptationLibrary


def track_slots(montage):
    return [str(track.get_editor_property("slot_name"))
            for track in montage.get_editor_property("slot_anim_tracks")]


def main():
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    registry.search_all_assets(True)
    before = {}
    changed = {}
    errors = []
    montage_records = [record for record in registry.get_assets_by_path(ROOT, recursive=True)
                       if str(record.asset_class_path.asset_name) == "AnimMontage"]
    for record in montage_records:
        path = str(record.package_name)
        montage = unreal.load_asset(path)
        if not montage:
            errors.append("Could not load montage: " + path)
            continue
        slots = track_slots(montage)
        before[path] = slots
        count = HELPER.normalize_montage_slots(montage, TARGET_SLOT)
        if count < 0:
            errors.append("Could not normalize montage: " + path)
        elif count:
            changed[path] = {"old": slots, "changed_tracks": count}
            if not EAL.save_loaded_asset(montage, only_if_is_dirty=False):
                errors.append("Could not save montage: " + path)

    blueprint = unreal.load_asset(BLUEPRINT)
    if not blueprint:
        errors.append("Missing animation blueprint: " + BLUEPRINT)
        blueprint_changes = -1
    else:
        blueprint_changes = HELPER.replace_blueprint_slot_name(blueprint, OLD_SLOT, TARGET_SLOT)
        if blueprint_changes < 0:
            errors.append("AB_Als_Base slot replacement or compilation failed")
        elif blueprint_changes and not EAL.save_loaded_asset(blueprint, only_if_is_dirty=False):
            errors.append("Could not save AB_Als_Base")

    remaining = {}
    for path in before:
        montage = unreal.load_asset(path)
        slots = track_slots(montage)
        invalid = [slot for slot in slots if slot != TARGET_SLOT]
        if invalid:
            remaining[path] = invalid
    if remaining:
        errors.append("Non-PostLocomotion montage slots remain")

    result = {
        "scope": ROOT,
        "target_slot": TARGET_SLOT,
        "montages_scanned": len(before),
        "montages_changed": len(changed),
        "changed": changed,
        "before": before,
        "blueprint": BLUEPRINT,
        "blueprint_nodes_changed": blueprint_changes,
        "remaining_invalid_slots": remaining,
        "errors": errors,
    }
    report = Path(unreal.Paths.project_saved_dir()) / "ALSAnimationAdaptation" / "post-locomotion-migration.json"
    report.parent.mkdir(parents=True, exist_ok=True)
    report.write_text(json.dumps(result, indent=2, ensure_ascii=False), encoding="utf-8")
    if errors:
        raise RuntimeError("; ".join(errors))
    unreal.log("ALS_POST_LOCOMOTION_SLOT_MIGRATION_OK")


if __name__ == "__main__":
    main()
