@echo off
REM Launch FREEQUENCY (build Release first if missing)
setlocal
cd /d "%~dp0\.."

set "EXE=build\src\FREEQUENCY_artefacts\Release\FREEQUENCY.exe"
if not exist "%EXE%" (
    echo Binary not found — building Release...
    call "%~dp0build.bat" release
    if errorlevel 1 exit /b 1
)

start "" "%CD%\%EXE%"
endlocal
