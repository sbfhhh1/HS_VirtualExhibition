import math
import unreal


MAP_PATH = "/Game/TransformationVFX/SM2SM/SM2SM"
BLUEPRINT_DIR = "/Game/TransformationVFX/SM2SM"
BLUEPRINT_NAME = "BP_DoubleAccordionFoldingDoor"
BLUEPRINT_PATH = f"{BLUEPRINT_DIR}/{BLUEPRINT_NAME}"
ACTOR_LABEL = "BP_DoubleAccordionFoldingDoor_Runtime"
DEFAULT_OPEN_AMOUNT = 1.0
DEFAULT_FOLD_COUNT = 16
DEFAULT_TOTAL_WIDTH = 342.0
DEFAULT_STRIP_HEIGHT = 187.0
DEFAULT_MINIMUM_OPEN_AMOUNT = 0.02
DEFAULT_DEPTH_SCALE = 1.0
DEFAULT_THICKNESS = 2.5
DEFAULT_END_STRIP_WIDTH = 6.0
DEFAULT_END_STRIP_DEPTH = 6.0
DEFAULT_HEIGHT_VERTICES = 14
DEFAULT_LOCATION = unreal.Vector(120.0, 171.0, 160.0)
DEFAULT_ROTATION = unreal.Rotator(0.0, 0.0, -90.0)
DEFAULT_SCALE = unreal.Vector(1.0, 1.0, 1.0)


def log(message):
    unreal.log(f"[DoubleAccordionDoorSetup] {message}")


def set_prop(obj, names, value):
    for name in names:
        try:
            obj.set_editor_property(name, value)
            return True
        except Exception:
            pass
    log(f"Could not set property {names}")
    return False


def get_prop(obj, names, default=None):
    for name in names:
        try:
            return obj.get_editor_property(name)
        except Exception:
            pass
    return default


def load_first_existing(paths):
    for path in paths:
        if unreal.EditorAssetLibrary.does_asset_exist(path):
            return unreal.EditorAssetLibrary.load_asset(path)
    return None


def get_actor_by_label(label):
    for actor in unreal.EditorLevelLibrary.get_all_level_actors():
        if actor.get_actor_label() == label or actor.get_name() == label:
            return actor
    return None


def get_reference_transform(total_width):
    actors = unreal.EditorLevelLibrary.get_all_level_actors()
    left = None
    right = None
    for actor in actors:
        label = actor.get_actor_label()
        name = actor.get_name()
        if label == "Door_L" or name.startswith("Door_L"):
            left = actor
        elif label == "Door_R" or name.startswith("Door_R"):
            right = actor

    if not left or not right:
        log("Door_L/Door_R not found; using fallback transform.")
        return unreal.Vector(0.0, 0.0, 120.0), unreal.Rotator(0.0, 0.0, 0.0)

    left_loc = left.get_actor_location()
    right_loc = right.get_actor_location()
    center = (left_loc + right_loc) * 0.5
    direction = right_loc - left_loc
    direction.z = 0.0
    distance = direction.length()

    if distance > 10.0:
        direction = direction / distance
        yaw = math.degrees(math.atan2(direction.y, direction.x))
        total_width = max(total_width, distance)
        location = center - direction * (total_width * 0.5)
        rotation = unreal.Rotator(0.0, yaw, 0.0)
    else:
        location = center - unreal.Vector(total_width * 0.5, 0.0, 0.0)
        rotation = left.get_actor_rotation()

    location.z = center.z
    return location, rotation


