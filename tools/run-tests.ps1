#!/usr/bin/env pwsh
#Requires -Version 7

<#
.SYNOPSIS
    Run smoke tests for StallFreeCapture project.

.DESCRIPTION
    This script runs smoke tests for the StallFreeCapture project:
    - sfc_core C++ tests via CMake/CTest (default)
    - Unreal Engine Automation test SFC.Smoke with -Unreal switch

    The script always writes a report to docs/reports/smoke-YYYY-MM-DD.txt
    with PASSED or FAILED status and full details.

.PARAMETER Preset
    The CMake preset to use for sfc_core tests. Default is "debug".

.PARAMETER Clean
    If specified, removes the build directory before configuring sfc_core.

.PARAMETER Unreal
    If specified, runs the Unreal Engine Automation test (SFC.Smoke) instead
    of the sfc_core tests. Requires UnrealEditor-Cmd in PATH or UNREAL_ENGINE
    environment variable pointing to the engine root.

.EXAMPLE
    ./tools/run-tests.ps1
    Run sfc_core smoke tests with default debug preset.

.EXAMPLE
    ./tools/run-tests.ps1 -Preset release -Clean
    Clean build and run sfc_core tests with release preset.

.EXAMPLE
    ./tools/run-tests.ps1 -Unreal
    Run Unreal Engine Automation test SFC.Smoke in headless mode.
#>

param(
    [string]$Preset = "debug",
    [switch]$Clean,
    [switch]$Unreal
)

# Capture errors but don't throw immediately so we can write reports
$ErrorActionPreference = "Continue"

# Get ISO-8601 timestamp with zone
$Timestamp = Get-Date -Format o
$DateStamp = Get-Date -Format "yyyy-MM-dd"

# Determine project root (one level up from tools/)
$ProjectRoot = Split-Path -Parent $PSScriptRoot

