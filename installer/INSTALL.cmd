@echo off
rem Virtua Fighter 2 reKOmped - installer.
rem   INSTALL.cmd [source] [destination]
rem source: "Virtua Fighter 2.rar" or 65D8FAC10B21FA185AAB93607663594CB568C78658
rem (default: searched next to this file). You can also drop the file onto it.
setlocal
if not "%~1"=="" set "VF2_SOURCE=%~1"
if not "%~2"=="" set "VF2_DEST=%~2"
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0installer\install.ps1"
set "VF2_RC=%errorlevel%"
rem Keep the window open when started by double-click or drag and drop.
if not defined VF2_NO_PAUSE echo %cmdcmdline% | find /i "%~nx0" >nul && pause
exit /b %VF2_RC%
