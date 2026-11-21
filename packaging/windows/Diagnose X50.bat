@echo off
rem Runs the X50 diagnose from the folder this file is in (the exe sits next to it).
rem Output goes to a new folder on the Desktop, which is then opened in Explorer.
setlocal
if not exist "%~dp0x50-patch-switcher.exe" (
  echo.
  echo No encuentro x50-patch-switcher.exe junto a este archivo.
  echo Primero extrae TODO el zip: clic derecho, "Extraer todo".
  echo Despues abre este archivo desde la carpeta extraida.
  echo.
  pause
  exit /b 1
)
rem Desktop and date come from PowerShell: OneDrive/localised desktops, regional date formats.
set "DESK="
set "STAMP="
for /f "usebackq delims=" %%i in (`powershell -NoProfile -Command "[Environment]::GetFolderPath('Desktop')"`) do set "DESK=%%i"
if not defined DESK set "DESK=%USERPROFILE%\Desktop"
for /f %%i in ('powershell -NoProfile -Command "Get-Date -Format yyyy-MM-dd_HHmmss"') do set "STAMP=%%i"
if not defined STAMP set "STAMP=manual"
rem Never reuse a folder: add -2, -3... if it exists.
set "BASE=%DESK%\X50-diagnose-%STAMP%"
set "OUT=%BASE%"
set /a N=1
:pick
if not exist "%OUT%" goto picked
set /a N+=1
set "OUT=%BASE%-%N%"
goto pick
:picked
echo.
echo Diagnostico del X50.
echo Enciende el X50 y conecta el cable USB. Esto tarda uno o dos minutos.
echo No cierres esta ventana.
echo.
rem The exe has no console of its own; "start /wait" makes cmd wait for it and keep its exit code.
start "" /wait "%~dp0x50-patch-switcher.exe" --diagnose "%OUT%"
set "RC=%ERRORLEVEL%"
echo.
rem Exit code 0 = every bank read; anything else still leaves a useful report.txt.
if exist "%OUT%\report.txt" (
  start "" explorer "%OUT%"
  if "%RC%"=="0" (echo Listo. Envia la carpeta que se abrio.) else (echo Hubo un problema, pero igual envia la carpeta que se abrio.)
) else (
  echo No se pudo crear el informe. Envia una foto de esta ventana a Mario.
)
echo.
pause
