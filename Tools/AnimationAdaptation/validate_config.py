"""Pure-Python preflight for ALS animation-adaptation JSON files."""
import json
import pathlib
import re
import sys

REQUIRED_TOP = {"schema_version", "weapon", "source_root", "source_mesh", "output_root", "animations"}
DIRECTIONS = {"F", "B", "L", "R"}
TURNS = {"L90", "R90", "L180", "R180"}


def validate(data):
    errors = []
    missing = REQUIRED_TOP - set(data)
    if missing:
        errors.append("missing top-level fields: " + ", ".join(sorted(missing)))
    if data.get("schema_version") != 1:
        errors.append("schema_version must be 1")
    weapon = data.get("weapon", "")
    if not re.fullmatch(r"[A-Za-z][A-Za-z0-9_]*", weapon):
        errors.append("weapon must be a stable Unreal identifier")
    if "Template" in data.get("output_root", ""):
        errors.append("output_root must not create a Template hierarchy")
    animations = data.get("animations", {})
    if set(animations.get("moves", {})) != DIRECTIONS:
        errors.append("animations.moves must contain exactly F, B, L, R")
    else:
        for direction, names in animations["moves"].items():
            if not isinstance(names, list) or len(names) != 2 or not all(isinstance(x, str) and x for x in names):
                errors.append("animations.moves.%s must be [walk, run]" % direction)
    if set(animations.get("turns", {})) != TURNS:
        errors.append("animations.turns must contain exactly L90, R90, L180, R180")
    if not data.get("retargeter", {}).get("template"):
        errors.append("retargeter.template is required for the source skeleton")
    templates = data.get("templates", {})
    if any("Standing_" + weapon + "Template" in str(value) for value in templates.values()):
        errors.append("Standing Template assets are forbidden")
    profile = data.get("profile")
    if profile:
        for key in ("data_table", "template_row", "row_name"):
            if not profile.get(key):
                errors.append("profile.%s is required when profile integration is enabled" % key)
    montages = data.get("sample_lab", {}).get("montages", [])
    if not isinstance(montages, list) or not all(isinstance(path, str) and path.startswith("/") for path in montages):
        errors.append("sample_lab.montages must be an array of Unreal asset paths")
    return errors


def main():
    if len(sys.argv) != 2:
        raise SystemExit("usage: python validate_config.py <config.json>")
    path = pathlib.Path(sys.argv[1]).resolve()
    data = json.loads(path.read_text(encoding="utf-8-sig"))
    errors = validate(data)
    result = {"valid": not errors, "config": str(path), "errors": errors}
    print(json.dumps(result, ensure_ascii=False, indent=2))
    raise SystemExit(0 if not errors else 2)


if __name__ == "__main__":
    main()
