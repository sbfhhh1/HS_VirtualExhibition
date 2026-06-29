# 对开折叠门 WPO 材质配方

配合 `Source/MCPGameProject/DoubleAccordionFoldingDoorActor.*`：C++ 端只建一次静止网格，
折叠动画由材质 **World Position Offset (WPO)** 驱动，从而获得正确运动矢量、TSR/TAA 不再糊纹理。

C++ 已把折叠系数烘焙进顶点 **UV1**：

- `UV1.x = Cx`：X 压缩系数（左叶 `+Distance`、右叶 `-Distance`；内侧木条 `±TextileWidth`、外侧 `0`）
- `UV1.y = Cy`：折叠深度系数（木条为 0）

每帧 C++ 只推一个标量参数 **`EffectiveOpen`**（名字必须完全一致）。`EffectiveOpen=1` = 全展平静止态。

---

## ① Material Function：`MF_FoldingDoorWPO`

新建 Material Function（建议放 `Content/TransformationVFX/SM2SM/Door/`），勾选
**Expose to Library**，输出一个 Functon Output（命名 `WPO`，类型 Float3）。内部连线：

```
ScalarParameter "EffectiveOpen"  (Default 1.0)            ──► eo

TexCoordinate[CoordinateIndex = 1]                        ──► UV1
   UV1 ─ ComponentMask(R) ──► Cx
   UV1 ─ ComponentMask(G) ──► Cy

// X 偏移：Cx * (eo - 1)
(eo) ─ Subtract(B=1.0) ──► (eo-1)
Cx ─ Multiply(other = eo-1) ──► offsetX

// Y 偏移：Cy * sqrt( saturate(1 - eo*eo) )
eo ─ Multiply(eo) ──► eo2
(1.0) ─ Subtract(B = eo2) ──► (1-eo2)
(1-eo2) ─ Saturate ──► sat
sat ─ SquareRoot ──► s
Cy ─ Multiply(other = s) ──► offsetY

// 组成本地空间位移并转到世界空间
MakeFloat3 / AppendVector( X=offsetX, Y=offsetY, Z=0.0 ) ──► offsetLocal
offsetLocal ─ TransformVector(Source=Local Space, Dest=World Space) ──► Output "WPO"
```

要点：
- `TransformVector` 必须是 **Local→World 向量变换**（顶点/UV1 都在组件本地空间，X=门宽方向、Y=进深、Z=高）。
- `ScalarParameter` 放在 MF 内部即可：任何引用该 MF 的材质都会自动带出 `EffectiveOpen` 参数，C++ 的 `DoorMID/WoodMID` 据名写入。

### 在两个材质里引用

- **布料材质**（`DoorMaterial`，即印有展览文字那张）：拖入 `MF_FoldingDoorWPO`，
  `WPO` 输出 → 材质的 **World Position Offset** 引脚。
- **木材材质**（`wood`，`WoodMaterial`）：同样接整段。木条 `Cy=0`，所以只在 X 平移、
  不产生折叠深度，行为与原 CPU 版一致。

> `EffectiveOpen=1` 时 `offsetX=Cx*0=0`、`offsetY=Cy*0=0`，位移恒为零——
> 静止态/编辑器与改造前完全相同（清晰）。

---

## ② 折叠解析法线（让折面受光正确，仅布料材质）

WPO 只移动顶点、不改顶点法线，所以默认折叠中折面没有明暗变化。C++ 已把每个折面的
**坡度系数**和**正反面符号**烘焙进顶点 **UV2 = (g_signed, ny)**：

- `UV2.x = g_signed`：折面深度沿表面的坡度系数（已折进左右叶方向），实际坡度 = `g_signed * sqrt(1-eo²)/eo`
- `UV2.y = ny`：`+1` 正面 / `-1` 背面 / `0` 非布料面（侧边、上下封口）

布料折面在局部空间的解析法线 = `normalize( ny * (-g_actual, 1, 0) )`，其中 `g_actual = (S/eo)*g_signed`，
`S = sqrt(1-eo²)`。`eo=1`（静止）时 `g_actual=0` → 法线为 `(0, ny, 0)`，与静止法线一致。

材质连线（**只加布料材质**；木材不折、`Cy=0`，无需改）：

1. 取消勾选材质的 **Tangent Space Normal**（改用世界空间法线）。
2. 加一个 **Custom** 节点（局部空间折叠法线）：
   - Output Type：`CMOT Float3`
   - 输入：`Eo`（← `EffectiveOpen` 标量参数）、`FN`（← `TexCoord[2]`）
   - 代码：
     ```hlsl
     float ny = FN.y;
     float S  = sqrt(saturate(1.0 - Eo*Eo));
     float eoSafe = max(Eo, 0.001);
     float g  = (S / eoSafe) * FN.x;
     return normalize(float3(ny * (-g), ny, 0.0));
     ```
3. Custom 输出 → **TransformVector(Local→World)** → 得世界空间布料法线 `clothN`。
4. 计算遮罩：`TexCoord[2]` → ComponentMask(G) → **Abs** = `mask`（布料面 1 / 其余 0）。
5. **Lerp**( A = `VertexNormalWS`（静止法线）, B = `clothN`, Alpha = `mask` ) → 材质 **Normal**。

> 这样侧边/封口（`ny=0`）回退到静止顶点法线，只有布料折面套用解析法线，左右叶/正反面符号都已在
> `g_signed`/`ny` 里处理好，无需手动调方向。若整体受光仍反，把 Custom 里 `ny * (-g)` 的负号去掉试试。

> 自动版见 `tools/build_folding_door_materials.py`：在编辑器 Output Log 的 Python 命令框运行
> `py "<repo>/tools/build_folding_door_materials.py"`，它会自动解析关卡里门 Actor 槽 0/1 的材质，
> 把上面 ① ② 一次性连好并保存。若某条连线打印了警告，按本文件对应步骤手动补上即可。

---

## ③ 必须开启：运动矢量（去模糊的真正开关）

**Project Settings → Rendering → Optimizations → "Output velocities due to vertex deformation"**
设为 **On**（默认 Auto 一般也可，保险设 On）。

这是速度/运动矢量通道，不是后处理调色——正是它让 TSR/TAA 拿到 WPO 的运动矢量，
动画时纹理才不会被时间抗锯齿糊掉。

---

## 验证清单

- [ ] 两个材质都接了 `MF_FoldingDoorWPO` 到 WPO，`EffectiveOpen` 参数名拼写一致。
- [ ] 开启 vertex deformation 速度输出。
- [ ] 运行后按 上/下 方向键：门折叠/展平，**布料文字与纹样全程清晰**（不再糊）。
- [ ] 静止态、编辑器状态与改造前一致。
- [ ] （加了②后）折面在折叠过程中有合理明暗。
