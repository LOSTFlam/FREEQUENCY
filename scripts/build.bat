@echo off
REM Build FREEQUENCY on Windows (produces FREEQUENCY.exe).
REM Requires CMake and Visual Studio 2022 (or Build Tools).
REM Usage: scripts\build.bat

setlocal
cd /d "%~dp0\.."

cmake -B build -DCMAKE_BUILD_TYPE=Release
if errorlevel 1 exit /b 1

cmake --build build --config Release --target FREEQUENCY --parallel
if errorlevel 1 exit /b 1

echo.
echo Built: build\src\FREEQUENCY_artefacts\Release\FREEQUENCY.exe
endlocal
