# -*- coding: utf-8 -*-
"""
修复折叠门「中间两根木条不随开门移动」：给木材加 WPO。

木材槽用的是共享 Substance 模板（/Substance/...），不能直接改（会污染全工程）。本脚本：
 1) 把共享模板复制成工程内副本 /Game/TransformationVFX/SM2SM/Door/M_FoldingDoorWood（独立、可安全改）；
 2) 给副本加 WPO（与布料同款，含 PreviousFrameSwitch；木材 UV1.y=0，只在 X 平移不折叠，木条移动也不糊）；
 3) 把门用的那个木材实例 reparent 到副本 —— 参数名完全相同，木纹外观不变，但继承了 WPO。

C++ 的 WoodMID 每帧已经在推 EffectiveOpen/EffectiveOpenPrev，reparent 后这些参数即生效，无需改 C++。

用法（编辑器 Output Log → Python 命令框）：
    py "C:/Users/lafa/Documents/GitHub/HS_VirtualExhibition/tools/fix_wood_material.py"

注意：若以后用 Substance 编辑器重新生成该木材实例，parent 可能被重置回共享模板，届时重跑本脚本即可。
"""
import unreal

MEL = unreal.MaterialEditingLibrary
EAL = unreal.EditorAssetLibrary
EFF = "EffectiveOpen"
EFF_PREV = "EffectiveOpenPrev"
F3 = unreal.CustomMaterialOutputType.CMOT_FLOAT3
DUP_PATH = "/Game/TransformationVFX/SM2SM/Door/M_FoldingDoorWood"


def log(m):
    unreal.log("[FoldingDoorWood] " + m)


def warn(m):
    unreal.log_warning("[FoldingDoorWood] " + m)


def find_door_actors():
    sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    return [a for a in sub.get_all_level_actors()
            if "DoubleAccordionFoldingDoor" in a.get_class().get_name()]


# ---------- 节点工具（与布料脚本一致） ----------

def expr(mat, cls, x, y):
    return MEL.create_material_expression(mat, cls, x, y)


def scalar_param(mat, name, default, x, y):
    e = expr(mat, unreal.MaterialExpressionScalarParameter, x, y)
    e.set_editor_property("parameter_name", name)
    e.set_editor_property("default_value", default)
    e.set_editor_property("group", "FoldingDoor")
    return e


def texcoord(mat, idx, x, y):
    e = expr(mat, unreal.MaterialExpressionTextureCoordinate, x, y)
    e.set_editor_property("coordinate_index", idx)
    return e


def custom(mat, desc, code, inputs, x, y):
    c = expr(mat, unreal.MaterialExpressionCustom, x, y)
    c.set_editor_property("description", desc)
    c.set_editor_property("code", code)
    c.set_editor_property("output_type", F3)
    arr = []
    for nm in inputs:
        ci = unreal.CustomInput()
        ci.set_editor_property("input_name", nm)
        arr.append(ci)
    c.set_editor_property("inputs", arr)
    return c


def transform_l2w(mat, x, y):
    t = expr(mat, unreal.MaterialExpressionTransform, x, y)
    t.set_editor_property("transform_source_type",
                          unreal.MaterialVectorCoordTransformSource.TRANSFORMSOURCE_LOCAL)
    t.set_editor_property("transform_type",
                          unreal.MaterialVectorCoordTransform.TRANSFORM_WORLD)
    return t


def link(a, ao, b, bo):
    if not MEL.connect_material_expressions(a, ao, b, bo):
        warn("connect 失败: {} -> {}[{}]".format(a.get_name(), b.get_name(), bo))


def link_try(a, ao, b, names):
    for nm in names:
        if MEL.connect_material_expressions(a, ao, b, nm):
            return True
    warn("connect 失败(候选名都不行): {} -> {} {}".format(a.get_name(), b.get_name(), names))
    return False


