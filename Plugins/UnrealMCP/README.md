# UnrealMCP 插件说明

为 Claude Code 桌面端创建的 UE5 MCP 连接器插件。

## 目录结构

```
Plugins/UnrealMCP/
├── UnrealMCP.uplugin          # 插件配置文件
├── Source/UnrealMCP/         # 插件源代码
│   ├── Public/               # 头文件
│   ├── Private/              # 实现文件
│   └── UnrealMCP.Build.cs    # 构建配置
├── 安装到其他项目.bat         # 将插件安装到其他 UE 项目
├── 引擎级安装.bat            # 将插件安装到 UE 引擎（需要管理员）
└── 启动UE并连接MCP.bat      # 一键启动脚本（通用版）
```

## 安装方式

### 方式一：引擎级安装（推荐）

将插件安装到 UE 引擎目录，所有 UE 项目都能使用。

1. 右键 `引擎级安装.bat` → **以管理员身份运行**
2. 选择 UE 版本号（如 5.7）
3. 等待安装完成

安装后无需在每个项目中单独配置。

### 方式二：项目级安装

将插件复制到特定项目的 Plugins 目录。

1. 修改 `安装到其他项目.bat` 中的目标路径，或直接拖拽目标项目文件夹到脚本上
2. 运行脚本

## 使用方法

### 快速开始

1. **确保 UE 编辑器已启动**，且项目已加载
2. **启用插件**：Edit → Plugins → Project → 找到 "UnrealMCP" → 勾选 Enabled
3. **启动 MCP Server**：插件设置中点击 "Start"
4. **运行 Claude Code**：
   - 在 UE 项目目录运行 `claude`
   - 或 Claude Code 桌面客户端中选择项目

### 使用一键启动脚本

双击 `启动UE并连接MCP.bat` 即可：
- 自动检测 UE 项目
- 等待 MCP 服务器就绪
- 提示可以运行 Claude Code

## Claude Code 使用示例

连接成功后，可以使用以下命令：

```
# 场景查询
列出场景中的所有 Actor
查找名称包含 Player 的 Actor
查看 Actor Cube1 的属性

# 创建 Actor
创建一个名为 MyBox 的立方体
在位置 (100, 200, 300) 创建一个点光源

# 蓝图操作
创建一个名为 MyCharacter 的蓝图（继承自 Pawn）
给 MyCharacter 添加 StaticMeshComponent
设置 StaticMesh 为 /Engine/BasicShapes/Cube.Cube
编译蓝图

# 蓝图节点编辑
在 MyCharacter 中添加 EventTick 事件节点
添加 PrintString 函数调用节点
```

## 工具列表

### 场景查询
- `get_actors_in_level` - 获取关卡中所有 Actor
- `find_actors_by_name` - 按名称搜索 Actor
- `get_actor_properties` - 获取 Actor 详细属性

### Actor 操作
- `spawn_actor` - 创建 Actor
- `delete_actor` - 删除 Actor
- `set_actor_transform` - 设置变换
- `set_actor_property` - 设置属性
- `focus_viewport` - 聚焦视口

### 蓝图操作
- `create_blueprint` - 创建蓝图
- `add_component_to_blueprint` - 添加组件
- `set_component_property` - 设置组件属性
- `set_static_mesh_properties` - 设置静态网格
- `set_physics_properties` - 设置物理属性
- `set_blueprint_property` - 设置蓝图属性
- `compile_blueprint` - 编译蓝图
- `spawn_blueprint_actor` - 生成蓝图 Actor
- `set_pawn_properties` - 设置 Pawn 属性
- `create_box` - 创建立方体

### 蓝图节点编辑
- `add_blueprint_event_node` - 添加事件节点
- `add_blueprint_function_node` - 添加函数节点
- `add_blueprint_variable` - 添加变量
- `add_blueprint_get_self_component_reference` - 获取组件引用
- `connect_blueprint_nodes` - 连接节点
- `find_blueprint_nodes` - 查找节点
- `add_blueprint_self_reference` - 添加自身引用

### 项目信息
- `get_unreal_project_info` - 获取项目信息

## 故障排查

### 端口 55557 连接失败
```
错误: 无法连接到 UE 编辑器 (127.0.0.1:55557)
```
解决：
1. 确认 UE 编辑器已完全启动
2. 确认 UnrealMCP 插件已启用（Edit → Plugins）
3. 确认插件设置中 MCP Server 已 Start
4. 检查防火墙是否阻止了端口

### Claude Code 未检测到 UnrealMCP
```
错误: 未找到 UE 项目 (.uproject 文件)
```
解决：
1. 确认在 UE 项目目录中运行 `claude`
2. 确认目录中存在 `.uproject` 文件
3. 或者使用 Claude Code 桌面客户端选择项目

### 插件编译错误
如果在其他 UE 版本中使用，可能需要重新编译：
1. 删除 `Intermediate` 和 `Binaries` 目录
2. 在 UE 编辑器中重新生成项目文件
3. 编译插件

## 技术架构

```
Claude Code  ──stdio──▶  unreal_engine_mcp_bridge.js  (Node 桥接)
                                        │  TCP 127.0.0.1:55557
                                        ▼
                            UnrealMCP 插件（UE 编辑器内监听）
```

桥接脚本使用 `@modelcontextprotocol/sdk`，提供 26 个工具。

## 版本信息

- 插件版本：1.0
- 桥接版本：1.0.0-global
- 支持引擎：UE 5.x
- 支持平台：Win64