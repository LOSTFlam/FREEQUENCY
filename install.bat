@echo off
REM Install FREEQUENCY without building — from portable ZIP or local build output.
powershell -ExecutionPolicy Bypass -File "%~dp0packaging\windows\install-portable.ps1" %*