WPO_CODE = (
    "float ox = Fold.x * (Eo - 1.0);\n"
    "float oy = Fold.y * sqrt(saturate(1.0 - Eo*Eo));\n"
    "return float3(ox, oy, 0.0);"
)


def has_eff_param(mat):
    try:
        return EFF in [str(n) for n in MEL.get_scalar_parameter_names(mat)]
    except Exception:
        return False


def add_wpo(mat):
    eff_cur = scalar_param(mat, EFF, 1.0, -1300, 40)
    eff_prev = scalar_param(mat, EFF_PREV, 1.0, -1300, 120)
    uv1 = texcoord(mat, 1, -1300, 250)
    cur = custom(mat, "FoldWPO_Cur", WPO_CODE, ["Eo", "Fold"], -1000, 180)
    link(eff_cur, "", cur, "Eo")
    link(uv1, "", cur, "Fold")
    prv = custom(mat, "FoldWPO_Prev", WPO_CODE, ["Eo", "Fold"], -1000, 360)
    link(eff_prev, "", prv, "Eo")
    link(uv1, "", prv, "Fold")
    pfs = expr(mat, unreal.MaterialExpressionPreviousFrameSwitch, -700, 270)
    link_try(cur, "", pfs, ["Current Frame", "CurrentFrame", "Current"])
    link_try(prv, "", pfs, ["Previous Frame", "PreviousFrame", "Previous"])
    t = transform_l2w(mat, -450, 270)
    link(pfs, "", t, "")
    if MEL.connect_material_property(t, "", unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET):
        log("副本 WPO 已接好（含 PreviousFrameSwitch）")
    else:
        warn("WPO 引脚连接失败，请手动把 Transform 接到 World Position Offset")


def main():
    doors = find_door_actors()
    if not doors:
        warn("当前关卡未找到 DoubleAccordionFoldingDoor Actor。")
        return
    comp = doors[0].get_component_by_class(unreal.ProceduralMeshComponent)
    wood_mi = comp.get_material(1) if comp else None
    if wood_mi is None:
        warn("木材槽1为空。")
        return
    log("木材槽1当前材质 = {} ({})".format(wood_mi.get_path_name(), wood_mi.get_class().get_name()))

    base = wood_mi if isinstance(wood_mi, unreal.Material) else wood_mi.get_base_material()
    if base is None:
        warn("无法解析木材基础 Material。")
        return
    log("木材基础 Material = {}".format(base.get_path_name()))

    # 1) 复制基础模板到工程内副本（已存在则复用）
    if EAL.does_asset_exist(DUP_PATH):
        dup = EAL.load_asset(DUP_PATH)
        log("复用已存在副本 {}".format(DUP_PATH))
    else:
        src = base.get_path_name().split(".")[0]
        dup = EAL.duplicate_asset(src, DUP_PATH)
        if dup is None:
            warn("复制模板失败：{} -> {}".format(src, DUP_PATH))
            return
        log("已复制模板到工程内副本 {}".format(DUP_PATH))

    # 2) 给副本加 WPO（幂等）
    if has_eff_param(dup):
        log("副本已含 {} 参数，跳过加节点".format(EFF))
    else:
        add_wpo(dup)
        MEL.recompile_material(dup)
    EAL.save_loaded_asset(dup)

    # 3) 把木材实例 reparent 到副本
    if isinstance(wood_mi, unreal.Material):
        warn("木材槽1直接是 Material 而非实例，已给副本加 WPO；请把门木材槽1改用 {} 或在该 Material 上手动加 WPO。".format(DUP_PATH))
        return
    try:
        wood_mi.set_editor_property("parent", dup)
        EAL.save_loaded_asset(wood_mi)
        log("已把木材实例 reparent 到副本，木纹参数保留 + 继承 WPO。")
    except Exception as e:
        warn("reparent 失败（{}）。可改为：把门木材槽1直接指向 {}（会用模板默认木纹）。".format(e, DUP_PATH))
        return

    log("完成。按方向键验证：开门时中间两根木条随之外移，且不糊。")


main()
