# Install FREEQUENCY without admin — portable user install from a built or downloaded folder.
# Usage:
#   powershell -ExecutionPolicy Bypass -File packaging\windows\install-portable.ps1
#   powershell -ExecutionPolicy Bypass -File packaging\windows\install-portable.ps1 -SourceDir "C:\Downloads\FREEQUENCY-win64"

param(
    [string]$SourceDir = "",
    [switch]$Uninstall
)

$ErrorActionPreference = "Stop"
$RepoRoot = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
if (-not $SourceDir) {
    $SourceDir = Join-Path $RepoRoot "build\src\FREEQUENCY_artefacts\Release"
}

$InstallDir = Join-Path $env:LOCALAPPDATA "Programs\FREEQUENCY"
$ExeName = "FREEQUENCY.exe"
$ShortcutDir = Join-Path $env:APPDATA "Microsoft\Windows\Start Menu\Programs"
$DesktopLink = Join-Path ([Environment]::GetFolderPath("Desktop")) "FREEQUENCY.lnk"

function Remove-IfExists($path) {
    if (Test-Path $path) { Remove-Item -LiteralPath $path -Force -Recurse -ErrorAction SilentlyContinue }
}

if ($Uninstall) {
    Remove-IfExists $InstallDir
    Remove-IfExists (Join-Path $ShortcutDir "FREEQUENCY.lnk")
    Remove-IfExists $DesktopLink
    reg delete "HKCU\Software\Classes\.freq" /f 2>$null
    reg delete "HKCU\Software\Classes\freequency.project" /f 2>$null
    Write-Host "FREEQUENCY removed from $InstallDir"
    exit 0
}

$SourceExe = Join-Path $SourceDir $ExeName
if (-not (Test-Path $SourceExe)) {
    Write-Error "FREEQUENCY.exe not found at: $SourceExe`nDownload a release from GitHub or build with build.bat first."
}

New-Item -ItemType Directory -Force -Path $InstallDir | Out-Null
Copy-Item -LiteralPath $SourceExe -Destination (Join-Path $InstallDir $ExeName) -Force
$icon = Join-Path $RepoRoot "assets\icon.png"
if (Test-Path $icon) { Copy-Item -LiteralPath $icon -Destination (Join-Path $InstallDir "icon.png") -Force }

$Wsh = New-Object -ComObject WScript.Shell
$lnk = $Wsh.CreateShortcut((Join-Path $ShortcutDir "FREEQUENCY.lnk"))
$lnk.TargetPath = Join-Path $InstallDir $ExeName
$lnk.WorkingDirectory = $InstallDir
$lnk.Description = "FREEQUENCY DAW"
$lnk.Save()

$desk = $Wsh.CreateShortcut($DesktopLink)
$desk.TargetPath = Join-Path $InstallDir $ExeName
$desk.WorkingDirectory = $InstallDir
$desk.Save()

reg add "HKCU\Software\Classes\.freq" /ve /d "freequency.project" /f | Out-Null
reg add "HKCU\Software\Classes\freequency.project" /ve /d "FREEQUENCY Project" /f | Out-Null
reg add "HKCU\Software\Classes\freequency.project\DefaultIcon" /ve /d "`"$InstallDir\FREEQUENCY.exe`",0" /f | Out-Null
reg add "HKCU\Software\Classes\freequency.project\shell\open\command" /ve /d "`"`"$InstallDir\FREEQUENCY.exe`"`" `"%1`"`"" /f | Out-Null

Write-Host ""
Write-Host "Installed FREEQUENCY to: $InstallDir"
Write-Host "Start Menu shortcut created. .freq files will open in FREEQUENCY."
Write-Host "To remove: powershell -ExecutionPolicy Bypass -File packaging\windows\install-portable.ps1 -Uninstall"
