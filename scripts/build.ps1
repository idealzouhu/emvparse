<#
.SYNOPSIS
    Configure, build emvparse, and run CTest (Windows).

.DESCRIPTION
    Ensures vcpkg is available (prefers ./vcpkg, else env:VCPKG_ROOT, else clones vcpkg-tool-version),
    runs CMake with manifest mode (vcpkg.json), builds default targets, and runs CTest unless -SkipTests.

    Requires CMake 3.10+ and a C++14 toolchain (e.g. Visual Studio 2019+).

.PARAMETER Config
    CMake build type: Release or Debug. Default: Release.

.PARAMETER Triplet
    vcpkg target triplet. Default: x64-windows (matches Release CI).

.PARAMETER SkipTests
    Build only; do not run ctest.

.PARAMETER VcpkgRoot
    Force a specific vcpkg root. When omitted, uses ./vcpkg if present, else VCPKG_ROOT, else clones ./vcpkg.

.EXAMPLE
    .\scripts\build.ps1

.EXAMPLE
    .\scripts\build.ps1 -Config Debug -SkipTests

.NOTES
    If script execution is blocked:
    Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass
#>
[CmdletBinding()]
param(
    [ValidateSet("Release", "Debug")]
    [string]$Config = "Release",
    [string]$Triplet = "x64-windows",
    [switch]$SkipTests,
    [string]$VcpkgRoot = ""
)

$ErrorActionPreference = "Stop"
$RepoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
Set-Location $RepoRoot

$VcpkgTag = (Get-Content -Path (Join-Path $RepoRoot "vcpkg-tool-version") -Raw).Trim()

function Test-VcpkgRoot {
    param([string]$Root)
    return $Root -and (Test-Path (Join-Path $Root "scripts/buildsystems/vcpkg.cmake"))
}

function Repair-VcpkgGitIfShallow {
    param([string]$LocalRoot)
    if (-not (Test-Path (Join-Path $LocalRoot ".git"))) { return }
    $isShallow = (git -C $LocalRoot rev-parse --is-shallow-repository 2>$null)
    if ($isShallow -ne "true") { return }
    Write-Host "vcpkg is a shallow clone; fetching full history (required for vcpkg.json baseline) ..."
    git -C $LocalRoot fetch --unshallow origin
    if ($LASTEXITCODE -ne 0) {
        Write-Host "unshallow failed; re-cloning vcpkg ..."
        Remove-Item -Recurse -Force $LocalRoot
        git clone --branch $VcpkgTag https://github.com/microsoft/vcpkg.git $LocalRoot
        if ($LASTEXITCODE -ne 0) { throw "vcpkg re-clone failed" }
    }
}

function Ensure-Vcpkg {
    param([string]$ForcedRoot)

    if (Test-VcpkgRoot $ForcedRoot) {
        return ,((Resolve-Path $ForcedRoot).Path)
    }

    $Local = Join-Path $RepoRoot "vcpkg"
    if (Test-VcpkgRoot $Local) {
        Repair-VcpkgGitIfShallow -LocalRoot $Local
        return ,((Resolve-Path $Local).Path)
    }

    if (-not $ForcedRoot -and (Test-VcpkgRoot $env:VCPKG_ROOT)) {
        Write-Host "Using VCPKG_ROOT from environment (no ./vcpkg yet)."
        return ,((Resolve-Path $env:VCPKG_ROOT).Path)
    }

    if (-not (Test-Path (Join-Path $Local ".git"))) {
        Write-Host "Cloning vcpkg ($VcpkgTag) into $Local (full clone for manifest baseline) ..."
        git clone --branch $VcpkgTag https://github.com/microsoft/vcpkg.git $Local
        if ($LASTEXITCODE -ne 0) { throw "vcpkg clone failed" }
    } else {
        Repair-VcpkgGitIfShallow -LocalRoot $Local
    }

    $Exe = Join-Path $Local "vcpkg.exe"
    if (-not (Test-Path $Exe)) {
        Write-Host "Bootstrapping vcpkg ..."
        & (Join-Path $Local "bootstrap-vcpkg.bat") -disableMetrics | Out-Host
        if ($LASTEXITCODE -ne 0) { throw "vcpkg bootstrap failed" }
        if (-not (Test-Path $Exe)) { throw "vcpkg.exe missing after bootstrap" }
    }

    return ,((Resolve-Path $Local).Path)
}

$VcpkgRoot = Ensure-Vcpkg -ForcedRoot $VcpkgRoot
$env:VCPKG_ROOT = $VcpkgRoot
$Toolchain = Join-Path $VcpkgRoot "scripts/buildsystems/vcpkg.cmake"
$BuildDir = Join-Path $RepoRoot "build"

Write-Host "Using VCPKG_ROOT=$VcpkgRoot"
Write-Host "Configuring (triplet=$Triplet, config=$Config) ..."
cmake -B $BuildDir -S $RepoRoot `
    "-DCMAKE_TOOLCHAIN_FILE=$Toolchain" `
    "-DVCPKG_TARGET_TRIPLET=$Triplet" `
    "-DVCPKG_MANIFEST_MODE=ON" `
    "-DCMAKE_BUILD_TYPE=$Config" `
    "-DBUILD_TESTING=ON" `
    "-DBUILD_EMVPARSE_CLI=ON"
if ($LASTEXITCODE -ne 0) { throw "cmake configure failed" }

Write-Host "Building ..."
cmake --build $BuildDir --config $Config --parallel
if ($LASTEXITCODE -ne 0) { throw "cmake build failed" }

if (-not $SkipTests) {
    Write-Host "Running ctest ..."
    ctest --test-dir $BuildDir -C $Config --output-on-failure
    if ($LASTEXITCODE -ne 0) { throw "ctest failed" }
}

$exeNames = @("emvparse.exe", "emvparse_tests.exe")
foreach ($name in $exeNames) {
    $candidates = @(
        (Join-Path $BuildDir "bin\$Config\$name"),
        (Join-Path $BuildDir "bin\$name"),
        (Join-Path $BuildDir "$Config\$name")
    )
    $built = $candidates | Where-Object { Test-Path $_ } | Select-Object -First 1
    if ($built) {
        Write-Host "Built: $((Resolve-Path $built).Path)"
    }
}

Write-Host "Done."
