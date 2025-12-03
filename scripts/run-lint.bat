@echo off
setlocal enabledelayedexpansion

set SCRIPT_DIR=%~dp0
set PROJECT_DIR=%SCRIPT_DIR%..

echo Running lint and format checks...

REM Frontend lint + format (Biome)
echo.
echo [Frontend] Running Biome lint and format...
cd /d "%PROJECT_DIR%\frontend"

REM First, auto-fix all issues
call npm run lint:fix
if !ERRORLEVEL! neq 0 (
    echo [Frontend] Biome auto-fix failed
    exit /b 1
)

REM Then verify no issues remain
call npm run lint -- --error-on-warnings
if !ERRORLEVEL! neq 0 (
    echo [Frontend] Lint failed - unfixable errors or warnings found
    exit /b 1
)
echo [Frontend] Lint and format passed

REM C++ format (clang-format)
echo.
echo [C++] Running clang-format...
cd /d "%PROJECT_DIR%"
where clang-format >nul 2>&1
if %ERRORLEVEL% neq 0 (
    echo [C++] clang-format not found, skipping C++ format
    goto :done
)

REM Auto-format C++ files
for /r src %%f in (*.cpp *.h) do (
    clang-format -i --style=file "%%f"
)
echo [C++] Format applied

REM Verify formatting (should pass after auto-format)
set CPP_FORMAT_ERROR=0
for /r src %%f in (*.cpp *.h) do (
    clang-format --style=file --dry-run --Werror "%%f" >nul 2>&1
    if !ERRORLEVEL! neq 0 (
        echo [C++] Format error: %%f
        set CPP_FORMAT_ERROR=1
    )
)
if %CPP_FORMAT_ERROR% neq 0 (
    echo [C++] Format check failed
    exit /b 1
)
echo [C++] Format check passed

:done
echo.
echo All lint and format checks passed!
exit /b 0
