<#
.SYNOPSIS
    Headless automation test runner for the HOLDOVER UE 5.8 project (PROC-4).

.DESCRIPTION
    Runs the Holdover automation suite in a separate headless UnrealEditor-Cmd
    process (safe while a GUI editor has the project open), waits for completion,
    parses the engine output log for per-test results, prints a per-test summary
    and exits with a CI-friendly code:

        0 = every expected test ran and returned Result={Success}
        1 = at least one test result was not Success (failures reported verbatim)
        2 = results could not be determined (no log / no result lines / expected
            test missing without an explicit failure)

    Engine facts this script relies on (verified against UE 5.8 sources):
      - Console syntax        : "Automation RunTests <filter>" (AutomationCommandline.cpp)
                                filter is a substring match; StartsWith:/Group:/^...$ also exist.
      - Per-test result line  : "Test Completed. Result={Success|Fail|NotRun|InProcess|Skipped}
                                 Name={<display>} Path={<full.path>}"  (AutomationControllerManager.cpp)
      - Per-test event block  : "BeginEvents: <path>" ... "EndEvents: <path>"
      - Completion marker     : "...Automation Test Queue Empty <N> tests performed."
      - Termination           : -TestExit="<phrase>" force-exits (after flushing the log) when
                                the phrase appears in the log (LaunchEngineLoop.cpp).
      - Log location          : -abslog=<path>, else <project>\Saved\Logs\<ProjectName>.log;
                                if that file is held by another process (e.g. the running GUI
                                editor) the engine silently falls back to <name>_2.log, _3.log...

    The engine exit code is NOT the test verdict (it is 0/3 regardless of test
    results), so the verdict is computed from the parsed result lines.

.PARAMETER TestFilter
    Automation filter passed to "Automation RunTests". Default 'Holdover'
    (substring match - runs the whole Holdover suite).

.PARAMETER UEPath
    Full path to UnrealEditor-Cmd.exe. Default: the UE 5.8 install on this machine.

.PARAMETER Project
    Full path to Holdover.uproject. Default: derived from this script's location
    (Tools\..\Holdover.uproject).

.PARAMETER AbsLog
    Optional. If set, passed to the editor as -abslog=<AbsLog> and used as the
    primary parse target. Must end in .log or .txt. If not set, the run uses the
    engine default (<project>\Saved\Logs\Holdover.log) and the script auto-discovers
    the file the engine actually wrote (see _2.log fallback above).

.PARAMETER TimeoutSec
    Kill the editor process after this many seconds and treat it as a blocked run
    (one retry is attempted). Default 900.

.EXAMPLE
    .\Tools\run_tests.ps1
    Runs the full Holdover suite with default paths.

.EXAMPLE
    .\Tools\run_tests.ps1 -TestFilter 'Holdover.Treatment' -TimeoutSec 600
    Runs only the Treatment tests with a 10 minute timeout.

.NOTES
    PowerShell 5.1 compatible. Exit codes: 0 all-pass, 1 any failure, 2 undetermined.
#>
[CmdletBinding()]
param(
    [string]$TestFilter = 'Holdover',
    [string]$UEPath     = 'D:\Gamez\EPIC\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe',
    [string]$Project    = '',
    [string]$AbsLog     = '',
    [int]$TimeoutSec    = 900
)

Set-StrictMode -Version 2.0
$ErrorActionPreference = 'Stop'

# ---------------------------------------------------------------------------
# The 7 tests expected under the 'Holdover' filter (PROC-4 / CODE_REVIEW).
# ---------------------------------------------------------------------------
$ExpectedTests = @(
    'Holdover.Shift.DepartureOnlyOnceAndValid',
    'Holdover.Shift.ScoringAndRestart',
    'Holdover.Treatment.FluidRules',
    'Holdover.Treatment.HoldoverStateMachine',
    'Holdover.Treatment.SectionIndependenceAndClamp',
    'Holdover.Truck.BoomClampAndApply',
    'Holdover.UI.BoardStateColor'
)