def create_or_update_blueprint():
    parent_class = unreal.DoubleAccordionFoldingDoorActor
    bp = unreal.EditorAssetLibrary.load_asset(BLUEPRINT_PATH)
    if not bp:
        factory = unreal.BlueprintFactory()
        set_prop(factory, ["parent_class", "ParentClass"], parent_class)
        asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
        bp = asset_tools.create_asset(BLUEPRINT_NAME, BLUEPRINT_DIR, unreal.Blueprint, factory)
        log(f"Created {BLUEPRINT_PATH}")
    else:
        log(f"Using existing {BLUEPRINT_PATH}")

    cdo = bp.generated_class().get_default_object()
    set_prop(cdo, ["open_amount", "OpenAmount"], DEFAULT_OPEN_AMOUNT)
    set_prop(cdo, ["fold_count", "FoldCount"], DEFAULT_FOLD_COUNT)
    set_prop(cdo, ["total_width", "TotalWidth"], DEFAULT_TOTAL_WIDTH)
    set_prop(cdo, ["strip_height", "StripHeight"], DEFAULT_STRIP_HEIGHT)
    set_prop(cdo, ["minimum_open_amount", "MinimumOpenAmount"], DEFAULT_MINIMUM_OPEN_AMOUNT)
    set_prop(cdo, ["depth_scale", "DepthScale"], DEFAULT_DEPTH_SCALE)
    set_prop(cdo, ["thickness", "Thickness"], DEFAULT_THICKNESS)
    set_prop(cdo, ["end_strip_width", "EndStripWidth"], DEFAULT_END_STRIP_WIDTH)
    set_prop(cdo, ["end_strip_depth", "EndStripDepth"], DEFAULT_END_STRIP_DEPTH)
    set_prop(cdo, ["height_vertices", "HeightVertices"], DEFAULT_HEIGHT_VERTICES)

    door_mat = load_first_existing([
        "/Game/TransformationVFX/SM2SM/Door/light-sofa-upholstery-ue/Fabric",
        "/Game/TransformationVFX/SM2SM/Material",
        "/Game/TransformationVFX/SM2SM/Door/Material",
    ])
    wood_mat = load_first_existing([
        "/Game/TransformationVFX/SM2SM/wood",
        "/Game/TransformationVFX/SM2SM/Door/Material",
    ])
    if door_mat:
        set_prop(cdo, ["door_material", "DoorMaterial"], door_mat)
    if wood_mat:
        set_prop(cdo, ["wood_material", "WoodMaterial"], wood_mat)

    if hasattr(unreal, "KismetEditorUtilities"):
        unreal.KismetEditorUtilities.compile_blueprint(bp)
    elif hasattr(unreal, "BlueprintEditorLibrary"):
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    else:
        log("No explicit Blueprint compile API found; saving generated Blueprint without manual compile.")
    unreal.EditorAssetLibrary.save_asset(BLUEPRINT_PATH, only_if_is_dirty=False)
    return bp


def place_actor(bp):
    level_subsystem = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    level_subsystem.load_level(MAP_PATH)

    cdo = bp.generated_class().get_default_object()
    total_width = get_prop(cdo, ["total_width", "TotalWidth"], DEFAULT_TOTAL_WIDTH)
    location, rotation = DEFAULT_LOCATION, DEFAULT_ROTATION

    actor = get_actor_by_label(ACTOR_LABEL)
    if not actor:
        actor = unreal.EditorLevelLibrary.spawn_actor_from_class(bp.generated_class(), location, rotation)
        actor.set_actor_label(ACTOR_LABEL)
        log(f"Spawned {ACTOR_LABEL}")
    else:
        actor.set_actor_location(location, False, False)
        actor.set_actor_rotation(rotation, False)
        log(f"Updated existing {ACTOR_LABEL}")

    actor.set_actor_scale3d(DEFAULT_SCALE)
    set_prop(actor, ["open_amount", "OpenAmount"], DEFAULT_OPEN_AMOUNT)
    set_prop(actor, ["fold_count", "FoldCount"], DEFAULT_FOLD_COUNT)
    set_prop(actor, ["total_width", "TotalWidth"], total_width)
    set_prop(actor, ["strip_height", "StripHeight"], DEFAULT_STRIP_HEIGHT)
    set_prop(actor, ["minimum_open_amount", "MinimumOpenAmount"], DEFAULT_MINIMUM_OPEN_AMOUNT)
    set_prop(actor, ["depth_scale", "DepthScale"], DEFAULT_DEPTH_SCALE)
    set_prop(actor, ["thickness", "Thickness"], DEFAULT_THICKNESS)
    set_prop(actor, ["end_strip_width", "EndStripWidth"], DEFAULT_END_STRIP_WIDTH)
    set_prop(actor, ["end_strip_depth", "EndStripDepth"], DEFAULT_END_STRIP_DEPTH)
    set_prop(actor, ["height_vertices", "HeightVertices"], DEFAULT_HEIGHT_VERTICES)
    if hasattr(actor, "rebuild_door"):
        actor.rebuild_door()
    elif hasattr(actor, "RebuildDoor"):
        actor.RebuildDoor()

    unreal.EditorLevelLibrary.save_current_level()
    log(f"Saved level {MAP_PATH}")


def main():
    bp = create_or_update_blueprint()
    place_actor(bp)


if __name__ == "__main__":
    main()
