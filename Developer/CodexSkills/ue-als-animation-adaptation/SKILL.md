---
name: ue-als-animation-adaptation
description: Configure and run the model-independent ALS plugin automation for adapting external weapon locomotion animation packs, then validate generated assets and runtime playback.
---

# ALS animation adaptation

Read `Plugins/ALS/Docs/AnimationAdaptation.md` in the target project first. The plugin JSON contract and command-line tools are authoritative; this Skill is only a Codex adapter and must not become a required runtime dependency.

Create or update a project-owned schema-version-1 configuration under `Config/ALSAnimationAdaptation/`. Run the plugin's pure-Python preflight before launching Unreal. Keep source animation directories read-only, preserve existing outputs incrementally, and report uncertain contact detection instead of guessing.

Enforce the three Idle responsibilities: Standing uses the complete Idle with ALS layer curves, BasePose uses a one-frame curve-free `_Idle_Pose`, and Overlay uses a complete curve-free `_Idle_Overlay`. Generate the final `AB_Als_Standing_{Weapon}` directly; never create a `Standing_*Template` intermediate.

After authoring, inspect the machine-readable report and run the target project's runtime test. Separate technical validation from the user's visual approval.

The generic builder registers completed profiles in `/Game/ALSAnimationAdaptation/DT_AlsSampleProfiles`. Open `/ALS/ALSSampleTemplate/Maps/L_AlsAnimationLab` for a plugin-owned playable check; use Q/E or 1-9 to switch generated profiles. This sample map is the default cross-project visual test and does not replace project-specific runtime validation.
