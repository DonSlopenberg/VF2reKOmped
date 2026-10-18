@echo off
setlocal
call "C:\Program Files\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
if errorlevel 1 exit /b 1
set "PATH=C:\Program Files\LLVM\bin;C:\Program Files\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin;C:\Program Files\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja;%PATH%"
cd /d "%~dp0"
clang++ --version
cmake --version
cmake --preset win-amd64-relwithdebinfo "-DCMAKE_PREFIX_PATH=%~dp0..\tools\rexglue-sdk-0.10.0\win-amd64"
if errorlevel 1 exit /b 1
if "%~1"=="launcher" goto launcher
cmake --build --preset win-amd64-relwithdebinfo --parallel 16
exit /b %errorlevel%
:launcher
cmake --build --preset win-amd64-relwithdebinfo --target vf2_launcher --parallel 16
exit /b %errorlevel%
