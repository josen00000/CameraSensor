#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Run smoke tests for StallFreeCapture project.

.DESCRIPTION
    This script runs the sfc_core test suite using CMake and CTest.
    It configures the project if needed, builds it, and runs all tests.
    Test results are displayed and a summary report is saved.

.PARAMETER Preset
    The CMake preset to use. Default is "debug".

.PARAMETER Clean
    If specified, removes the build directory before configuring.

.EXAMPLE
    ./tools/run-tests.ps1
    Run tests with default debug preset.

.EXAMPLE
    ./tools/run-tests.ps1 -Preset release -Clean
    Clean build and run tests with release preset.
#>

param(
    [string]$Preset = "debug",
    [switch]$Clean
)

$ErrorActionPreference = "Stop"

# Navigate to sfc_core directory
$SfcCoreRoot = Join-Path $PSScriptRoot ".." "Plugins" "StallFreeCapture" "ThirdParty" "sfc_core"

if (-not (Test-Path $SfcCoreRoot)) {
    Write-Error "sfc_core directory not found at: $SfcCoreRoot"
    exit 1
}

Push-Location $SfcCoreRoot

try {
    Write-Host "=== StallFreeCapture Smoke Tests ===" -ForegroundColor Cyan
    Write-Host "Working directory: $PWD" -ForegroundColor Gray
    Write-Host "Preset: $Preset" -ForegroundColor Gray
    Write-Host ""

    # Clean if requested
    if ($Clean) {
        Write-Host "Cleaning build directory..." -ForegroundColor Yellow
        if (Test-Path "build") {
            Remove-Item -Recurse -Force "build"
        }
    }

    # Configure
    Write-Host "Configuring CMake..." -ForegroundColor Cyan
    cmake --preset $Preset
    if ($LASTEXITCODE -ne 0) {
        Write-Error "CMake configuration failed with exit code $LASTEXITCODE"
        exit $LASTEXITCODE
    }

    # Build
    Write-Host ""
    Write-Host "Building..." -ForegroundColor Cyan
    cmake --build --preset $Preset
    if ($LASTEXITCODE -ne 0) {
        Write-Error "Build failed with exit code $LASTEXITCODE"
        exit $LASTEXITCODE
    }

    # Run tests
    Write-Host ""
    Write-Host "Running tests..." -ForegroundColor Cyan
    $TestStartTime = Get-Date
    ctest --preset $Preset --output-on-failure
    $TestExitCode = $LASTEXITCODE
    $TestEndTime = Get-Date
    $TestDuration = $TestEndTime - $TestStartTime

    Write-Host ""
    if ($TestExitCode -eq 0) {
        Write-Host "=== All tests PASSED ===" -ForegroundColor Green
    } else {
        Write-Host "=== Tests FAILED ===" -ForegroundColor Red
    }
    Write-Host "Duration: $($TestDuration.TotalSeconds.ToString('F2'))s" -ForegroundColor Gray

    # Generate report
    $ReportPath = Join-Path $PSScriptRoot ".." "test-report.txt"
    $ReportContent = @"
StallFreeCapture Smoke Test Report
Generated: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')
Preset: $Preset
Duration: $($TestDuration.TotalSeconds.ToString('F2'))s
Exit Code: $TestExitCode
Result: $(if ($TestExitCode -eq 0) { "PASSED" } else { "FAILED" })

Tests run from: $SfcCoreRoot
"@

    $ReportContent | Out-File -FilePath $ReportPath -Encoding UTF8
    Write-Host ""
    Write-Host "Report saved to: $ReportPath" -ForegroundColor Gray

    exit $TestExitCode

} finally {
    Pop-Location
}
