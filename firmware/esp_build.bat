@echo off
setlocal
echo ============================================================
echo   TEAM DISTRO - ESPRESSIF ESP-IDF NATIVE BUILD
echo ============================================================
set "IDF_TOOLS_PATH=C:\Espressif\tools"
set "IDF_PATH=C:\esp\v5.2.7\esp-idf"
set "IDF_PYTHON_ENV_PATH=C:\Espressif\tools\python\v5.2.7\venv"

set "PATH=C:\Espressif\tools\xtensa-esp-elf\esp-13.2.0_20250707\xtensa-esp-elf\bin;C:\Espressif\tools\ninja\1.12.1;C:\Espressif\tools\ccache\4.12.1\ccache-4.12.1-windows-x86_64;C:\Espressif\tools\idf-exe\1.0.3;%IDF_PYTHON_ENV_PATH%\Scripts;%PATH%"

cd /d "%~dp0"
call "%IDF_PYTHON_ENV_PATH%\Scripts\python.exe" "%IDF_PATH%\tools\idf.py" build
if %ERRORLEVEL% EQU 0 (
    echo.
    echo ============================================================
    echo   BUILD SUCCESSFUL!
    echo   Binary: %~dp0build\unverified_controller.bin
    echo   To flash, run: esp_flash.bat
    echo ============================================================
) else (
    echo.
    echo [ERROR] Build failed with exit code %ERRORLEVEL%
)
endlocal
