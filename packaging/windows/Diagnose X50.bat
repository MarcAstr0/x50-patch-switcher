@echo off
rem Runs the X50 diagnose from the folder this file is in (the exe sits next to it).
rem Output goes to a new folder on the Desktop, which is then opened in Explorer.
setlocal
set "STAMP="
rem The date comes from PowerShell so it does not depend on the regional settings.
for /f %%i in ('powershell -NoProfile -Command "Get-Date -Format yyyy-MM-dd_HHmm"') do set "STAMP=%%i"
if not defined STAMP set "STAMP=manual"
set "OUT=%USERPROFILE%\Desktop\X50-diagnose-%STAMP%"
echo.
echo Diagnostico del X50.
echo Enciende el X50 y conecta el cable USB. Esto tarda menos de un minuto.
echo No cierres esta ventana.
echo.
"%~dp0x50-patch-switcher.exe" --diagnose "%OUT%"
set "RC=%ERRORLEVEL%"
echo.
rem Exit code 0 = every bank read; anything else still leaves a useful report.txt.
if "%RC%"=="0" (
  echo Listo. Envia la carpeta que se abrio.
) else (
  echo Hubo un problema, pero igual envia la carpeta que se abrio.
)
if exist "%OUT%" start "" explorer "%OUT%"
echo.
pause
