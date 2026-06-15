# Package FREEQUENCY into distributable installers (run AFTER build).
param(
    [string]$Config = "Release",
    [string]$Version = "0.1.0",
    [switch]$PortableOnly
)

$ErrorActionPreference = "Stop"
$Root = Split-Path $PSScriptRoot -Parent
Set-Location $Root

$BuildDir = Join-Path $Root "build\src\FREEQUENCY_artefacts\$Config"
$Exe = Join-Path $BuildDir "FREEQUENCY.exe"
$Dist = Join-Path $Root "dist"

if (-not (Test-Path $Exe)) {
    Write-Host "Binary missing — building Release..."
    & (Join-Path $Root "scripts\build.bat") release
    if (-not (Test-Path $Exe)) { throw "Build failed: $Exe not found" }
}

New-Item -ItemType Directory -Force -Path $Dist | Out-Null

# Portable ZIP
$ZipName = "FREEQUENCY-$Version-win64-portable.zip"
$ZipPath = Join-Path $Dist $ZipName
$Stage = Join-Path $env:TEMP "freequency-pack"
Remove-Item -Recurse -Force $Stage -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force -Path $Stage | Out-Null
Copy-Item $Exe (Join-Path $Stage "FREEQUENCY.exe")
Copy-Item (Join-Path $Root "assets\icon.png") $Stage -ErrorAction SilentlyContinue
Copy-Item (Join-Path $Root "packaging\windows\install-portable.ps1") $Stage
if (Test-Path $ZipPath) { Remove-Item $ZipPath -Force }
Compress-Archive -Path (Join-Path $Stage "*") -DestinationPath $ZipPath -Force
Write-Host "[OK] $ZipPath"

if ($PortableOnly) { exit 0 }

# Inno Setup installer
$Iscc = $null
foreach ($candidate in @(
    "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe",
    "${env:ProgramFiles}\Inno Setup 6\ISCC.exe"
)) {
    if (Test-Path $candidate) { $Iscc = $candidate; break }
}
if (-not $Iscc -and (Get-Command iscc -ErrorAction SilentlyContinue)) {
    $Iscc = "iscc"
}

if ($Iscc) {
    $Iss = Join-Path $Root "packaging\windows\freequency.iss"
    & $Iscc "/DBuildDir=$BuildDir" "/DAppVersion=$Version" $Iss
    Write-Host "[OK] Inno Setup installer in dist\"
} else {
    Write-Host "[SKIP] Inno Setup not found — install from https://jrsoftware.org/isinfo.php"
    Write-Host "       Portable ZIP is ready; users can run install-portable.ps1"
}

Write-Host ""
Write-Host "Done. Share dist\FREEQUENCY-Setup-$Version-win64.exe or the portable ZIP."
