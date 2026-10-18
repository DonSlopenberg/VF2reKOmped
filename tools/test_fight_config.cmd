@echo off
setlocal
call "C:\Program Files\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
cd /d "%~dp0.."
"C:\Program Files\LLVM\bin\clang++.exe" -std=c++23 -Inative/src native/tests/fight_config_test.cpp -o analysis/fight_config_test.exe
if errorlevel 1 exit /b 1
analysis\fight_config_test.exe analysis/fight-config-check
