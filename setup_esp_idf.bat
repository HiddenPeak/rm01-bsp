@echo off
:: ESP-IDF Development Environment Setup - 最佳版本
:: 用于rm01-bsp项目的快速环境设置

echo ==========================================
echo ESP-IDF Development Environment Setup
echo rm01-bsp Project
echo ==========================================

:: 设置 ESP-IDF 路径
set "IDF_PATH=C:\Users\sprin\esp\v5.4.1\esp-idf"

:: 检查 ESP-IDF 安装
if not exist "%IDF_PATH%" (
    echo [ERROR] ESP-IDF path not found: %IDF_PATH%
    pause
    exit /b 1
)

echo [INFO] ESP-IDF Path: %IDF_PATH%

:: 检查导出脚本 - 优先使用 .bat 版本
set "EXPORT_BAT=%IDF_PATH%\export.bat"
set "EXPORT_PS1=%IDF_PATH%\export.ps1"

if exist "%EXPORT_BAT%" (
    echo [INFO] Using batch export script...
    call "%EXPORT_BAT%"
) else if exist "%EXPORT_PS1%" (
    echo [INFO] Using PowerShell export script...
    powershell -ExecutionPolicy Bypass -File "%EXPORT_PS1%"
) else (
    echo [ERROR] No export script found
    pause
    exit /b 1
)

if errorlevel 1 (
    echo [ERROR] Failed to setup ESP-IDF environment
    pause
    exit /b 1
)

echo [SUCCESS] ESP-IDF environment setup complete!

:: 验证环境
echo.
echo ==========================================
echo Environment Verification
echo ==========================================

echo [INFO] IDF_PATH: %IDF_PATH%

:: 检查 idf.py 是否可用
idf.py --version >nul 2>&1
if errorlevel 1 (
    echo [WARNING] idf.py command not available
    echo [INFO] You may need to restart your terminal
) else (
    echo [SUCCESS] idf.py is available
    for /f "tokens=*" %%i in ('idf.py --version 2^>^&1') do echo [INFO] Version: %%i
)

:: 检查项目文件
echo.
echo ==========================================
echo Project Status
echo ==========================================

if exist "CMakeLists.txt" (
    echo [SUCCESS] CMakeLists.txt: Found
) else (
    echo [ERROR] CMakeLists.txt: Not found
)

if exist "main" (
    echo [SUCCESS] main directory: Found
) else (
    echo [ERROR] main directory: Not found
)

if exist "build" (
    echo [SUCCESS] build directory: Found
    for /f %%i in ('dir /b build 2^>nul ^| find /c /v ""') do echo [INFO] Build files: %%i
) else (
    echo [INFO] build directory: Not found (will be created on first build)
)

echo.
echo ==========================================
echo Available Commands
echo ==========================================
echo idf.py build          - Build the project
echo idf.py flash          - Flash firmware to device  
echo idf.py monitor        - Monitor serial output
echo idf.py size           - Show size information
echo idf.py clean          - Clean build files
echo idf.py menuconfig     - Open configuration menu
echo idf.py erase-flash    - Erase device flash
echo idf.py app-flash      - Flash app only
echo.
echo ==========================================
echo Quick Start Examples
echo ==========================================
echo First build:    idf.py build
echo Flash device:   idf.py flash monitor
echo Size analysis:  idf.py size-components
echo Clean rebuild:  idf.py clean build
echo.
echo [INFO] Environment ready for ESP32-S3 development!
pause
