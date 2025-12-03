@echo off
setlocal

echo Packaging Pre-DateGrip...

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
    echo Run this script from Developer Command Prompt
    exit /b 1
)

REM Build frontend first
echo Building frontend...
cd /d %~dp0\..\frontend
if not exist node_modules (
    echo Installing npm dependencies...
    call npm install
    if errorlevel 1 (
        echo npm install failed
        exit /b 1
    )
)
call npm run build
if errorlevel 1 (
    echo Frontend build failed
    exit /b 1
)
cd /d %~dp0\..

REM Build backend with Ninja
echo Building backend...
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
if errorlevel 1 (
    echo CMake configuration failed
    exit /b 1
)

cmake --build build --parallel
if errorlevel 1 (
    echo Release build failed
    exit /b 1
)

REM Create distribution package
if not exist dist mkdir dist
if exist dist\frontend rmdir /S /Q dist\frontend
mkdir dist\frontend

copy /Y build\src\PreDateGrip.exe dist\
xcopy /E /Y frontend\dist\* dist\frontend\

echo.
echo Package created in dist/
echo Contents:
dir /B dist
