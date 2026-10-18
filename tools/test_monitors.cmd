@echo off
setlocal
call "C:\Program Files\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
cd /d "%~dp0.."
"C:\Program Files\LLVM\bin\clang++.exe" -std=c++23 -Inative/src native/tests/monitors_test.cpp -luser32 -o analysis/monitors_test.exe
if errorlevel 1 exit /b 1
analysis\monitors_test.exe
