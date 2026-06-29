# -*- coding: utf-8 -*-
"""
在 UE 编辑器里运行：为对开折叠门的布料材质添加
 1) WPO 折叠动画（读 UV1=(Cx,Cy)），并用 PreviousFrameSwitch 输出正确运动矢量（TSR 去模糊关键）
 2) 解析法线（读 UV2=(折面坡度系数, ny)），让折面受光正确

用法（编辑器 Output Log 下方命令框，模式选 Python）：
    py "C:/Users/lafa/Documents/GitHub/HS_VirtualExhibition/tools/build_folding_door_materials.py"

要点：
 - 自动从关卡里门 Actor 的 ProcMesh 槽0 读真正在用的布料材质（解析到基础 Material）。
 - 仅处理 /Game 下的工程材质；绝不修改 /Substance 等共享/插件模板（木材槽用的是共享 Substance
   模板，会被跳过，避免污染全工程）。
 - 幂等：每次运行先清除上次加的折叠节点（EffectiveOpen/EffectiveOpenPrev 参数、Fold* Custom），再重建。
 - C++ 端每帧推 EffectiveOpen + EffectiveOpenPrev 两个标量；材质用 PreviousFrameSwitch 区分当前/上一帧。
"""
import unreal

MEL = unreal.MaterialEditingLibrary
EFF = "EffectiveOpen"
EFF_PREV = "EffectiveOpenPrev"
F3 = unreal.CustomMaterialOutputType.CMOT_FLOAT3


def log(m):
    unreal.log("[FoldingDoorMat] " + m)


def warn(m):
    unreal.log_warning("[FoldingDoorMat] " + m)


# ---------------------------------------------------------------- 资产解析

def find_door_actors():
    sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    return [a for a in sub.get_all_level_actors()
            if "DoubleAccordionFoldingDoor" in a.get_class().get_name()]


def get_section_materials(actor):
    comp = actor.get_component_by_class(unreal.ProceduralMeshComponent)
    if comp is None:
        return (None, None)
    return (comp.get_material(0), comp.get_material(1))


def resolve_base_material(mat_iface):
    if mat_iface is None:
        return None
    if isinstance(mat_iface, unreal.Material):
        return mat_iface
    try:
        return mat_iface.get_base_material()
    except Exception:
        return None


# ---------------------------------------------------------------- 节点工具

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
    ok = MEL.connect_material_expressions(a, ao, b, bo)
    if not ok:
        warn("connect 失败: {}[{}] -> {}[{}]".format(a.get_name(), ao, b.get_name(), bo))
    return ok


def link_try(a, ao, b, names):
    """目标输入引脚名在不同 UE 版本可能不同，逐个尝试。"""
    for nm in names:
        if MEL.connect_material_expressions(a, ao, b, nm):
            return True
    warn("connect 失败(候选名都不行): {} -> {} {}".format(a.get_name(), b.get_name(), names))
    return False


# ---------------------------------------------------------------- 清除旧节点

def purge_fold_nodes(mat):
    try:
        exprs = list(mat.get_editor_property("expressions"))
    except Exception as e:
        warn("无法枚举材质表达式（{}）。若重复运行有冗余节点，请手动删除旧的 Fold* / EffectiveOpen 节点。".format(e))
        return
    removed = 0
    for e in exprs:
        cls = e.get_class().get_name()
        kill = False
        if cls == "MaterialExpressionScalarParameter":
            nm = str(e.get_editor_property("parameter_name"))
            kill = nm in (EFF, EFF_PREV)
        elif cls == "MaterialExpressionCustom":
            kill = str(e.get_editor_property("description")).startswith("Fold")
        if kill:
            MEL.delete_material_expression(mat, e)
            removed += 1
    if removed:
        log("清除旧折叠节点 {} 个（重建）".format(removed))


# ---------------------------------------------------------------- WPO（含 PreviousFrameSwitch）

WPO_CODE = (
    "float ox = Fold.x * (Eo - 1.0);\n"
    "float oy = Fold.y * sqrt(saturate(1.0 - Eo*Eo));\n"
    "return float3(ox, oy, 0.0);"
)


