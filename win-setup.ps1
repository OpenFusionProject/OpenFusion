# Run this first if needed:
# Set-ExecutionPolicy -ExecutionPolicy Bypass -Scope Process
param (
    # height of largest column without top bar
    [Parameter(Mandatory=$true)]
    [string]$protocolVersion
)

$ErrorActionPreference = 'Stop'
# check for vscmd
if ([string]::IsNullOrEmpty($env:VSCMD_VER)) {
    Write-Host 'Must be run inside of VS Developer Powershell'
    exit 1
}

# check for git
try {
    $git_version = git --version
} catch {
    Write-Host 'git not installed'
    exit 1
}

# setup vcpkg
if (Test-Path -Path 'vcpkg\') {
    Write-Host 'vcpkg already setup'
} else {
    Write-Host 'Setting up vcpkg...'
    git clone "https://github.com/microsoft/vcpkg.git"
}

if (-not (Test-Path -Path 'vcpkg\vcpkg.exe')) {
    Write-Host 'Bootstrapping vcpkg...'
    Start-Process -Wait -NoNewWindow -FilePath 'vcpkg\bootstrap-vcpkg.bat' -ArgumentList '-disableMetrics'
}

$env:VCPKG_ROOT='' # ignore msvc's vcpkg root, it doesn't work
$vcpkg = (Resolve-Path -Path '.\vcpkg')
Write-Host "vcpkg installed to $vcpkg"
Start-Process -Wait -NoNewWindow -FilePath 'vcpkg\vcpkg.exe' -ArgumentList 'install sqlite3:x64-windows'

# setup cmake project
if (Test-Path -Path 'build\') {
    Write-Host 'cmake project already setup';
} else {
    Write-Host 'Setting up cmake project...'
    Start-Process -Wait -NoNewWindow -FilePath 'cmake' -ArgumentList "-B build -DPROTOCOL_VERSION=$protocolVersion -DCMAKE_TOOLCHAIN_FILE=$vcpkg\scripts\buildsystems\vcpkg.cmake"
}

Write-Host 'Done!'
$sln = (Resolve-Path -Path '.\build\OpenFusion.sln')
Write-Host "Solution file is at $sln"
