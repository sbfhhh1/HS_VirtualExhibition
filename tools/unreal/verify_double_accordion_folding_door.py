import unreal


BLUEPRINT_PATH = "/Game/TransformationVFX/SM2SM/BP_DoubleAccordionFoldingDoor"
LEVEL_PATH = "/Game/TransformationVFX/SM2SM/SM2SM"
ACTOR_LABEL = "BP_DoubleAccordionFoldingDoor_Runtime"
EXPECTED_VALUES = {
    "OpenAmount": 1.0,
    "FoldCount": 16,
    "TotalWidth": 342.0,
    "StripHeight": 187.0,
    "MinimumOpenAmount": 0.02,
    "DepthScale": 1.0,
    "Thickness": 2.5,
    "EndStripWidth": 6.0,
    "EndStripDepth": 6.0,
    "HeightVertices": 14,
}
EXPECTED_LOCATION = unreal.Vector(120.0, 171.0, 160.0)
EXPECTED_ROTATION = unreal.Rotator(0.0, 0.0, -90.0)


def assert_near(name, actual, expected, tolerance=0.05):
    if abs(float(actual) - float(expected)) > tolerance:
        raise RuntimeError(f"{name} expected {expected}, got {actual}")


def main():
    bp = unreal.EditorAssetLibrary.load_asset(BLUEPRINT_PATH)
    if not bp:
        raise RuntimeError(f"Missing blueprint asset: {BLUEPRINT_PATH}")

    unreal.EditorLoadingAndSavingUtils.load_map(LEVEL_PATH)
    actors = unreal.EditorLevelLibrary.get_all_level_actors()
    matches = [actor for actor in actors if actor.get_actor_label() == ACTOR_LABEL]
    if not matches:
        raise RuntimeError(f"Missing level actor: {ACTOR_LABEL}")

    actor = matches[0]
    actor_class = actor.get_class().get_name()
    proc_mesh = actor.get_component_by_class(unreal.ProceduralMeshComponent)
    if not proc_mesh:
        raise RuntimeError(f"Actor has no ProceduralMeshComponent: {ACTOR_LABEL}")

    for prop_name, expected_value in EXPECTED_VALUES.items():
        actual_value = actor.get_editor_property(prop_name)
        assert_near(prop_name, actual_value, expected_value)

    location = actor.get_actor_location()
    rotation = actor.get_actor_rotation()
    assert_near("Location.X", location.x, EXPECTED_LOCATION.x)
    assert_near("Location.Y", location.y, EXPECTED_LOCATION.y)
    assert_near("Location.Z", location.z, EXPECTED_LOCATION.z)
    assert_near("Rotation.Pitch", rotation.pitch, EXPECTED_ROTATION.pitch)
    assert_near("Rotation.Yaw", rotation.yaw, EXPECTED_ROTATION.yaw)
    assert_near("Rotation.Roll", rotation.roll, EXPECTED_ROTATION.roll)

    unreal.log(
        "DoubleAccordionFoldingDoor verified: "
        f"blueprint={BLUEPRINT_PATH}, actor={ACTOR_LABEL}, "
        f"class={actor_class}, open_amount={actor.get_editor_property('OpenAmount')}, "
        f"fold_count={actor.get_editor_property('FoldCount')}, "
        f"total_width={actor.get_editor_property('TotalWidth')}, "
        f"location={location}, rotation={rotation}"
    )


main()
