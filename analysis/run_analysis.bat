@echo off
setlocal
echo ============================================================
echo   TEAM DISTRO - MATLAB Flow & Calibration Analysis
echo ============================================================
matlab -batch "ft01_calibration_plots; valve_position_plot;"
if %ERRORLEVEL% EQU 0 (
    echo.
    echo Plots successfully generated in media/
) else (
    echo.
    echo [ERROR] MATLAB execution failed. Ensure MATLAB is installed on PATH.
)
endlocal
