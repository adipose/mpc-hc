<#
.SYNOPSIS
    The unit suite's entry point for the orchestrator.

.DESCRIPTION
    A thin wrapper over ..\..\unit\Invoke-UnitTests.ps1: it builds the native
    test executable (which pulls in the player's DSUtil, Subtitles and SubPic
    static libraries as project references) and runs it, translating the exe's
    JSON result into the orchestrator's pass/fail counts.

    This tier runs no player and needs no test rig, so the probe reports
    NeedsRig = $false and the orchestrator claims no guest on its behalf. It is
    also the one suite that runs on a bare bench with no emulator submodule.

    An expected failure -- a test that documents a bug present in the code
    under test today -- is not a suite failure: it is reported in Notes and
    counted as skipped, the way the dvb suite reports an assertion it cannot
    apply. An unexpected pass (a bug marker that should be removed) is a
    failure, because the exe returns non-zero for it.
#>
[CmdletBinding()]
param(
    [switch] $Probe,
    [string] $VMName = '',
    [string] $OutDir = (Join-Path $PSScriptRoot 'results'),
    [string] $PlayerBinary
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$description = 'Native unit tests over DSUtil/Subtitles/SubPic: parsers and helpers, fixture in and struct out, no player and no rig'
$runner = (Resolve-Path (Join-Path $PSScriptRoot '..\..\unit\Invoke-UnitTests.ps1')).Path

if ($Probe) {
    # Ready whenever the toolchain can build the project: a Visual Studio with
    # the C++ toolset, and nasm on PATH for the libass assembly kernels. The
    # submodules are fetched by the build step itself (-InitSubmodules), so a
    # bare checkout still probes ready.
    $ready = $true; $reason = ''
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (-not (Test-Path $vswhere)) {
        $ready = $false; $reason = 'Visual Studio not found (vswhere.exe missing)'
    } elseif (-not (& $vswhere -latest -prerelease -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath)) {
        $ready = $false; $reason = 'no Visual Studio with the C++ toolset installed'
    } elseif (-not (Get-Command nasm.exe -ErrorAction SilentlyContinue)) {
        $ready = $false; $reason = 'nasm.exe is not on PATH (libass needs it)'
    }
    return [pscustomobject]@{ Suite = 'unit'; Description = $description; Ready = $ready; Reason = $reason; NeedsRig = $false }
}

New-Item -ItemType Directory -Force $OutDir | Out-Null

$passed = 0; $failed = 0; $skipped = 0
$notes = [System.Collections.Generic.List[string]]::new()
function Note { param([string] $Colour, [string] $Text) $notes.Add($Text); Write-Host "  $Text" -ForegroundColor $Colour }

# Build and run. -InitSubmodules so a bare checkout fetches the libass chain,
# tinyxml2 and stb the libraries need. A throw here is an infrastructure
# failure (build broke), which the orchestrator records distinctly.
$run = & $runner -InitSubmodules -OutDir $OutDir -PassThru
$r = $run.Results
if (-not $r) {
    throw "unit tests produced no result JSON (exit $($run.ExitCode))"
}

$passed = [int]$r.passed
$failed = [int]$r.failed + [int]$r.unexpectedPasses
$skipped = [int]$r.expectedFailures

Note ($(if ($failed) { 'Red' } else { 'Green' })) "$($r.total) tests: $passed passed, $failed failed, $($r.expectedFailures) expected failure(s)"

foreach ($t in @($r.tests | Where-Object { $_.status -eq 'failed' -or $_.status -eq 'unexpected-pass' })) {
    $first = if ($t.failures) { $t.failures[0].message.Split([char]10)[0] } else { $t.status }
    Note Red "FAIL $($t.name): $first"
}
foreach ($t in @($r.tests | Where-Object { $_.status -eq 'expected-failure' })) {
    Note DarkGray "documents a current bug: $($t.name) -- $($t.expectedFailure)"
}

[pscustomobject]@{ Suite = 'unit'; Passed = $passed; Failed = $failed; Skipped = $skipped; Notes = $notes }
