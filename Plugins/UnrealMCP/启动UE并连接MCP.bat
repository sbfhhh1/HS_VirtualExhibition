@echo off
chcp 65001 >nul
title UE5 MCP 连接器

:: ================================================
:: UE5 MCP 一键启动脚本（通用版）
:: 自动检测当前目录的 UE 项目并连接 MCP
:: ================================================

setlocal enabledelayedexpansion

:: 配置区域 - 根据需要修改
set "UE_EXE=C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor.exe"
set "MCP_PORT=55557"
set "MAX_WAIT_SECONDS=180"

:: 自动检测项目目录（向上查找 .uproject）
set "CURRENT_DIR=%~dp0"
set "PROJECT_DIR="

:: 如果在 Plugins 目录，从父目录开始查找
if "%CURRENT_DIR:~-9%"=="Plugins\" (
    set "CHECK_DIR=%~dp0.."
) else (
    set "CHECK_DIR=%CURRENT_DIR%"
)

:: 向上查找 .uproject 文件
for /f "tokens=*" %%i in ('powershell -Command "& {
        $dir = [System.IO.DirectoryInfo]::new('%CHECK_DIR:\=\\%')
        while ($dir) {
            $uproj = $dir.GetFiles('*.uproject')
            if ($uproj.Count -gt 0) {
                $uproj[0].DirectoryName
                break
            }
            $dir = $dir.Parent
        }
    }"') do set "PROJECT_DIR=%%i"

if "%PROJECT_DIR%"=="" (
    echo 错误: 未找到 UE 项目（.uproject 文件）
    echo 请在 UE 项目目录中运行此脚本
    pause
    exit /b 1
)

echo ================================================
echo    UE5 MCP 连接器
echo ================================================
echo.
echo 项目: %PROJECT_DIR%
echo 端口: %MCP_PORT%
echo.

:: 查找 .uproject 文件名
for %%f in ("%PROJECT_DIR%\*.uproject") do set "PROJECT_FILE=%%~nxf"
echo 找到项目: %PROJECT_FILE%

:: 检查 UE 是否正在运行
echo.
echo 检查 UE 编辑器运行状态...
netstat -ano | findstr ":%MCP_PORT%" >nul
if errorlevel 1 (
    echo UE MCP 服务器未运行
) else (
    echo UE MCP 服务器已在运行!
    echo.
    echo 可以开始使用 Claude Code:
    echo   cd /d "%PROJECT_DIR%"
    echo   claude
    echo.
    echo 或者在 Claude Code 桌面客户端中选择此项目
    echo.
    pause
    exit /b 0
)

:: 检查 UE 编辑器进程
tasklist /FI "IMAGENAME eq UnrealEditor.exe" | findstr /I "UnrealEditor" >nul
if errorlevel 1 (
    echo.
    echo UE 编辑器未运行，正在启动...
    
    if not exist "%UE_EXE%" (
        echo 错误: UE 可执行文件不存在!
        echo 路径: %UE_EXE%
        echo.
        echo 请修改脚本顶部的 UE_EXE 路径，或手动启动 UE 编辑器
        echo.
        set /p MANUAL_START="手动启动 UE 后按 Y 继续: "
        if /i not "!MANUAL_START!"=="Y" exit /b 1
    ) else (
        start "" "%UE_EXE%" "%PROJECT_DIR%\%PROJECT_FILE%"
        echo 已启动 UE，请等待编辑器完全加载...
    )
) else (
    echo UE 编辑器正在运行
    echo 请确保项目已加载且 UnrealMCP 插件已启用
)

:: 等待 MCP 端口就绪
echo.
echo 等待 MCP 服务器就绪（最多 %MAX_WAIT_SECONDS% 秒）...
set /a ELAPSED=0
set /a INTERVAL=2

:wait_loop
timeout /t %INTERVAL% /nobreak >nul
set /a ELAPSED+=!INTERVAL!

netstat -ano | findstr ":%MCP_PORT%" >nul
if not errorlevel 1 (
    echo.
    echo ================================================
    echo    MCP 服务器已就绪!
    echo ================================================
    echo.
    echo 可以开始使用 Claude Code:
    echo   cd /d "%PROJECT_DIR%"
    echo   claude
    echo.
    echo 或者在 Claude Code 桌面客户端中选择此项目
    echo.
    pause
    exit /b 0
)

if !ELAPSED! GEQ %MAX_WAIT_SECONDS% (
    echo.
    echo 错误: 等待超时！
    echo.
    echo 请检查:
    echo 1. UE 编辑器是否已完全启动
    echo 2. 项目是否已加载
    echo 3. UnrealMCP 插件是否已启用
    echo    - 菜单: Edit ^> Plugins ^> Project ^> UnrealMCP
    echo    - 勾选 Enabled
    echo 4. 插件设置中 MCP Server 是否已 Start
    echo.
    pause
    exit /b 1
)

echo 等待中... !ELAPSED!/!MAX_WAIT_SECONDS! 秒
goto wait_loop

endlocal