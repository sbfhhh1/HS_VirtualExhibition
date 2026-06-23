@echo off
chcp 65001 >nul
title UnrealMCP 插件安装工具

echo ================================================
echo    UnrealMCP 插件安装工具
echo    将插件复制到其他 UE5 项目
echo ================================================
echo.

:: 检查参数
if "%~1"=="" (
    echo 使用方式:
    echo   安装到其他项目: UnrealMCP安装.bat ^<目标项目路径^>
    echo   示例: UnrealMCP安装.bat "C:\MyUEProjects\MyGame"
    echo.
    echo   或者直接拖拽目标项目文件夹到本脚本上
    echo.
    echo   注意: 目标项目需要先在 UE5 编辑器中打开一次
    echo         以创建 Plugins 目录结构
    echo.
    pause
    exit /b 1
)

set "SOURCE_DIR=%~dp0"
set "TARGET_DIR=%~1"

:: 移除尾部反斜杠
if "%TARGET_DIR:~-1%"=="\" set "TARGET_DIR=%TARGET_DIR:~0,-1%"

:: 检查源插件是否存在
if not exist "%SOURCE_DIR%UnrealMCP.uplugin" (
    echo 错误: 源插件文件不存在!
    echo 路径: %SOURCE_DIR%UnrealMCP.uplugin
    pause
    exit /b 1
)

:: 检查目标目录是否存在
if not exist "%TARGET_DIR%" (
    echo 错误: 目标项目目录不存在!
    echo 路径: %TARGET_DIR%
    pause
    exit /b 1
)

:: 检查目标是否为 UE 项目
set "IS_UE_PROJECT=0"
for %%f in ("%TARGET_DIR%\*.uproject") do set "IS_UE_PROJECT=1"
if "%IS_UE_PROJECT%"=="0" (
    echo 警告: 目标目录似乎不是 UE 项目（未找到 .uproject 文件）
    echo 继续安装可能有问题
    echo.
    set /p CONFIRM="确定继续吗? (y/n): "
    if /i not "%CONFIRM%"=="y" exit /b 1
)

:: 创建 Plugins 目录
if not exist "%TARGET_DIR%\Plugins" (
    echo 创建 Plugins 目录...
    mkdir "%TARGET_DIR%\Plugins"
)

:: 复制插件
echo.
echo 正在复制插件到目标项目...
echo 源: %SOURCE_DIR%UnrealMCP
echo 目标: %TARGET_DIR%\Plugins\
echo.

xcopy /E /I /Y /Q "%SOURCE_DIR%UnrealMCP" "%TARGET_DIR%\Plugins\UnrealMCP"
if errorlevel 1 (
    echo.
    echo 错误: 复制失败!
    pause
    exit /b 1
)

echo.
echo ================================================
echo    安装成功!
echo ================================================
echo.
echo 接下来请:
echo 1. 打开 UE5 编辑器，加载项目: %TARGET_DIR%
echo 2. 在菜单栏点击 Edit ^> Plugins
echo 3. 在 Project 分页下找到 "UnrealMCP"
echo 4. 勾选启用插件
echo 5. 重启 UE 编辑器
echo.
echo 启动连接:
echo   双击运行 "启动UE并连接MCP.bat"
echo   或在项目目录运行: claude
echo.
pause