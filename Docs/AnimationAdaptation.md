# ALS Animation Adaptation Automation

This directory is the portable, model-independent contract for adapting an external weapon locomotion set to ALS. Copying the complete `ALS` plugin transfers the editor library, build tool, validation tool, documentation, and optional AI skill adapter.

## Stable interface

The caller may be a human, CI job, local program, Codex, or another model. The core never calls a model API and does not require a Skill. A caller must:

1. Create a project-owned JSON configuration conforming to schema version 1. Use `Config/ALSAnimationAdaptation/<Pack>.json`; keep source assets read-only.
2. Run the pure-Python preflight:

   `python Plugins/ALS/Tools/AnimationAdaptation/validate_config.py Config/ALSAnimationAdaptation/<Pack>.json`

3. Close the target editor, then execute the UE tool using the matching engine:

   `UnrealEditor-Cmd.exe <Project>.uproject -run=pythonscript -script=<Project>/Plugins/ALS/Tools/AnimationAdaptation/build_from_config.py -AlsAdaptationConfig=<absolute-config-path> -unattended -nop4 -nosplash -nullrhi -NoLiveCoding`

4. Treat a zero process exit plus `manifest.json` with `"status": "success"` as authoring success. Then run project-specific runtime tests and visual review.

5. Open `/ALS/ALSSampleTemplate/Maps/L_AlsAnimationLab`. The build tool automatically registers every generated set in `/Game/ALSAnimationAdaptation/DT_AlsSampleProfiles`. Use `Q/E` or number keys `1-9` to switch profiles, `[` / `]` to select that profile's montage, and Enter to play it. Test movement with `WASD`, walking with `Ctrl`, sprinting with `Shift`, aiming with RMB, and crouching with `C`.

## Configuration contract

Required fields are checked by `validate_config.py`. The animation map contains one Idle, `[Walk, Run]` for `F/B/L/R`, and `L90/R90/L180/R180`. Template paths are optional: by default the builder uses the self-contained assets under `/ALS/ALSSampleTemplate`. The `profile` block is also optional and is only for integrating a project-specific runtime DataTable; the sample-lab registration is always created automatically. Optional `sample_lab.montages` is an array of already adapted `UAnimMontage` asset paths. Every track must use the canonical `PostLocomotion` slot consumed by `AB_Als_Base`; `Grounded Slot` is forbidden. These are stored per profile and exposed by the sample map; the locomotion adapter does not automatically convert attacks or combat logic into montages. Run `migrate_post_locomotion_slots.py` once in an existing project to normalize all `/Game` montages and the shared base animation Blueprint, then run `validate_post_locomotion_slots.py` as the regression gate.

The tool creates `{Weapon}_Idle_Overlay` as a full curve-free loop, creates full-duration curve-free poses whose every bone key is frozen at source frame 0, and keeps the Standing Idle separate with ALS layer curves. Preserving the source frame count prevents degenerate BlendSpace timing and synchronization. It creates `AB_Als_Standing_{Weapon}` directly from the ALS Standing graph. `AB_Als_Standing_{Weapon}Template` is forbidden.

Project configuration is intentionally outside the plugin: copying the plugin preserves the engine, while each project supplies its own content paths, template row, output root, and runtime tests. `Config/ALSAnimationAdaptation/SpearAndHalberd.json` in BloodlineWarband is the validated reference configuration.

The playable sample map and character are plugin-owned and do not depend on a project Blueprint. The generated profile table is project-owned so copying the plugin never carries stale references to another project's animations.

## Bundled template content

`/ALS/ALSSampleTemplate` contains 14 complete sequences (Standing Idle, curve-free Overlay Idle, eight directional Walk/Run sequences, and four turns), 13 generated full-duration first-frame-frozen poses, four locomotion BlendSpaces, four turn BlendSpaces, `AB_Als_Standing_TemplateSource`, `AB_Als_BasePose_TemplateSource`, `AB_Als_Overlay_TemplateSource`, `AIS_Als_Template`, the ALS target IK Rig, and the playable sample map/character. Generated weapon Standing Blueprints are direct copies of the single `TemplateSource`; no per-weapon intermediate Template is created. The source IK Rig and Retargeter are pack-specific and remain required configuration inputs; bundling one that references another project's source Skeletal Mesh would break plugin independence.

Run `validate_sample_templates.py` through `UnrealEditor-Cmd` to verify completeness, source-matching Pose frame counts, frozen first/middle/last samples, Blueprint compilation, and the absence of references to `/Game` project content.

## AI interoperability

Give any model this file, the JSON preflight output, and the generated report directory. The model should edit only project JSON or source mappings; it should not rewrite the engine for each animation pack. The JSON schema version and machine-readable manifest are the compatibility boundary. Codex may additionally use the bundled Skill, but that adapter is optional.
