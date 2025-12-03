@echo off
setlocal

set BUILD_TYPE=%1
if "%BUILD_TYPE%"=="" set BUILD_TYPE=Debug

echo Building Pre-DateGrip (%BUILD_TYPE%)...

REM Check for Ninja
ninja --version >nul 2>&1
if errorlevel 1 (
    echo ERROR: Ninja not found in PATH
    echo Install Ninja: winget install Ninja-build.Ninja
    exit /b 1
)

REM Check for MSVC compiler
cl 2>&1 | findstr /c:"Microsoft" >nul
if errorlevel 1 (
    echo ERROR: MSVC compiler (cl.exe) not found in PATH
    echo Run this script from Developer Command Prompt or run:
    echo   "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
    exit /b 1
)

if not exist build mkdir build

cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=%BUILD_TYPE%
if errorlevel 1 (
    echo CMake configuration failed
    exit /b 1
)

cmake --build build --parallel
if errorlevel 1 (
    echo Build failed
    exit /b 1
)

echo Build completed successfully!
