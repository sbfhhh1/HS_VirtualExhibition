import unreal

log = []
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
in_pie = les.is_in_play_in_editor()
log.append("in_pie=%s" % in_pie)

if in_pie:
    worlds = unreal.EditorLevelLibrary.get_pie_worlds(False)
    for w in worlds:
        for a in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.Actor):
            if "DoubleAccordion" in a.get_class().get_name():
                for m in ("stop_door_animation", "StopDoorAnimation"):
                    if hasattr(a, m):
                        getattr(a, m)()
                        break
                for m in ("set_open_amount", "SetOpenAmount"):
                    if hasattr(a, m):
                        getattr(a, m)(0.5)
                        break
                log.append("set half static, open=%s" % a.get_editor_property("OpenAmount"))
                break

with open(r"C:/Users/lafa/Documents/GitHub/HS_VirtualExhibition/tools/unreal/halflog.txt", "w", encoding="utf-8") as f:
    f.write("\n".join(log))