def add_wpo(mat, eff_cur, eff_prev):
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
        log("WPO 已接好（含 PreviousFrameSwitch 运动矢量）")
    else:
        warn("WPO 引脚连接失败，请手动把 Transform 接到 World Position Offset")


# ---------------------------------------------------------------- 解析法线（布料）

NRM_CODE = (
    "float ny = FN.y;\n"
    "float S = sqrt(saturate(1.0 - Eo*Eo));\n"
    "float eoSafe = max(Eo, 0.001);\n"
    "float g = (S / eoSafe) * FN.x;\n"
    "return normalize(float3(ny * (-g), ny, 0.0));"
)


def add_normal(mat, eff_cur):
    uv2 = texcoord(mat, 2, -1300, 600)
    c = custom(mat, "FoldNormal_Local", NRM_CODE, ["Eo", "FN"], -1000, 600)
    link(eff_cur, "", c, "Eo")
    link(uv2, "", c, "FN")
    tN = transform_l2w(mat, -700, 600)
    link(c, "", tN, "")

    cm = expr(mat, unreal.MaterialExpressionComponentMask, -1000, 820)
    cm.set_editor_property("r", False)
    cm.set_editor_property("g", True)
    cm.set_editor_property("b", False)
    cm.set_editor_property("a", False)
    link(uv2, "", cm, "")
    ab = expr(mat, unreal.MaterialExpressionAbs, -800, 820)
    link(cm, "", ab, "")

    restN = expr(mat, unreal.MaterialExpressionVertexNormalWS, -700, 470)
    lerp = expr(mat, unreal.MaterialExpressionLinearInterpolate, -450, 600)
    link(restN, "", lerp, "A")
    link(tN, "", lerp, "B")
    link(ab, "", lerp, "Alpha")

    mat.set_editor_property("tangent_space_normal", False)
    if MEL.connect_material_property(lerp, "", unreal.MaterialProperty.MP_NORMAL):
        log("解析法线已接好（已关闭 Tangent Space Normal）")
    else:
        warn("Normal 引脚连接失败（可能已有法线贴图占用），请手动把 Lerp 接到 Normal")


# ---------------------------------------------------------------- 主流程

def setup(mat, with_normal, label):
    if mat is None:
        warn(label + ": 未找到材质，跳过")
        return
    path = mat.get_path_name()
    if not path.startswith("/Game/"):
        warn("{}: 材质 {} 不在 /Game 下（疑似共享/插件模板），跳过以免污染全工程。".format(label, path))
        warn("       —— 该材质需要单独建一个工程内材质并加 WPO，详见 tools/FoldingDoor_WPO_Material_Setup.md")
        return
    log("{}: 处理材质 {}".format(label, path))
    purge_fold_nodes(mat)
    eff_cur = scalar_param(mat, EFF, 1.0, -1300, 40)
    eff_prev = scalar_param(mat, EFF_PREV, 1.0, -1300, 120)
    add_wpo(mat, eff_cur, eff_prev)
    if with_normal:
        add_normal(mat, eff_cur)
    MEL.recompile_material(mat)
    unreal.EditorAssetLibrary.save_loaded_asset(mat)
    log(label + ": 完成并保存")


def main():
    doors = find_door_actors()
    if not doors:
        warn("当前关卡未找到 DoubleAccordionFoldingDoor Actor，请打开含折叠门的关卡后重试。")
        return
    cloth_mi, wood_mi = get_section_materials(doors[0])
    cloth = resolve_base_material(cloth_mi)
    wood = resolve_base_material(wood_mi)
    log("布料材质槽0 = {}".format(cloth.get_path_name() if cloth else None))
    log("木材材质槽1 = {}".format(wood.get_path_name() if wood else None))
    setup(cloth, True, "布料")
    setup(wood, False, "木材")
    log("完成。C++ 已编译后：开 r.Velocity.EnableVertexDeformation，按方向键验证纹理清晰。")


main()
