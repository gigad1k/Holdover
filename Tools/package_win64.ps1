<#
.SYNOPSIS
    Packaged Win64 build of HOLDOVER via RunUAT BuildCookRun (Phase 10a).

.DESCRIPTION
    Produces a runnable packaged Windows build (build + cook + stage + pak +
    archive), then optionally smoke-tests it by launching the packaged game
    headless for a few seconds and scanning its log for fatal errors.

    Exit codes:
        0 = package (and smoke test, unless -SkipSmokeTest) succeeded
        1 = BuildCookRun failed
        2 = packaged executable not found / smoke test failed

    This script never touches Steamworks, store pages or accounts (NEEDS_ALI #6).

.PARAMETER UERoot
    UE 5.8 install root. Default: the UE 5.8 install on this machine.

.PARAMETER Project
    Full path to Holdover.uproject. Default: derived from this script's location.

.PARAMETER Config
    Client configuration to package. Default 'Development' (the Phase 10 ask).
    'Shipping' is accepted but not the demo target and may need extra settings.

.PARAMETER ArchiveDir
    Where the packaged build is archived. Default: <project>\Builds.

.PARAMETER Iterate
    Faster recook (-iterate) when only content changed.

.PARAMETER SkipSmokeTest
    Package only; do not launch the result.

.PARAMETER SmokeSeconds
    How long the smoke test leaves the packaged game running. Default 20.

.EXAMPLE
    .\Tools\package_win64.ps1
    Full package + smoke test.

.EXAMPLE
    .\Tools\package_win64.ps1 -Iterate -SkipSmokeTest
    Quick repackage of changed content.
#>
param(
    [string]$UERoot = "C:\Program Files\Epic Games\UE_5.8",
    [string]$Project = "",
    [string]$Config = "Development",
    [string]$ArchiveDir = "",
    [switch]$Iterate,
    [switch]$SkipSmokeTest,
    [int]$SmokeSeconds = 20
)

$ErrorActionPreference = "Stop"
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$RootDir = Split-Path -Parent $ScriptDir

if (-not $Project) { $Project = Join-Path $RootDir "Holdover.uproject" }
if (-not $ArchiveDir) { $ArchiveDir = Join-Path $RootDir "Builds" }
$Project = (Resolve-Path $Project).Path
$UAT = Join-Path $UERoot "Engine\Build\BatchFiles\RunUAT.bat"

Write-Host "======================================================================"
Write-Host "HOLDOVER packaged Win64 build (Phase 10a)"
Write-Host "======================================================================"
Write-Host "UAT     : $UAT"
Write-Host "Project : $Project"
Write-Host "Config  : $Config"
Write-Host "Archive : $ArchiveDir"
Write-Host ""

if (-not (Test-Path $UAT)) {
    Write-Host "ERROR: RunUAT.bat not found at $UAT"
    exit 1
}

$UatArgs = @(
    "BuildCookRun",
    "-project=`"$Project`"",
    "-platform=Win64",
    "-clientconfig=$Config",
    "-build",
    "-cook",
    "-stage",
    "-pak",
    "-archive",
    "-archivedirectory=`"$ArchiveDir`"",
    "-utf8output",
    "-unattended"
)
if ($Iterate) { $UatArgs += "-iterate" }

Write-Host "Command : $UAT $($UatArgs -join ' ')"
Write-Host ""
Write-Host "--- BuildCookRun starting (first cook can take a while) ---"

& $UAT @UatArgs
$UatExit = $LASTEXITCODE
Write-Host ""
Write-Host "--- BuildCookRun finished with exit code $UatExit ---"

if ($UatExit -ne 0) {
    Write-Host "RESULT: FAIL - BuildCookRun returned $UatExit"
    exit 1
}

# Locate the packaged executable (archive layout: <ArchiveDir>\<Platform>\...).
$Exe = Get-ChildItem -Path $ArchiveDir -Filter "Holdover.exe" -Recurse -ErrorAction SilentlyContinue |
    Select-Object -First 1
if (-not $Exe) {
    Write-Host "RESULT: FAIL - Holdover.exe not found under $ArchiveDir"
    exit 2
}
Write-Host "Packaged exe: $($Exe.FullName)"

if ($SkipSmokeTest) {
    Write-Host "RESULT: PASS (smoke test skipped)"
    exit 0
}

# --- Smoke test: launch the packaged game headless, scan its log ---
$SmokeLog = Join-Path $RootDir "Saved\Logs\package_smoke.log"
if (Test-Path $SmokeLog) { Remove-Item $SmokeLog -Force }

Write-Host ""
Write-Host "--- Smoke test: launching packaged game for $SmokeSeconds s (headless) ---"
$GameArgs = @(
    "-nullrhi",
    "-unattended",
    "-nosplash",
    "-log",
    "-AbsLog=`"$SmokeLog`""
)
$Proc = Start-Process -FilePath $Exe.FullName -ArgumentList $GameArgs -PassThru
Start-Sleep -Seconds $SmokeSeconds
if (-not $Proc.HasExited) {
    Stop-Process -Id $Proc.Id -Force
    Write-Host "Smoke test: stopped the game after $SmokeSeconds s (was still running = good sign)"
} else {
    Write-Host "Smoke test: game exited on its own after $SmokeSeconds s with code $($Proc.ExitCode)"
}

if (-not (Test-Path $SmokeLog)) {
    Write-Host "RESULT: FAIL - smoke test log not written: $SmokeLog"
    exit 2
}

$FatalLines = Select-String -Path $SmokeLog -Pattern "Fatal error|Assertion failed|Critical error|appError called" -ErrorAction SilentlyContinue
if ($FatalLines) {
    Write-Host "Smoke test: FATAL issues found in $($SmokeLog):"
    $FatalLines | ForEach-Object { Write-Host "  $($_.Line)" }
    Write-Host "RESULT: FAIL - packaged build crashed"
    exit 2
}

Write-Host "Smoke test: no fatal errors in $SmokeLog"
Write-Host "RESULT: PASS - packaged Win64 $Config build is runnable"
Write-Host "NOTE: a full shift play-through of the packaged build still needs a human"
Write-Host "      (see Docs/DEMO_CHECKLIST.md)."
exit 0
