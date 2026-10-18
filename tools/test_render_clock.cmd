@echo off
setlocal
cd /d "%~dp0.."
call "C:\Program Files\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
"C:\Program Files\LLVM\bin\clang++.exe" -std=c++20 -UNDEBUG -Wall -Wextra -Werror native\tests\render_clock_test.cpp -o analysis\test_render_clock.exe
if errorlevel 1 exit /b 1
analysis\test_render_clock.exe
exit /b %errorlevel%
