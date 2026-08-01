@echo off
setlocal enabledelayedexpansion

if defined INCLUDE goto :haveenv

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "!VSWHERE!" set "VSWHERE=%ProgramFiles%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "!VSWHERE!" (
    echo Could not find vswhere.exe; is Visual Studio installed?
    exit /b 1
)

for /f "usebackq delims=" %%i in (`"!VSWHERE!" -latest -property installationPath`) do set "VSPATH=%%i"
if not defined VSPATH (
    echo vswhere reported no Visual Studio installation.
    exit /b 1
)

call "!VSPATH!\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
if not defined INCLUDE (
    echo vcvars64.bat did not initialise the environment.
    exit /b 1
)

:haveenv
cd /d "%~dp0"

if not exist "build\CMakeCache.txt" (
    cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release || exit /b 1
)

cmake --build build --config Release || exit /b 1

echo.
echo Staged in dist\SKSE\Plugins:
dir /b dist\SKSE\Plugins
