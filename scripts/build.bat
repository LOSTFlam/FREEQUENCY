@echo off
setlocal EnableExtensions EnableDelayedExpansion

REM FREEQUENCY Windows build helper
REM Usage:
REM   scripts\build.bat              Release build (default)
REM   scripts\build.bat debug        Debug build
REM   scripts\build.bat clean        Delete build/ then configure
REM   scripts\build.bat run          Build Release and launch FREEQUENCY.exe
REM   scripts\build.bat test         Build Release and run --selftest
REM   scripts\build.bat nolto        Release without link-time optimisation (faster links)

cd /d "%~dp0\.."

set "CONFIG=Release"
set "DO_CLEAN=0"
set "DO_RUN=0"
set "DO_TEST=0"
set "LTO=ON"

if /I "%~1"=="debug"  set "CONFIG=Debug" & shift
if /I "%~1"=="release" set "CONFIG=Release" & shift
if /I "%~1"=="clean"  set "DO_CLEAN=1" & shift
if /I "%~1"=="run"    set "DO_RUN=1" & shift
if /I "%~1"=="test"   set "DO_TEST=1" & shift
if /I "%~1"=="nolto"  set "LTO=OFF" & shift

echo.
echo === FREEQUENCY build (%CONFIG%) ===
echo Repo: %CD%
echo.

where cmake >nul 2>&1
if errorlevel 1 (
    echo [ERROR] CMake not found. Install CMake 3.22+ and add it to PATH.
    echo         https://cmake.org/download/
    exit /b 1
)

where git >nul 2>&1
if errorlevel 1 (
    echo [ERROR] Git not found. JUCE is downloaded via CMake FetchContent and requires Git.
    echo         https://git-scm.com/download/win
    exit /b 1
)

if "%DO_CLEAN%"=="1" (
    echo Cleaning build folder...
    if exist build rmdir /s /q build
)

set "GENERATOR="
set "ARCH=-A x64"
set "VS_PATH="

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if exist "%VSWHERE%" (
    for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.Component.MSBuild -property installationPath 2^>nul`) do set "VS_PATH=%%i"
)

if defined VS_PATH (
    echo Using Visual Studio: !VS_PATH!
    set "GENERATOR=-G Visual Studio 17 2022"
) else (
    where ninja >nul 2>&1
    if not errorlevel 1 (
        echo Using Ninja generator
        set "GENERATOR=-G Ninja"
        set "ARCH="
    ) else (
        echo [ERROR] No Visual Studio 2022 and no Ninja found.
        echo.
        echo Install ONE of:
        echo   - Visual Studio 2022 with "Desktop development with C++"
        echo   - Build Tools for Visual Studio 2022 + Windows 10/11 SDK
        echo   - Ninja + MSVC via "x64 Native Tools Command Prompt"
        echo.
        echo Or run:  powershell -ExecutionPolicy Bypass -File scripts\build.ps1
        exit /b 1
    )
)

set "CMAKE_ARGS=-B build %GENERATOR% %ARCH% -DFREEQUENCY_ENABLE_LTO=%LTO%"

REM Single-config generators (Ninja) need CMAKE_BUILD_TYPE on configure.
if /I "%GENERATOR%"=="-G Ninja" (
    set "CMAKE_ARGS=%CMAKE_ARGS% -DCMAKE_BUILD_TYPE=%CONFIG%"
)

echo.
echo Configuring...
cmake %CMAKE_ARGS%
if errorlevel 1 (
    echo.
    echo [ERROR] CMake configure failed.
    exit /b 1
)

echo.
echo Building FREEQUENCY (%CONFIG%)...
cmake --build build --config %CONFIG% --target FREEQUENCY --parallel
if errorlevel 1 (
    echo.
    echo [ERROR] Build failed.
    exit /b 1
)

set "EXE=build\src\FREEQUENCY_artefacts\%CONFIG%\FREEQUENCY.exe"
if not exist "%EXE%" (
    echo [ERROR] Expected binary missing: %EXE%
    exit /b 1
)

echo.
echo [OK] Built: %CD%\%EXE%

if "%DO_TEST%"=="1" (
    echo.
    echo Running --selftest...
    "%EXE%" --selftest
    if errorlevel 1 exit /b 1
)

if "%DO_RUN%"=="1" (
    echo.
    echo Launching FREEQUENCY...
    start "" "%EXE%"
)

endlocal
exit /b 0
