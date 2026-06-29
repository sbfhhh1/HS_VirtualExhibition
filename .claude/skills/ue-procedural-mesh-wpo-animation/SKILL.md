---
name: ue-procedural-mesh-wpo-animation
description: UE5 程序化网格(ProceduralMeshComponent)做形变/折叠动画时纹理被 TSR/TAA 糊成一团的根因与正解。涵盖 WPO 驱动形变、PreviousFrameSwitch 运动矢量、停止归零、解析法线、刚体部件用独立组件 Transform、Substance 材质避坑、UE Python 建材质、Live Coding 编译限制。当遇到「程序化资产实时运动时纹理模糊、停止后清晰」或要给 ProcMesh 做动画时使用。
---

# UE5 程序化网格 WPO 动画与 TSR 去模糊

本 skill 沉淀自 `BP_DoubleAccordionFoldingDoor`(对开折叠门)的完整调试。
源码示例：`Source/MCPGameProject/DoubleAccordionFoldingDoorActor.{h,cpp}`，
材质脚本/文档：`tools/build_folding_door_materials.py`、`tools/FoldingDoor_WPO_Material_Setup.md`。

## 核心症状 → 根因

**症状**：程序化网格在实时运行、播放形变动画时纹理模糊；动画一停或在编辑器里立刻清晰。

**根因**：用 CPU 每帧重建/更新 ProcMesh 顶点来做动画。表面在屏幕上真实移动，但
`UProceduralMeshComponent` 对「逐帧顶点形变」**不输出运动矢量(motion vector)**。
TSR/TAA 靠运动矢量把上一帧累积的高频细节重投影到当前位置；没有运动矢量 → 重投影错位 →
高频纹理(文字/图案)无法收敛 → 糊。停下后几帧收敛 → 清晰。这不是后处理调色/光照问题。

> 用 `UpdateMeshSection`(不重建代理)只能避免 Clear+Create 型闪烁，**无法**解决缺运动矢量的糊。

## 正解架构：静态拓扑 + 材质 WPO

1. **只建一次静止位姿网格**（拓扑、顶点位置恒定），动画完全交给材质 World Position Offset。
2. **把形变系数烘焙进 UV 通道**（用 `CreateMeshSection_LinearColor` 才能带 UV1/UV2）：
   - 例：UV1=(Cx, Cy) 折叠系数；UV2=(折面坡度, 正反面符号 ny) 给解析法线。
3. **每帧只用 DMI 推标量参数**（如 `EffectiveOpen`），不重建网格 → 拓扑稳定。

```cpp
// 静止网格只在结构参数变化时重建；动画帧只调 SetScalarParameterValue
DoorMID = ProceduralMesh->CreateAndSetMaterialInstanceDynamicFromMaterial(0, Mat);
// Tick / 每帧：
DoorMID->SetScalarParameterValue("EffectiveOpen", eo);
```

## 关键陷阱 1：DMI 标量参数驱动的 WPO 不产生运动矢量

最容易踩、最难想到的一条。**引擎计算「上一帧 WPO」时用的是当前帧的材质参数值**
（材质参数不会逐帧历史化），于是 `WPO_当前 - WPO_上一帧 = 0` → 没有运动矢量 → 照糊。
（用 `Time` 节点驱动的 WPO 能自动算速度，**参数驱动的不能**。）

**正解：`PreviousFrameSwitch` + 一个「上一帧」参数。**
- 材质：当前分支 `WPO(EffectiveOpen)`、上一帧分支 `WPO(EffectiveOpenPrev)`，喂进
  `PreviousFrameSwitch`(Current/Previous) → World Position Offset。
- C++ 每帧同时推两个值：

```cpp
const float eo = GetEffectiveOpenAmount();
const float prev = (LastEffectiveOpen < 0.f) ? eo : LastEffectiveOpen; // 首帧 prev=cur
MID->SetScalarParameterValue("EffectiveOpenPrev", prev);
MID->SetScalarParameterValue("EffectiveOpen",     eo);
LastEffectiveOpen = eo;
```
- 项目设置 `r.Velocity.EnableVertexDeformation>=1`（Output velocities due to vertex deformation）。

## 关键陷阱 2：动画停止后反而糊

停止动画后若不再更新参数，`EffectiveOpenPrev` 与 `EffectiveOpen` 被**冻结在两个不同值**，
速度 pass 每帧算出**恒定假位移** → TSR 误判表面一直在动 → 持续糊（表现为松开按键反而糊）。

