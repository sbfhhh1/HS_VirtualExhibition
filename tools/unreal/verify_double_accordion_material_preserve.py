import unreal


LEVEL_PATH = "/Game/TransformationVFX/SM2SM/SM2SM"
ACTOR_LABEL = "BP_DoubleAccordionFoldingDoor_Runtime"


def main():
    unreal.EditorLoadingAndSavingUtils.load_map(LEVEL_PATH)
    actors = unreal.EditorLevelLibrary.get_all_level_actors()
    matches = [actor for actor in actors if actor.get_actor_label() == ACTOR_LABEL]
    if not matches:
        raise RuntimeError(f"Missing level actor: {ACTOR_LABEL}")

    actor = matches[0]
    proc_mesh = actor.get_component_by_class(unreal.ProceduralMeshComponent)
    if not proc_mesh:
        raise RuntimeError(f"Actor has no ProceduralMeshComponent: {ACTOR_LABEL}")

    original = proc_mesh.get_material(0)
    replacement = proc_mesh.get_material(1)
    if not replacement:
        raise RuntimeError("Element 1 has no material to use as replacement")

    proc_mesh.set_material(0, replacement)
    if hasattr(actor, "rebuild_door"):
        actor.rebuild_door()
    elif hasattr(actor, "RebuildDoor"):
        actor.RebuildDoor()
    else:
        raise RuntimeError("Actor has no RebuildDoor callable")

    after = proc_mesh.get_material(0)
    if after != replacement:
        raise RuntimeError(
            "Element 0 material was not preserved after RebuildDoor: "
            f"original={original}, replacement={replacement}, after={after}"
        )

    unreal.log(
        "DoubleAccordion material preserve verified: "
        f"original={original.get_name() if original else 'None'}, "
        f"replacement={replacement.get_name()}, after={after.get_name()}"
    )


main()
