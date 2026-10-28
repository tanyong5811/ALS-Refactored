"""Create the self-contained ALS plugin sample map. Safe to rerun."""
import unreal

ROOT = "/ALS/ALSSampleTemplate"
MAP = ROOT + "/Maps/L_AlsAnimationLab"
EAL = unreal.EditorAssetLibrary
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
unreal.AssetRegistryHelpers.get_asset_registry().scan_paths_synchronous([ROOT], force_rescan=True)

def blueprint(name, parent):
    path = ROOT + "/Blueprints/" + name
    asset = unreal.load_asset(path)
    if asset:
        return asset
    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", parent)
    asset = TOOLS.create_asset(name, ROOT + "/Blueprints", unreal.Blueprint, factory)
    if not asset:
        raise RuntimeError("Could not create " + path)
    return asset

character = blueprint("BP_AlsSampleAnimationLabCharacter", unreal.AlsSampleAnimationLabCharacter)
game_mode = blueprint("BP_AlsSampleAnimationLabGameMode", unreal.AlsSampleAnimationLabGameMode)
game_mode_default = unreal.get_default_object(game_mode.generated_class())
game_mode_default.set_editor_property("default_pawn_class", character.generated_class())
unreal.BlueprintEditorLibrary.compile_blueprint(character)
unreal.BlueprintEditorLibrary.compile_blueprint(game_mode)
EAL.save_loaded_asset(character, only_if_is_dirty=False)
EAL.save_loaded_asset(game_mode, only_if_is_dirty=False)

if not unreal.load_asset(MAP):
    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    if not levels.new_level(MAP):
        raise RuntimeError("Could not create " + MAP)
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    world.get_world_settings().set_editor_property("default_game_mode", game_mode.generated_class())
    world.get_world_settings().set_editor_property("force_no_precomputed_lighting", True)
    cube = unreal.load_asset("/Engine/BasicShapes/Cube")

    def block(label, location, scale):
        actor = actors.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(*location))
        actor.set_actor_label(label)
        actor.static_mesh_component.set_static_mesh(cube)
        actor.static_mesh_component.set_collision_profile_name("BlockAll")
        actor.set_actor_scale3d(unreal.Vector(*scale))

    block("SampleLab_Floor", (0, 0, -20), (60, 60, .4))
    for coordinate in range(-2500, 2501, 500):
        block("GridX_" + str(coordinate), (coordinate, 0, .5), (.015, 60, .008))
        block("GridY_" + str(coordinate), (0, coordinate, .5), (60, .015, .008))
    for index in range(8):
        block("Step_" + str(index + 1), (1000 + index * 80, 700, (index + 1) * 10), (.8, 3, (index + 1) * .2))
    actors.spawn_actor_from_class(unreal.PlayerStart, unreal.Vector(0, 0, 110))
    sun = actors.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, 0, 700), unreal.Rotator(pitch=-50, yaw=-25, roll=0))
    sun.light_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    sun.light_component.set_editor_property("intensity", 4.0)
    sky = actors.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(0, 0, 600))
    sky.light_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    sky.light_component.set_editor_property("intensity", 1.0)
    actors.spawn_actor_from_class(unreal.SkyAtmosphere, unreal.Vector())
    if not levels.save_current_level():
        raise RuntimeError("Could not save " + MAP)
else:
    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if not levels.load_level(MAP):
        raise RuntimeError("Sample map failed to reload")
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    world.get_world_settings().set_editor_property("default_game_mode", game_mode.generated_class())
    if not levels.save_current_level():
        raise RuntimeError("Could not update " + MAP)

unreal.log("ALS_SAMPLE_ANIMATION_LAB_READY " + MAP)
