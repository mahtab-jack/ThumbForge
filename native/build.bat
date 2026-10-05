@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat"
if errorlevel 1 exit /b 1

set CMAKE="C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"

%CMAKE% -B "%~dp0build_nmake" -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release -S "%~dp0."
if errorlevel 1 exit /b 1

%CMAKE% --build "%~dp0build_nmake"
if errorlevel 1 exit /b 1
