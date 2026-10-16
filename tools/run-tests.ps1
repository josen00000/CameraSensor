#!/usr/bin/env pwsh
#Requires -Version 7

<#
.SYNOPSIS
    Run smoke tests for StallFreeCapture project.

.DESCRIPTION
    This script runs smoke tests for the StallFreeCapture project:
    - sfc_core C++ tests via CMake/CTest (default)
    - Unreal Engine Automation test StallFreeCapture.Version with -Unreal switch

    The script always writes a report to docs/reports/ with PASSED or FAILED status:
    - smoke-core-YYYY-MM-DD.txt for sfc_core tests
    - smoke-ue-YYYY-MM-DD.txt for Unreal Automation tests

.PARAMETER Preset
    The CMake preset to use for sfc_core tests. Default is "debug".

.PARAMETER Clean
    If specified, removes the build directory before configuring sfc_core.

.PARAMETER Unreal
    If specified, runs the Unreal Engine Automation test (StallFreeCapture.Version)
    instead of the sfc_core tests. Requires UnrealEditor-Cmd in PATH or UNREAL_ENGINE
    environment variable pointing to the engine root.

.EXAMPLE
    ./tools/run-tests.ps1
    Run sfc_core smoke tests with default debug preset.

.EXAMPLE
    ./tools/run-tests.ps1 -Preset release -Clean
    Clean build and run sfc_core tests with release preset.

.EXAMPLE
    ./tools/run-tests.ps1 -Unreal
    Run Unreal Engine Automation test StallFreeCapture.Version in headless mode.
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
    $TimeStamp = Get-Date -Format "HHmmss"

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
        
        $ReportPath = Join-Path $ProjectRoot "docs" "reports" "smoke-ue-$DateStamp.txt"
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
    $ReportExportPath = Join-Path $ProjectRoot "docs" "reports" "unreal-automation-$DateStamp-$TimeStamp"
    
    # Delete any existing report export path to prevent reading stale index.json
    if (Test-Path $ReportExportPath) {
        Write-Host "Removing existing report path: $ReportExportPath" -ForegroundColor Yellow
        Remove-Item -Recurse -Force $ReportExportPath
    }
    
    if (-not (Test-Path $UProjectPath)) {
        Write-Host "ERROR: Project file not found: $UProjectPath" -ForegroundColor Red
        $ReportPath = Join-Path $ProjectRoot "docs" "reports" "smoke-ue-$DateStamp.txt"
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
    Write-Host "Running StallFreeCapture.Version test..." -ForegroundColor Cyan

    $TestStartTime = Get-Date
    
    # Run the test
    & $UnrealCmd $UProjectPath `
        -ExecCmds="Automation RunTests StallFreeCapture.Version;Quit" `
        -nullrhi `
        -unattended `
        -nosplash `
        -testexit="Automation Test Queue Empty" `
        -ReportExportPath="$ReportExportPath" `
        -log

    $UnrealExitCode = $LASTEXITCODE
    $TestEndTime = Get-Date
    $TestDuration = $TestEndTime - $TestStartTime

    Write-Host ""
    Write-Host "Unreal Editor exit code: $UnrealExitCode" -ForegroundColor Gray

    # Parse index.json to verify test results
    $IndexJsonPath = Join-Path $ReportExportPath "index.json"
    $TestPassed = $false
    $TestDetails = ""
    
    if (Test-Path $IndexJsonPath) {
        try {
            $IndexJson = Get-Content $IndexJsonPath -Raw | ConvertFrom-Json
            $Succeeded = $IndexJson.succeeded
            $SucceededWithWarnings = $IndexJson.succeededWithWarnings
            $Failed = $IndexJson.failed
            $NotRun = $IndexJson.notRun
            
            # Count tests that passed (with or without warnings)
            $Passed = $Succeeded + $SucceededWithWarnings
            
            Write-Host "Test results from index.json: succeeded=$Succeeded, succeededWithWarnings=$SucceededWithWarnings, failed=$Failed, notRun=$NotRun" -ForegroundColor Gray
            
            $TestDetails = "Test counts: succeeded=$Succeeded, succeededWithWarnings=$SucceededWithWarnings, failed=$Failed, notRun=$NotRun (passed=$Passed)"
            
            if ($Passed -ge 1 -and $Failed -eq 0 -and $NotRun -eq 0) {
                $TestPassed = $true
            } else {
                $TestPassed = $false
                if ($Passed -lt 1) {
                    $TestDetails += "`nFAILED: No tests passed (passed=$Passed)"
                }
                if ($Failed -gt 0) {
                    $TestDetails += "`nFAILED: Some tests failed (failed=$Failed)"
                }
                if ($NotRun -gt 0) {
                    $TestDetails += "`nFAILED: Some tests did not run (notRun=$NotRun)"
                }
            }
        } catch {
            $TestPassed = $false
            $TestDetails = "FAILED: Could not parse index.json: $_"
            Write-Host "ERROR: Failed to parse index.json: $_" -ForegroundColor Red
        }
    } else {
        $TestPassed = $false
        $TestDetails = "FAILED: index.json not found at $IndexJsonPath (no tests matched or early exit)"
        Write-Host "ERROR: index.json not found at $IndexJsonPath" -ForegroundColor Red
    }

    Write-Host ""
    if ($TestPassed) {
        Write-Host "=== Unreal Automation test PASSED ===" -ForegroundColor Green
    } else {
        Write-Host "=== Unreal Automation test FAILED ===" -ForegroundColor Red
    }
    Write-Host "Duration: $($TestDuration.TotalSeconds.ToString('F2'))s" -ForegroundColor Gray

    # Generate report
    $ReportPath = Join-Path $ProjectRoot "docs" "reports" "smoke-ue-$DateStamp.txt"
    $ReportContent = @"