if ($Unreal) {
    # ===== UNREAL ENGINE AUTOMATION TEST =====
    
    Write-Host "=== StallFreeCapture Unreal Automation Smoke Test ===" -ForegroundColor Cyan
    Write-Host "Timestamp: $Timestamp" -ForegroundColor Gray
    Write-Host ""

    # Find UnrealEditor-Cmd
    $UnrealCmd = $null
    if (Get-Command "UnrealEditor-Cmd" -ErrorAction SilentlyContinue) {
        $UnrealCmd = "UnrealEditor-Cmd"
    } elseif ($env:UNREAL_ENGINE) {
        $UnrealCmd = Join-Path $env:UNREAL_ENGINE "Engine" "Binaries" "Win64" "UnrealEditor-Cmd.exe"
        if (-not (Test-Path $UnrealCmd)) {
            $UnrealCmd = $null
        }
    }

    if (-not $UnrealCmd) {
        Write-Host "ERROR: UnrealEditor-Cmd not found in PATH or UNREAL_ENGINE environment variable." -ForegroundColor Red
        Write-Host "Set UNREAL_ENGINE to your Unreal Engine root directory (e.g., C:\Program Files\Epic Games\UE_5.8)" -ForegroundColor Yellow
        
        $ReportPath = Join-Path $ProjectRoot "docs" "reports" "smoke-$DateStamp.txt"
        $ReportContent = @"
StallFreeCapture Unreal Automation Smoke Test Report
Generated: $Timestamp
Result: FAILED

Error: UnrealEditor-Cmd not found.
Please install Unreal Engine 5.8 or set UNREAL_ENGINE environment variable.
"@
        New-Item -ItemType Directory -Force -Path (Split-Path $ReportPath) | Out-Null
        $ReportContent | Out-File -FilePath $ReportPath -Encoding UTF8
        Write-Host "Report saved to: $ReportPath" -ForegroundColor Gray
        exit 1
    }

    # Setup paths
    $UProjectPath = Join-Path $ProjectRoot "CaptureSandbox.uproject"
    $ReportExportPath = Join-Path $ProjectRoot "docs" "reports" "unreal-automation-$DateStamp"
    
    if (-not (Test-Path $UProjectPath)) {
        Write-Host "ERROR: Project file not found: $UProjectPath" -ForegroundColor Red
        $ReportPath = Join-Path $ProjectRoot "docs" "reports" "smoke-$DateStamp.txt"
        $ReportContent = @"
StallFreeCapture Unreal Automation Smoke Test Report
Generated: $Timestamp
Result: FAILED

Error: CaptureSandbox.uproject not found at: $UProjectPath
"@
        New-Item -ItemType Directory -Force -Path (Split-Path $ReportPath) | Out-Null
        $ReportContent | Out-File -FilePath $ReportPath -Encoding UTF8
        Write-Host "Report saved to: $ReportPath" -ForegroundColor Gray
        exit 1
    }

    Write-Host "UnrealEditor-Cmd: $UnrealCmd" -ForegroundColor Gray
    Write-Host "Project: $UProjectPath" -ForegroundColor Gray
    Write-Host "Report export: $ReportExportPath" -ForegroundColor Gray
    Write-Host ""
    Write-Host "Running SFC.Smoke test..." -ForegroundColor Cyan

    $TestStartTime = Get-Date
    
    # Run the test
    & $UnrealCmd $UProjectPath `
        -ExecCmds="Automation RunTests StallFreeCapture.Smoke" `
        -nullrhi `
        -unattended `
        -ReportExportPath="$ReportExportPath" `
        -log

    $TestExitCode = $LASTEXITCODE
    $TestEndTime = Get-Date
    $TestDuration = $TestEndTime - $TestStartTime

    Write-Host ""
    if ($TestExitCode -eq 0) {
        Write-Host "=== Unreal Automation test PASSED ===" -ForegroundColor Green
    } else {
        Write-Host "=== Unreal Automation test FAILED ===" -ForegroundColor Red
    }
    Write-Host "Duration: $($TestDuration.TotalSeconds.ToString('F2'))s" -ForegroundColor Gray

    # Generate report
    $ReportPath = Join-Path $ProjectRoot "docs" "reports" "smoke-$DateStamp.txt"
    $ReportContent = @"
StallFreeCapture Unreal Automation Smoke Test Report
Generated: $Timestamp
Test: StallFreeCapture.Smoke
Mode: -nullrhi -unattended
Duration: $($TestDuration.TotalSeconds.ToString('F2'))s
Exit Code: $TestExitCode
Result: $(if ($TestExitCode -eq 0) { "PASSED" } else { "FAILED" })

Command:
UnrealEditor-Cmd "$UProjectPath" -ExecCmds="Automation RunTests StallFreeCapture.Smoke" -nullrhi -unattended -ReportExportPath="$ReportExportPath" -log

Report export path: $ReportExportPath

$( if (Test-Path $ReportExportPath) {
    "Automation reports written to export path (see index.html for details)."
} else {
    "Note: Report export path not created (expected if test failed early)."
})
"@

    New-Item -ItemType Directory -Force -Path (Split-Path $ReportPath) | Out-Null
    $ReportContent | Out-File -FilePath $ReportPath -Encoding UTF8
    Write-Host ""
    Write-Host "Report saved to: $ReportPath" -ForegroundColor Gray

    exit $TestExitCode

} else {
    # ===== SFC_CORE C++ TESTS =====
    
    $SfcCoreRoot = Join-Path $ProjectRoot "Plugins" "StallFreeCapture" "ThirdParty" "sfc_core"

    if (-not (Test-Path $SfcCoreRoot)) {
        Write-Host "ERROR: sfc_core directory not found at: $SfcCoreRoot" -ForegroundColor Red
        $ReportPath = Join-Path $ProjectRoot "docs" "reports" "smoke-$DateStamp.txt"
        $ReportContent = @"
StallFreeCapture sfc_core Smoke Test Report
Generated: $Timestamp
Result: FAILED

Error: sfc_core directory not found at: $SfcCoreRoot
"@
        New-Item -ItemType Directory -Force -Path (Split-Path $ReportPath) | Out-Null
        $ReportContent | Out-File -FilePath $ReportPath -Encoding UTF8
        Write-Host "Report saved to: $ReportPath" -ForegroundColor Gray
        exit 1
    }

    Push-Location $SfcCoreRoot

    try {
        Write-Host "=== StallFreeCapture sfc_core Smoke Tests ===" -ForegroundColor Cyan
        Write-Host "Timestamp: $Timestamp" -ForegroundColor Gray
        Write-Host "Working directory: $PWD" -ForegroundColor Gray
        Write-Host "Preset: $Preset" -ForegroundColor Gray
        Write-Host ""

        $OverallSuccess = $true
        $ErrorMessages = @()

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
            $OverallSuccess = $false
            $ErrorMessages += "CMake configuration failed with exit code $LASTEXITCODE"
            Write-Host "ERROR: CMake configuration failed with exit code $LASTEXITCODE" -ForegroundColor Red
        }

        # Build (only if configure succeeded)
        $BuildExitCode = 0
        if ($OverallSuccess) {
            Write-Host ""
            Write-Host "Building..." -ForegroundColor Cyan
            cmake --build --preset $Preset
            $BuildExitCode = $LASTEXITCODE
            if ($BuildExitCode -ne 0) {
                $OverallSuccess = $false
                $ErrorMessages += "Build failed with exit code $BuildExitCode"
                Write-Host "ERROR: Build failed with exit code $BuildExitCode" -ForegroundColor Red
            }
        }

        # Run tests (only if build succeeded)
        $TestExitCode = 0
        $TestDuration = $null
        if ($OverallSuccess) {
            Write-Host ""
            Write-Host "Running tests..." -ForegroundColor Cyan
            $TestStartTime = Get-Date
            ctest --preset $Preset --output-on-failure
            $TestExitCode = $LASTEXITCODE
            $TestEndTime = Get-Date
            $TestDuration = $TestEndTime - $TestStartTime

            if ($TestExitCode -ne 0) {
                $OverallSuccess = $false
                $ErrorMessages += "Tests failed with exit code $TestExitCode"
            }
        }

        Write-Host ""
        if ($OverallSuccess) {
            Write-Host "=== All tests PASSED ===" -ForegroundColor Green
        } else {
            Write-Host "=== Tests FAILED ===" -ForegroundColor Red
        }
        if ($TestDuration) {
            Write-Host "Test duration: $($TestDuration.TotalSeconds.ToString('F2'))s" -ForegroundColor Gray
        }

        # Generate report (always, even on failure)
        $ReportPath = Join-Path $ProjectRoot "docs" "reports" "smoke-$DateStamp.txt"
        $ReportContent = @"
StallFreeCapture sfc_core Smoke Test Report
Generated: $Timestamp
Preset: $Preset
Working directory: $SfcCoreRoot
$(if ($TestDuration) { "Test duration: $($TestDuration.TotalSeconds.ToString('F2'))s" } else { "Tests did not run (build or configure failed)" })
Exit Code: $(if ($TestExitCode -ne 0) { $TestExitCode } elseif ($BuildExitCode -ne 0) { $BuildExitCode } else { $LASTEXITCODE })
Result: $(if ($OverallSuccess) { "PASSED" } else { "FAILED" })

$(if ($ErrorMessages.Count -gt 0) {
"Errors:
$($ErrorMessages | ForEach-Object { "- $_" } | Out-String)"
} else {
"All steps completed successfully.
- CMake configuration: OK
- Build: OK
- Tests: PASSED
"})
"@

        New-Item -ItemType Directory -Force -Path (Split-Path $ReportPath) | Out-Null
        $ReportContent | Out-File -FilePath $ReportPath -Encoding UTF8
        Write-Host ""
        Write-Host "Report saved to: $ReportPath" -ForegroundColor Gray

        # Exit with appropriate code
        if ($OverallSuccess) {
            exit 0
        } else {
            if ($TestExitCode -ne 0) {
                exit $TestExitCode
            } elseif ($BuildExitCode -ne 0) {
                exit $BuildExitCode
            } else {
                exit 1
            }
        }

    } finally {
        Pop-Location
    }
}
