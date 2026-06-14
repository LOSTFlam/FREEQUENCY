# FREEQUENCY Windows build (recommended)
# Usage:
#   .\scripts\build.ps1                    # Release
#   .\scripts\build.ps1 -Config Debug
#   .\scripts\build.ps1 -Clean
#   .\scripts\build.ps1 -Run
#   .\scripts\build.ps1 -SelfTest
#   .\scripts\build.ps1 -NoLto

param(
    [ValidateSet('Release', 'Debug')]
    [string] $Config = 'Release',
    [switch] $Clean,
    [switch] $Run,
    [switch] $SelfTest,
    [switch] $NoLto
)

$ErrorActionPreference = 'Stop'
Set-Location (Join-Path $PSScriptRoot '..')

function Require-Command($name, $hint) {
    if (-not (Get-Command $name -ErrorAction SilentlyContinue)) {
        Write-Error "[ERROR] '$name' not found.`n$hint"
    }
}

Write-Host "`n=== FREEQUENCY build ($Config) ===" -ForegroundColor Cyan
Write-Host "Repo: $(Get-Location)`n"

Require-Command cmake "Install CMake 3.22+ from https://cmake.org/download/"
Require-Command git   "Install Git from https://git-scm.com/download/win"

if ($Clean -and (Test-Path 'build')) {
    Write-Host 'Cleaning build/...'
    Remove-Item -Recurse -Force 'build'
}

$lto = if ($NoLto) { 'OFF' } else { 'ON' }
$cmakeArgs = @('-B', 'build', "-DFREEQUENCY_ENABLE_LTO=$lto")

$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vsPath = $null
if (Test-Path $vswhere) {
    $vsPath = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild -property installationPath 2>$null
}

if ($vsPath) {
    Write-Host "Using Visual Studio: $vsPath"
    $cmakeArgs += @('-G', 'Visual Studio 17 2022', '-A', 'x64')
}
elseif (Get-Command ninja -ErrorAction SilentlyContinue) {
    Write-Host 'Using Ninja generator'
    $cmakeArgs += @('-G', 'Ninja', "-DCMAKE_BUILD_TYPE=$Config")
}
else {
    Write-Error @"
[ERROR] No C++ toolchain found.

Install Visual Studio 2022 with workload 'Desktop development with C++'
(includes MSVC, Windows SDK, CMake support), then re-run this script.

Download: https://visualstudio.microsoft.com/downloads/
"@
}

Write-Host "`nConfiguring..."
& cmake @cmakeArgs
if ($LASTEXITCODE -ne 0) { throw 'CMake configure failed' }

Write-Host "`nBuilding..."
& cmake --build build --config $Config --target FREEQUENCY --parallel
if ($LASTEXITCODE -ne 0) { throw 'Build failed' }

$exe = Join-Path (Get-Location) "build\src\FREEQUENCY_artefacts\$Config\FREEQUENCY.exe"
if (-not (Test-Path $exe)) { throw "Binary not found: $exe" }

Write-Host "`n[OK] Built: $exe" -ForegroundColor Green

if ($SelfTest) {
    Write-Host "`nRunning --selftest..."
    & $exe --selftest
    if ($LASTEXITCODE -ne 0) { throw 'Self-test failed' }
}

if ($Run) {
    Write-Host 'Launching FREEQUENCY...'
    Start-Process $exe
}