# Result line format (AutomationControllerManager.cpp, AutomationStateFormat):
#   Test Completed. Result={State} Name={Display} Path={Full.Path}
$ResultRegex = New-Object System.Text.RegularExpressions.Regex(
    'Test Completed\. Result=\{(?<state>[^}]*)\} Name=\{(?<name>[^}]*)\} Path=\{(?<path>[^}]*)\}',
    [System.Text.RegularExpressions.RegexOptions]::Compiled)

$TestStartedRegex = New-Object System.Text.RegularExpressions.Regex(
    'Test Started\. Name=\{(?<name>[^}]*)\} Path=\{(?<path>[^}]*)\}',
    [System.Text.RegularExpressions.RegexOptions]::Compiled)

function Write-Banner([string]$Text) {
    Write-Output ''
    Write-Output ('=' * 70)
    Write-Output $Text
    Write-Output ('=' * 70)
}

function Resolve-ProjectPath {
    if ($Project -ne '') { return $Project }
    $derived = Join-Path (Split-Path -Parent $PSScriptRoot) 'Holdover.uproject'
    if (Test-Path -LiteralPath $derived) { return $derived }
    return 'D:\Projects\HOLDOVER\Holdover.uproject'
}

# Returns candidate log files (excluding engine "*-backup-*" copies) written
# during this run attempt, ordered from most to least likely to be our log.
# A candidate qualifies only if the log STARTED during this attempt (checked via
# the timestamp of its first line) - this discriminates our run's log from the
# log of a GUI editor that happens to be running and holding Holdover.log.
function Find-RunLog {
    param(
        [string]$LogDir,
        [string]$PreferredPath,
        [datetime]$AttemptStart,
        [string]$Filter
    )
    $marker = "RunTests='$Filter' Queued"
    $candidates = @()
    if (Test-Path -LiteralPath $LogDir) {
        $candidates = @(Get-ChildItem -LiteralPath $LogDir -Filter '*.log' -ErrorAction SilentlyContinue |
            Where-Object {
                $_.Name -notmatch '-backup-' -and
                $_.LastWriteTime -ge $AttemptStart.AddSeconds(-5)
            } |
            Sort-Object LastWriteTime -Descending)
    }

    $scored = @()
    foreach ($f in $candidates) {
        $text = $null
        try {
            $text = [System.IO.File]::ReadAllText($f.FullName)
        } catch {
            continue
        }
        if ($text -notmatch [regex]::Escape('Log file open')) { continue }

        # UE log line prefix: [2026.10.29-21.35.00:123][  0]...
        $startedDuringAttempt = $false
        $m = [regex]::Match($text, '\[(\d{4})\.(\d{2})\.(\d{2})-(\d{2})\.(\d{2})\.(\d{2})')
        if ($m.Success) {
            $logStart = Get-Date -Year ([int]$m.Groups[1].Value) -Month ([int]$m.Groups[2].Value) -Day ([int]$m.Groups[3].Value) `
                -Hour ([int]$m.Groups[4].Value) -Minute ([int]$m.Groups[5].Value) -Second ([int]$m.Groups[6].Value)
            if ($logStart -ge $AttemptStart.AddSeconds(-30)) { $startedDuringAttempt = $true }
        }
        if (-not $startedDuringAttempt) { continue }

        $hasRunMarker = $text.Contains($marker) -or $text.Contains('Automation RunTests')
        $hasAnyResult = $text.Contains('Test Completed. Result=')
        if (-not ($hasRunMarker -or $hasAnyResult)) { continue }

        $score = 0
        if ($hasRunMarker) { $score += 10 }
        if ($hasAnyResult) { $score += 5 }
        if ($PreferredPath -and ($f.FullName -ieq $PreferredPath)) { $score += 100 }
        $scored += New-Object psobject -Property @{ File = $f; Score = $score }
    }

    if ($scored.Count -eq 0) { return $null }
    $best = $scored | Sort-Object -Property @{Expression='Score'; Descending=$true}, @{Expression={$_.File.LastWriteTime}; Descending=$true} | Select-Object -First 1
    return $best.File.FullName
}

# Parses one engine log. Returns an object with Results / Completion / notes.
function Parse-RunLog {
    param([string]$LogPath, [string]$Filter)

    $lines = [System.IO.File]::ReadAllLines($LogPath)
    $results = @()
    $engineFoundCount = $null
    $engineTestCount  = $null
    $engineExitLine   = $null
    $errorLines       = @()

    $beginEventsPath = $null
    $eventsForPath   = @{}

    for ($i = 0; $i -lt $lines.Count; $i++) {
        $line = $lines[$i]

        $m = $ResultRegex.Match($line)
        if ($m.Success) {
            $results += New-Object psobject -Property @{
                State = $m.Groups['state'].Value.Trim()
                Name  = $m.Groups['name'].Value.Trim()
                Path  = $m.Groups['path'].Value.Trim()
                Raw   = $line
            }
            continue
        }

        if ($line -match "Found (\d+) automation tests based on '$([regex]::Escape($Filter))'") {
            $engineFoundCount = [int]$Matches[1]
        }
        if ($line -match '\.\.\.Automation Test Queue Empty (\d+) tests performed') {
            $engineTestCount = [int]$Matches[1]
        }
        if ($line -match '\*\*\*\* TEST COMPLETE\. EXIT CODE: (-?\d+) \*\*\*\*') {
            $engineExitLine = $line.Trim()
        }

        # Capture per-test event blocks so failures can be reported verbatim.
        if ($line -match "^.*BeginEvents: (?<p>.+?)\s*$") {
            $beginEventsPath = $Matches['p'].Trim()
            $eventsForPath[$beginEventsPath] = New-Object System.Collections.ArrayList
            continue
        }
        if ($line -match "^.*EndEvents: (?<p>.+?)\s*$") {
            $beginEventsPath = $null
            continue
        }
        if ($beginEventsPath -ne $null) {
            [void]$eventsForPath[$beginEventsPath].Add($line)
            continue
        }

        # Keep engine-level error lines that indicate the run itself broke.
        if ($line -match 'LogAutomationCommandLine: Error:' -or
            $line -match 'No automation tests matched' -or
            $line -match 'Failed to find workers') {
            $errorLines += $line.Trim()
        }
    }

    return New-Object psobject -Property @{
        LogPath          = $LogPath
        Results          = $results
        EngineFoundCount = $engineFoundCount
        EngineTestCount  = $engineTestCount
        EngineExitLine   = $engineExitLine
        ErrorLines       = $errorLines
        EventsForPath    = $eventsForPath
    }
}

# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------
$Project = Resolve-ProjectPath
$LogDir  = Join-Path (Split-Path -Parent $Project) 'Saved\Logs'

Write-Banner 'HOLDOVER headless automation runner (PROC-4)'
Write-Output ("UE       : {0}" -f $UEPath)
Write-Output ("Project  : {0}" -f $Project)
Write-Output ("Filter   : {0}" -f $TestFilter)
Write-Output ("Log dir  : {0}" -f $LogDir)
if ($AbsLog -ne '') { Write-Output ("AbsLog   : {0}" -f $AbsLog) }

if (-not (Test-Path -LiteralPath $UEPath)) {
    Write-Output ("ERROR: UnrealEditor-Cmd.exe not found at '{0}'" -f $UEPath)
    exit 2
}
if (-not (Test-Path -LiteralPath $Project)) {
    Write-Output ("ERROR: project file not found at '{0}'" -f $Project)
    exit 2
}

$quotedProject = '"{0}"' -f $Project
$execCmds      = '-ExecCmds="Automation RunTests {0}"' -f $TestFilter
$testExit      = '-TestExit="Automation Test Queue Empty"'
$commonArgs    = '{0} {1} {2} -nullrhi -unattended -nosplash -log' -f $quotedProject, $execCmds, $testExit

if ($AbsLog -ne '') {
    if ($AbsLog -notmatch '\.(log|txt)$') {
        Write-Output "ERROR: -AbsLog must end in .log or .txt"
        exit 2
    }
    $absLogDir = Split-Path -Parent $AbsLog
    if ($absLogDir -ne '' -and -not (Test-Path -LiteralPath $absLogDir)) {
        New-Item -ItemType Directory -Path $absLogDir -Force | Out-Null
    }
    $commonArgs = '{0} -abslog="{1}"' -f $commonArgs, $AbsLog
}

$fullCommand = '"{0}" {1}' -f $UEPath, $commonArgs
Write-Output ''
Write-Output ("Command  : {0}" -f $fullCommand)

$parsed        = $null
$attemptCount  = 0
$blockedNote   = ''

while ($attemptCount -lt 2 -and $parsed -eq $null) {
    $attemptCount++
    $attemptStart = Get-Date
    Write-Output ''
    Write-Output ("--- attempt {0}: launching editor (timeout {1}s) ---" -f $attemptCount, $TimeoutSec)

    $proc = $null
    $waitInfo = $null
    try {
        $psi = New-Object System.Diagnostics.ProcessStartInfo
        $psi.FileName = $UEPath
        $psi.Arguments = $commonArgs
        $psi.UseShellExecute = $false
        $psi.WorkingDirectory = Split-Path -Parent $UEPath
        $proc = [System.Diagnostics.Process]::Start($psi)
    } catch {
        $blockedNote = ("launch failed: {0}" -f $_.Exception.Message)
        Write-Output ("NOTE: {0}" -f $blockedNote)
        Start-Sleep -Seconds 3
        continue
    }

    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    $exited = $proc.WaitForExit($TimeoutSec * 1000)
    $sw.Stop()
    if (-not $exited) {
        $blockedNote = ("process still running after {0}s - possible block on a held file; killing and retrying once" -f $TimeoutSec)
        Write-Output ("NOTE: {0}" -f $blockedNote)
        try { $proc.Kill() } catch { }
        try { $proc.WaitForExit(30000) | Out-Null } catch { }
    }
    $procExitCode = $null
    try { if ($proc.HasExited) { $procExitCode = $proc.ExitCode } } catch { }
    Write-Output ("Process exited after {0}s (engine exit code: {1})" -f [math]::Round($sw.Elapsed.TotalSeconds, 1), $(if ($procExitCode -ne $null) { $procExitCode } else { 'n/a' }))

    # Locate the log the engine actually wrote (default or _2/_3 fallback).
    $preferred = $null
    if ($AbsLog -ne '') { $preferred = $AbsLog }
    else { $preferred = Join-Path $LogDir 'Holdover.log' }

    $logPath = Find-RunLog -LogDir $LogDir -PreferredPath $preferred -AttemptStart $attemptStart -Filter $TestFilter
    if ($logPath -eq $null) {
        $blockedNote = 'no engine log produced for this attempt'
        Write-Output ("NOTE: {0}" -f $blockedNote)
        continue
    }

    Write-Output ("Parsed log: {0}" -f $logPath)
    $attemptParsed = Parse-RunLog -LogPath $logPath -Filter $TestFilter

    if ($attemptParsed.Results.Count -eq 0 -and $attemptParsed.EngineFoundCount -eq $null -and -not $exited) {
        # Timed out and produced nothing parseable: retry.
        continue
    }
    $parsed = $attemptParsed
}

Write-Banner 'Engine output (key lines)'
if ($parsed -eq $null) {
    Write-Output 'ERROR: no parseable automation output produced.'
    if ($blockedNote -ne '') { Write-Output ("NOTE: {0}" -f $blockedNote) }
    exit 2
}
if ($parsed.EngineFoundCount -ne $null) {
    Write-Output ("Found {0} automation tests based on '{1}'" -f $parsed.EngineFoundCount, $TestFilter)
} else {
    Write-Output "WARNING: 'Found N automation tests' line not seen (filter matched nothing?)"
}
if ($parsed.EngineTestCount -ne $null) {
    Write-Output ("...Automation Test Queue Empty {0} tests performed." -f $parsed.EngineTestCount)
} else {
    Write-Output "WARNING: '...Automation Test Queue Empty' completion line not seen (run may have been cut short)"
}
if ($parsed.EngineExitLine -ne $null) { Write-Output $parsed.EngineExitLine }
foreach ($e in $parsed.ErrorLines) { Write-Output ("ENGINE ERROR: {0}" -f $e) }
if ($blockedNote -ne '') { Write-Output ("NOTE: {0}" -f $blockedNote) }

# ---------------------------------------------------------------------------
# Per-test summary
# ---------------------------------------------------------------------------
$byPath = @{}
foreach ($r in $parsed.Results) {
    $byPath[$r.Path] = $r   # last occurrence wins (multi-pass runs)
}

Write-Banner ('Per-test summary (expected {0})' -f $ExpectedTests.Count)
$missing  = @()
$failed   = @()
$passed   = 0
$matchedPaths = @{}

foreach ($t in $ExpectedTests) {
    $hit = $null
    $leaf = ($t -split '\.')[-1]
    if ($byPath.ContainsKey($t)) {
        $hit = $byPath[$t]
    } else {
        foreach ($k in $byPath.Keys) {
            if ($k.EndsWith('.' + $t) -or ($byPath[$k].Name -eq $leaf -and $k.Contains($leaf))) {
                $hit = $byPath[$k]
                break
            }
        }
    }

    if ($hit -eq $null) {
        Write-Output ("[MISSING] {0}" -f $t)
        $missing += $t
        continue
    }
    $matchedPaths[$hit.Path] = $true
    if ($hit.State -eq 'Success') {
        Write-Output ("[PASS]    {0}   Result={{{1}}}" -f $t, $hit.State)
        $passed++
    } else {
        Write-Output ("[FAIL]    {0}   Result={{{1}}}" -f $t, $hit.State)
        $failed += $hit
    }
}

$extras = @($parsed.Results | Where-Object { -not $matchedPaths.ContainsKey($_.Path) })
if ($extras.Count -gt 0) {
    Write-Output ''
    Write-Output ("Additional tests matched by filter '{0}' (outside the expected {1}):" -f $TestFilter, $ExpectedTests.Count)
    foreach ($x in $extras) {
        $tag = $(if ($x.State -eq 'Success') { '[PASS]' } else { '[FAIL]' })
        Write-Output ("{0}    {1}   Result={{{2}}}" -f $tag, $x.Path, $x.State)
        if ($x.State -ne 'Success') { $failed += $x }
    }
}

# Verbatim failure report
if ($failed.Count -gt 0) {
    Write-Banner 'Failure detail (verbatim)'
    foreach ($f in $failed) {
        Write-Output $f.Raw
        if ($parsed.EventsForPath.ContainsKey($f.Path)) {
            foreach ($ev in $parsed.EventsForPath[$f.Path]) {
                if ($ev -match 'Error|Warning|Expected|Actual|Failed') { Write-Output $ev }
            }
        }
        Write-Output ''
    }
}

# ---------------------------------------------------------------------------
# Verdict
# ---------------------------------------------------------------------------
$totalResults = $parsed.Results.Count
Write-Banner 'Result'
Write-Output ("Test results parsed : {0}" -f $totalResults)
Write-Output ("Expected passed     : {0}/{1}" -f $passed, $ExpectedTests.Count)
Write-Output ("Failures            : {0}" -f $failed.Count)
Write-Output ("Missing expected    : {0}" -f $missing.Count)

if ($totalResults -eq 0) {
    Write-Output 'RESULT: UNDETERMINED - no test results could be parsed from the log.'
    exit 2
}
if ($failed.Count -gt 0) {
    Write-Output ("RESULT: FAIL - {0} test(s) did not return Result={{Success}}." -f $failed.Count)
    exit 1
}
if ($missing.Count -gt 0) {
    Write-Output ("RESULT: UNDETERMINED - {0} expected test(s) never reported a result." -f $missing.Count)
    exit 2
}
Write-Output ('RESULT: PASS - all {0} expected tests returned Result={{Success}}.' -f $ExpectedTests.Count)
exit 0
