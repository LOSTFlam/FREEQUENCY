@echo off
REM Package FREEQUENCY — portable ZIP + Inno Setup installer (after build).
powershell -ExecutionPolicy Bypass -File "%~dp0package.ps1" %*
