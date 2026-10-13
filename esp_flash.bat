@echo off
setlocal
echo ============================================================
echo   TEAM DISTRO - ESPRESSIF ESP-IDF FLASH ^& MONITOR
echo ============================================================
set "IDF_TOOLS_PATH=C:\Espressif\tools"
set "IDF_PATH=C:\esp\v5.2.7\esp-idf"
set "IDF_PYTHON_ENV_PATH=C:\Espressif\tools\python\v5.2.7\venv"

cd /d "%~dp0controller_firmware"
call "%IDF_PYTHON_ENV_PATH%\Scripts\python.exe" tools\esp_quick.py run
endlocal
