@echo off
chcp 65001 >nul
title UnrealMCP 引擎级安装工具

echo ================================================
echo    UnrealMCP 引擎级安装工具
echo    将插件安装到 UE 引擎目录
echo    安装后所有 UE 项目都可以使用
echo ================================================
echo.

:: 检查管理员权限
net session >nul 2>&1
if %errorlevel% neq 0 (
    echo 警告: 需要管理员权限才能安装到引擎目录
    echo 请右键选择 "以管理员身份运行"
    echo.
    pause
    exit /b 1
)

:: 查找 UE 引擎目录
set "UE_BASE=C:\Program Files\Epic Games"

if not exist "%UE_BASE%" (
    echo 错误: UE 引擎目录不存在!
    echo 路径: %UE_BASE%
    pause
    exit /b 1
)

:: 列出可用的 UE 版本
echo 可用的 UE 引擎版本:
echo.

set "FOUND_UE=0"
for /d %%e in ("%UE_BASE%\UE_*") do (
    set "VERSION=%%~nxe"
    if exist "%%e\Engine\Binaries\Win64\UnrealEditor.exe" (
        echo   [!VERSION!]
        set "FOUND_UE=1"
    )
)

if "%FOUND_UE%"=="0" (
    echo 未找到任何 UE 引擎安装
    pause
    exit /b 1
)

echo.
set /p SELECTED_VERSION="请输入 UE 版本号 (如 5.7): "

set "UE_ENGINE_DIR=%UE_BASE%\UE_%SELECTED_VERSION%\Engine"
if not exist "%UE_ENGINE_DIR%" (
    echo 错误: 指定版本的 UE 引擎目录不存在!
    echo 路径: %UE_ENGINE_DIR%
    pause
    exit /b 1
)

set "UE_PLUGINS_DIR=%UE_ENGINE_DIR%\Plugins"
set "SOURCE_DIR=%~dp0UnrealMCP"
set "TARGET_DIR=%UE_PLUGINS_DIR%\UnrealMCP"

:: 检查源插件是否存在
if not exist "%SOURCE_DIR%\UnrealMCP.uplugin" (
    echo 错误: 源插件文件不存在!
    echo 路径: %SOURCE_DIR%
    pause
    exit /b 1
)

:: 创建引擎插件目录
if not exist "%UE_PLUGINS_DIR%" (
    echo 创建引擎插件目录...
    mkdir "%UE_PLUGINS_DIR%"
)

:: 备份旧版本（如果存在）
if exist "%TARGET_DIR%" (
    echo.
    echo 发现已安装版本，正在备份...
    set "BACKUP_DIR=%UE_PLUGINS_DIR%\UnrealMCP.bak.%date:~0,4%%date:~5,2%%date:~8,2%.%time:~0,2%%time:~3,2%%time:~6,2%"
    set "BACKUP_DIR=!BACKUP_DIR: =0!"
    move /Y "%TARGET_DIR%" "!BACKUP_DIR!" >nul
    echo 已备份到: !BACKUP_DIR!
)

:: 复制插件
echo.
echo 正在安装到引擎目录...
echo 源: %SOURCE_DIR%
echo 目标: %TARGET_DIR%
echo.

xcopy /E /I /Y /Q /H /R /O "%SOURCE_DIR%" "%TARGET_DIR%"
if errorlevel 1 (
    echo.
    echo 错误: 复制失败!
    pause
    exit /b 1
)

echo.
echo ================================================
echo    引擎级安装成功!
echo ================================================
echo.
echo UnrealMCP 插件已安装到 UE %SELECTED_VERSION% 引擎
echo 现在所有使用 UE %SELECTED_VERSION% 的项目都可以使用此插件
echo.
echo 使用方法:
echo 1. 在 UE 编辑器中，菜单 Edit ^> Plugins
echo 2. 在 Project 分页下找到 "UnrealMCP"
echo 3. 勾选启用插件
echo 4. 重启 UE 编辑器
echo.
echo 连接 MCP:
echo   在项目目录运行: claude
echo   或使用 "启动UE并连接MCP.bat"
echo.
echo 注意: 如果要卸载，请手动删除目录:
echo   %TARGET_DIR%
echo.
pause