StallFreeCapture Unreal Automation Smoke Test Report
Generated: $Timestamp
Test: StallFreeCapture.Version
Mode: -nullrhi -unattended -nosplash
Duration: $($TestDuration.TotalSeconds.ToString('F2'))s
Unreal Exit Code: $UnrealExitCode
Exit Code: $(if ($TestPassed) { "0" } else { "1" })
Result: $(if ($TestPassed) { "PASSED" } else { "FAILED" })

$TestDetails

Command:
UnrealEditor-Cmd "$UProjectPath" -ExecCmds="Automation RunTests StallFreeCapture.Version;Quit" -nullrhi -unattended -nosplash -testexit="Automation Test Queue Empty" -ReportExportPath="$ReportExportPath" -log

Report export path: $ReportExportPath
Index JSON: $IndexJsonPath
"@

    New-Item -ItemType Directory -Force -Path (Split-Path $ReportPath) | Out-Null
    $ReportContent | Out-File -FilePath $ReportPath -Encoding UTF8
    Write-Host ""
    Write-Host "Report saved to: $ReportPath" -ForegroundColor Gray

    exit $(if ($TestPassed) { 0 } else { 1 })

} else {
    # ===== SFC_CORE C++ TESTS =====
    
    $SfcCoreRoot = Join-Path $ProjectRoot "Plugins" "StallFreeCapture" "ThirdParty" "sfc_core"

    if (-not (Test-Path $SfcCoreRoot)) {
        Write-Host "ERROR: sfc_core directory not found at: $SfcCoreRoot" -ForegroundColor Red
        $ReportPath = Join-Path $ProjectRoot "docs" "reports" "smoke-core-$DateStamp.txt"
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
        $ReportPath = Join-Path $ProjectRoot "docs" "reports" "smoke-core-$DateStamp.txt"
        $ReportContent = @"
StallFreeCapture sfc_core Smoke Test Report
Generated: $Timestamp
Preset: $Preset
Working directory: $SfcCoreRoot
$(if ($TestDuration) { "Test duration: $($TestDuration.TotalSeconds.ToString('F2'))s" } else { "Tests did not run (build or configure failed)" })
Exit Code: $(if ($OverallSuccess) { "0" } else { if ($TestExitCode -ne 0) { $TestExitCode } elseif ($BuildExitCode -ne 0) { $BuildExitCode } else { "1" } })
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