**正解：停止时把上一帧对齐到当前帧（prev=cur），运动矢量归零。**
```cpp
void StopDoorAnimation() {
    AnimationDirection = 0.f;
    LastEffectiveOpen = GetEffectiveOpenAmount();
    ApplyOpenAmountToMaterial(); // 令 prev == cur
}
```

## WPO 不改法线 → 解析法线

WPO 只移动顶点、不改顶点法线，形变中折面没有受光变化。可在材质里**解析推导**：
- 烘焙每个折面的坡度系数与正反面符号到 UV2（坡度需相邻列差分，单顶点位置反推不出来，
  且 ProcMesh 折面顶点不共享，故在建面时按面写入）。
- 材质：`N_local = normalize(ny * (-g, 1, 0))`，`g = (sqrt(1-eo²)/eo) * 坡度系数`；
  非折叠面(ny=0)用 `lerp` 回退到 `VertexNormalWS`；关掉 Tangent Space Normal，
  Custom 输出经 `TransformVector(Local→World)` 接 Normal。

## 刚体部件：用独立组件 + Transform，而非 WPO

对**只做刚性平移/旋转**的部件（如门框木条），**不要**用材质 WPO，改用独立子组件 +
`SetRelativeLocation`：
- 组件 Transform 移动**天然产生正确运动矢量**（无需 PreviousFrameSwitch），不糊；
- **不依赖材质**，可保留任意原材质（尤其 Substance，见下）；
- 几何居中建一次，逐帧只改 RelativeLocation。

```cpp
InnerBarLeft = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("InnerBarLeft"));
InnerBarLeft->SetupAttachment(ProceduralMesh);
// 每帧：InnerBarLeft->SetRelativeLocation(FVector(x(eo),0,0));
```

## Substance 材质避坑

- **绝不给共享/插件材质(`/Substance/...` 模板)加节点**——会污染全工程所有用它的材质。
  脚本必须 guard：只改 `/Game/` 下材质。
- **不要 reparent Substance 材质实例**：引擎会拒绝把 DMID 当父级
  (`... is not a valid parent for MaterialInstanceConstant`)，且 Substance 实例体系特殊，
  reparent 会丢贴图（材质变黑/灰）。
- 需要给 Substance 木材加动画时，优先用上面的「独立组件 Transform」方案，材质原样不动。

## UE Python 建材质要点（MaterialEditingLibrary）

- 用 `Custom` HLSL 节点把数学压成少数节点，**降低连线 pin 名出错面**。
- **幂等**：先 purge 旧节点（按 ScalarParameter 名 / Custom description 前缀识别）再重建。
- **从关卡 Actor 读真实在用材质**（`GetComponentByClass(ProceduralMeshComponent).get_material(i)`
  → `get_base_material()`），不要猜资产路径。
- 注意 `get_material(i)` 在 C++ 已包 DMI 时返回的是运行时 MID，其 `get_base_material()` 直接
  跳到根 UMaterial（会跳过中间 Substance 实例）——别据此复制根模板，会丢贴图。
- pin 名跨版本可能不同（如 PreviousFrameSwitch 的 "Current Frame"/"CurrentFrame"），逐个候选试。

## Live Coding / 编译限制（务必牢记）

- **Live Coding 只能处理函数体改动**。新增/删 `UPROPERTY`、改函数签名、改 `USTRUCT`/对象布局
  → 必须**关闭编辑器**做完整编译。
- 完整编译命令（编辑器与 `LiveCodingConsole.exe` 必须全部退出，否则报
  `Unable to build while Live Coding is active` 或 `Target is up to date`）：
  ```
  "<UE>/Engine/Build/BatchFiles/Build.bat" <Project>Editor Win64 Development -Project="<.uproject 绝对路径>" -WaitMutex
  ```
- 残留 `LiveCodingConsole.exe` / 卡住的 `UnrealEditor.exe` 会锁住模块 DLL 与资产文件；
  确认进程全退再编译/`git checkout` 资产。
- 损坏的 `Engine/Intermediate/LiveCoding.json`（`missing LinkerPath`）：完整编译会重建，可删它重生。

## 排错顺序速查

1. 运动中糊、停下清晰 → 没运动矢量。先确认动画是不是「每帧重建顶点」。
2. 改 WPO 后仍糊 → 多半是参数驱动 WPO 缺 PreviousFrameSwitch + 上一帧参数。
3. 松开按键后反而糊 → 停止时没把 prev 归零到 cur。
4. 折面无明暗 → 补解析法线。
5. 刚体部件糊或材质难搞 → 改独立组件 Transform。
6. 材质变黑/灰 → 多半动了 Substance/共享模板或 reparent 失败，回退共享资产、改用组件方案。
