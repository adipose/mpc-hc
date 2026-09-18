<#
.SYNOPSIS
    The dvb-playback suite: tunes channels on the virtual tuner and asserts on
    the audio that reaches the virtual sound card.

.DESCRIPTION
    The dvb suite proves the player DECODES a broadcast correctly: its scans
    are headless and never build a playback graph. This suite covers the other
    half -- tune a channel, render it -- which nothing else in the tree does.
    Issues in this area (#74 "audio is lost after changing a channel", #295,
    #372) could not be tested on the rig before it had an audio device.

    Every emulator test channel carries its own sine tone (see
    tests\emulator\tools\New-TestStreams.ps1), so which channel is actually
    playing is audible to a test: tune "Test Channel 3", hear 1320 Hz.

    A case is two player runs from one fresh portable profile:
      1. /dvbscan ... /dvbscansave   scan headlessly AND store the channels
      2. /device                      open the tuner; it tunes LastChannel
    with the profile's LastChannel set between them to pick the channel. The
    player is given a fixed time on air and then closed, since live television
    never ends by itself.

    Needs a player build with /dvbscansave (a headless scan that stores what
    it found; without it a scripted run has no channel list to tune from).
    A build without the switch is reported and the suite counts as skipped.

.PARAMETER Standard
    Which virtual tuner to use. DVBC by default: five channels, the quickest
    scan.
#>
[CmdletBinding()]
param(
    [switch] $Probe,
    [string] $VMName = '',
    [string] $OutDir = (Join-Path $PSScriptRoot 'results'),
    [string] $PlayerBinary,
    [ValidateSet('DVBT', 'DVBC', 'DVBS', 'ATSC')] [string] $Standard = 'DVBC',
    [int] $OnAirSec = 14
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$description = 'Tunes channels on the virtual tuner; asserts each channel''s tone reaches the virtual sound card'
$testsRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$repoRoot = Split-Path $testsRoot -Parent
$vaudio = if ($env:MPC_TEST_VAUDIO) { $env:MPC_TEST_VAUDIO } else { Join-Path $testsRoot 'vaudio' }
$transport = Join-Path $testsRoot 'emulator\tools\GuestTransport.ps1'
$ranges = Join-Path $testsRoot 'suites\dvb\scan-ranges.psd1'

# What each emulator test channel sounds like. Mirrors the channel plan in
# tests\emulator\tools\New-TestStreams.ps1; a channel not listed here is tuned
# and must produce sound, but its pitch is not asserted.
$channelTones = @{
    'Test Channel 1' = 440; 'Test Channel 2' = 880; 'Test Channel 3' = 1320
    'Test Channel 4' = 1760; 'Test Channel 5' = 2200
}

if ($Probe) {
    $ready = $true; $reason = ''
    if (-not (Test-Path $transport)) { $ready = $false; $reason = 'emulator submodule not initialised (git submodule update --init tests/emulator)' }
    elseif (-not (Test-Path (Join-Path $vaudio 'tests\wavcheck.py'))) { $ready = $false; $reason = 'vaudio submodule not initialised (git submodule update --init tests/vaudio)' }
    elseif (-not (Get-Command python -ErrorAction SilentlyContinue)) { $ready = $false; $reason = 'python (with numpy) not on PATH' }
    return [pscustomobject]@{ Suite = 'dvb-playback'; Description = $description; Ready = $ready; Reason = $reason }
}

New-Item -ItemType Directory -Force $OutDir | Out-Null
$OutDir = (Resolve-Path $OutDir).Path
$passed = 0; $failed = 0; $skipped = 0
$notes = [System.Collections.Generic.List[string]]::new()
function Note { param([string] $Colour, [string] $Text) $notes.Add($Text); Write-Host "  $Text" -ForegroundColor $Colour }
function Finish { [pscustomobject]@{ Suite = 'dvb-playback'; Passed = $passed; Failed = $failed; Skipped = $skipped; Notes = $notes } }
function Complete-Case {
    param([string] $Name, [string[]] $Problems)
    $Problems = @($Problems | Where-Object { $_ })
    if ($Problems.Count -eq 0) { $script:passed++; Note Green "PASS $Name" }
    else { $script:failed++; Note Red "FAIL ${Name}: $($Problems -join ' | ')" }
}

# --- player -------------------------------------------------------------------

$playerExe = if ($PlayerBinary) { (Resolve-Path $PlayerBinary).Path } else { Join-Path $repoRoot 'bin\mpc-hc_x64\mpc-hc64.exe' }
if (-not (Test-Path $playerExe)) { throw 'No player build: pass -PlayerBinary or build this repository (bin\mpc-hc_x64\mpc-hc64.exe).' }
$playerDir = Split-Path $playerExe -Parent

# Does this build have the verb? The player is a Unicode binary, so look for the switch as UTF-16 -- and look for
# a switch that is certainly there as well, because a probe that cannot find a known-present string is measuring
# nothing (tests\emulator docs, and two sessions' wasted evenings).
$bytes = [IO.File]::ReadAllBytes($playerExe)
$text = [Text.Encoding]::Unicode.GetString($bytes)
if ($text.IndexOf('dvbscanout', [StringComparison]::Ordinal) -lt 0) {
    throw "Could not find the known switch 'dvbscanout' in $playerExe; the probe is broken or the build predates the headless scan (#4138)."
}
if ($text.IndexOf('dvbscansave', [StringComparison]::Ordinal) -lt 0) {
    $skipped = 1
    Note Yellow "not run: $playerExe has no /dvbscansave switch, so a scripted run cannot store a channel list to tune from"
    return (Finish)
}

# --- target -------------------------------------------------------------------

. $transport
$cfg = Get-TestBedConfig
$range = (Import-PowerShellDataFile $ranges)[$Standard]
$session = Connect-TestGuest -Guest $VMName
try {
    $target = Invoke-Command -Session $session -ArgumentList $Standard {
        param($standard)
        $moniker = $null
        $cat = 'HKLM:\SYSTEM\CurrentControlSet\Control\DeviceClasses\{71985f48-1ca1-11d3-9cc8-00c04f7971e0}'
        if (Test-Path -LiteralPath $cat) {
            # -LiteralPath throughout: the interface name contains '?', which the provider otherwise globs.
            foreach ($ifaceName in (Get-ChildItem -LiteralPath $cat -Name | Where-Object { $_ -match '#ROOT#MEDIA#' })) {
                foreach ($refName in (Get-ChildItem -LiteralPath "$cat\$ifaceName" -Name | Where-Object { $_ -match '^#\{' })) {
                    $friendly = (Get-ItemProperty -LiteralPath "$cat\$ifaceName\$refName\Device Parameters" -Name FriendlyName -ErrorAction SilentlyContinue).FriendlyName
                    if ($friendly -and (($friendly -replace '[- ]', '') -match $standard) -and -not $moniker) {
                        $moniker = ('@device:pnp:\\?\' + ($ifaceName -replace '^##\?#', '') + '\' + $refName.TrimStart('#')).ToLowerInvariant()
                    }
                }
            }
        }
        [pscustomobject]@{
            Moniker = $moniker
            Audio   = [bool](Get-PnpDevice -PresentOnly -ErrorAction SilentlyContinue | Where-Object { $_.HardwareID -contains 'ROOT\VAudioEndpoint' -and $_.Status -eq 'OK' })
            Console = (Get-CimInstance Win32_ComputerSystem).UserName
        }
    }
    if (-not $target.Audio) { $skipped = 1; Note Yellow 'not run: this guest has no virtual audio endpoint (tests\suites\playback\Install-OutputDevices.ps1 provisions one)'; return (Finish) }
    if (-not $target.Moniker) { $skipped = 1; Note Yellow "not run: no virtual $Standard tuner on this guest (emulator: Build-And-Deploy.ps1)"; return (Finish) }
    $consoleUser = if ($cfg.GuestConsoleUser) { $cfg.GuestConsoleUser } else { ($target.Console -split '\\')[-1] }
    if (-not $consoleUser) { throw 'Nobody is logged on at the guest console; the player needs a desktop.' }

    # Deploy: exe, icon library, LAV Filters (taken from beside the exe; any recent build's will do).
    $stage = Join-Path $OutDir 'player-stage'
    if (Test-Path $stage) { Get-ChildItem $stage -Recurse -File | ForEach-Object { [IO.File]::Delete($_.FullName) } }
    New-Item -ItemType Directory -Force $stage, (Join-Path $stage 'LAVFilters64') | Out-Null
    Copy-Item $playerExe $stage
    if (Test-Path (Join-Path $playerDir 'mpciconlib.dll')) { Copy-Item (Join-Path $playerDir 'mpciconlib.dll') $stage }
    $lav = Join-Path $playerDir 'LAVFilters64'
    if (-not (Test-Path $lav)) { throw "No LAVFilters64 beside $playerExe; the player needs its decoders to render a channel." }
    Get-ChildItem $lav -File | Where-Object { $_.Extension -in '.ax', '.dll', '.manifest' } | Copy-Item -Destination (Join-Path $stage 'LAVFilters64')
    $zip = Join-Path $OutDir 'player.zip'
    if (Test-Path $zip) { [IO.File]::Delete($zip) }
    Compress-Archive -Path (Join-Path $stage '*') -DestinationPath $zip

    Invoke-Command -Session $session {
        $ErrorActionPreference = 'Continue'
        foreach ($d in 'C:\mpc-test', 'C:\mpc-test\out') { if (-not (Test-Path $d)) { New-Item -ItemType Directory $d | Out-Null } }
        Get-Process mpc-hc64 -ErrorAction SilentlyContinue | Stop-Process -Force
    }
    Copy-Item -ToSession $session $zip 'C:\mpc-test\player.zip' -Force
    Copy-Item -ToSession $session (Join-Path $PSScriptRoot 'Run-TunedCase.guest.ps1') 'C:\mpc-test\' -Force
    Invoke-Command -Session $session { Expand-Archive 'C:\mpc-test\player.zip' 'C:\mpc-test\player' -Force }
    $version = Invoke-Command -Session $session { (Get-Item 'C:\mpc-test\player\mpc-hc64.exe').VersionInfo.ProductVersion }
    Note Gray "player under test: $version"
    Note Gray "tuner: $Standard"

    # Runs Run-TunedCase.guest.ps1 as the console user and returns its JSON, parsed.
    function Invoke-TunedRun {
        param([string] $Tag, [string] $ArgumentLine, [string] $Extra)
        $json = Invoke-Command -Session $session -ArgumentList $Tag, $ArgumentLine, $Extra, $consoleUser {
            param($tag, $argumentLine, $extra, $user)
            $ErrorActionPreference = 'Continue'
            Get-Process mpc-hc64 -ErrorAction SilentlyContinue | Stop-Process -Force
            $out = "C:\mpc-test\out\$tag.json"
            $taskArgs = "-NoProfile -ExecutionPolicy Bypass -File C:\mpc-test\Run-TunedCase.guest.ps1 -Exe C:\mpc-test\player\mpc-hc64.exe -ArgumentLine `"$($argumentLine.Replace('"','\"'))`" -Out $out $extra"
            $action = New-ScheduledTaskAction -Execute 'powershell.exe' -Argument $taskArgs
            $principal = New-ScheduledTaskPrincipal -UserId $user -LogonType Interactive
            Register-ScheduledTask -TaskName 'MpcDvbPlayback' -Action $action -Principal $principal -Force | Out-Null
            Start-ScheduledTask -TaskName 'MpcDvbPlayback'
            $deadline = (Get-Date).AddSeconds(900)
            while (-not (Test-Path $out) -and (Get-Date) -lt $deadline) { Start-Sleep -Seconds 2 }
            Unregister-ScheduledTask -TaskName 'MpcDvbPlayback' -Confirm:$false
            Get-Process mpc-hc64 -ErrorAction SilentlyContinue | Stop-Process -Force
            if (Test-Path $out) { Get-Content $out -Raw }
        }
        if (-not $json) { throw "run '$Tag' produced no result on the guest" }
        Set-Content (Join-Path $OutDir "$Tag.guest.json") $json
        $json | ConvertFrom-Json
    }

    # --- 1. scan and store ------------------------------------------------------
    #
    # A fresh portable profile. Three things in it are load-bearing: UpdaterAutoCheck (or the first-run prompt
    # blocks an unattended player), DefaultCapture=1 (digital; 0 opens the analog capture options instead), and
    # BDASymbolRate -- the player reads [DVBConfiguration2] only if that value is present there, and otherwise
    # falls back to the legacy section, silently ignoring the tuner named here.
    $ini = @(
        '[Settings]', 'UpdaterAutoCheck=0', 'KeepHistory=0', 'DefaultCapture=1', 'LogoFile=', 'ShowOSD=0',
        '[DVBConfiguration2]', "BDATuner=$($target.Moniker)", "BDASymbolRate=$($range.SymbolRate)",
        "BDABandWidth=$([int]($range.Bandwidth / 1000))", "BDAScanFreqStart=$($range.FreqStart)", "BDAScanFreqEnd=$($range.FreqEnd)",
        'BDAIgnoreEncryptedChannels=0', 'BDAUseOffset=0'
    ) -join "`r`n"
    Invoke-Command -Session $session -ArgumentList ($ini + "`r`n") {
        param($ini)
        Get-ChildItem 'C:\mpc-test\player' -Filter '*.ini' | ForEach-Object { [IO.File]::Delete($_.FullName) }
        [IO.File]::WriteAllText('C:\mpc-test\player\mpc-hc64.ini', $ini, [Text.Encoding]::Unicode)
    }

    $stamp = Get-Date -Format 'HHmmss'
    $scanOut = "C:\mpc-test\out\scan-$stamp.channels.json"
    $scanArgs = "/dvbscan $($range.FreqStart)-$($range.FreqEnd) /dvbscanout `"$scanOut`" /dvbbandwidth $($range.Bandwidth) /dvbscansave"
    if ($range.SymbolRate) { $scanArgs += " /dvbsymbolrate $($range.SymbolRate)" }
    $scan = Invoke-TunedRun -Tag "scan-$stamp" -ArgumentLine $scanArgs -Extra "-WaitForExit -ExitTimeoutSec $($range.TimeoutSec)"

    $stored = Invoke-Command -Session $session -ArgumentList $scanOut {
        param($scanOut)
        $channels = if (Test-Path $scanOut) { (Get-Content $scanOut -Raw | ConvertFrom-Json).channels } else { @() }
        $ini = Get-Content 'C:\mpc-test\player\mpc-hc64.ini'
        [pscustomobject]@{
            Channels  = @($channels | ForEach-Object { [pscustomobject]@{ Index = $_.index; Name = $_.name } })
            IniStored = @($ini | Where-Object { $_ -match '^\d+=' }).Count
        }
    }
    $scanProblems = @()
    if ($scan.timedOut) { $scanProblems += 'the headless scan did not finish and was killed' }
    elseif ($scan.exitCode -ne 0) { $scanProblems += "the headless scan exited with code $($scan.exitCode)" }
    if (-not $stored.Channels.Count) { $scanProblems += 'the scan found no channels' }
    elseif ($stored.IniStored -ne $stored.Channels.Count) { $scanProblems += "/dvbscansave stored $($stored.IniStored) channel(s) in the profile, the scan found $($stored.Channels.Count)" }
    Complete-Case 'scan-and-store' $scanProblems
    if ($scanProblems) { return (Finish) }     # nothing to tune from

    # --- 2. tune and listen -------------------------------------------------------
    #
    # First, second and last channel: two services of one multiplex, and a different multiplex.
    $picks = @($stored.Channels[0])
    if ($stored.Channels.Count -gt 1) { $picks += $stored.Channels[1] }
    if ($stored.Channels.Count -gt 2) { $picks += $stored.Channels[-1] }

    foreach ($ch in $picks) {
        $case = 'tune-' + (($ch.Name -replace '[^A-Za-z0-9]+', '-').Trim('-').ToLowerInvariant())

        Invoke-Command -Session $session -ArgumentList $ch.Index {
            param($index)
            $path = 'C:\mpc-test\player\mpc-hc64.ini'
            $lines = [System.Collections.Generic.List[string]](Get-Content $path)
            $lines = [System.Collections.Generic.List[string]]@($lines | Where-Object { $_ -notmatch '^LastChannel=' })
            $at = $lines.IndexOf('[DVBConfiguration2]')
            if ($at -lt 0) { throw 'the profile has no [DVBConfiguration2] section after the scan' }
            $lines.Insert($at + 1, "LastChannel=$index")
            [IO.File]::WriteAllLines($path, $lines, [Text.Encoding]::Unicode)
        }

        $run = Invoke-TunedRun -Tag "$case-$stamp" -ArgumentLine '/device' -Extra "-OnAirSec $OnAirSec"

        $problems = @()
        if (-not $run.aliveAtEnd) { $problems += "the player went away by itself while on air (exit code $($run.exitCode))" }
        elseif (-not $run.closedGracefully) { $problems += 'the player did not close when asked and had to be killed' }

        $from = [datetime]$run.started; $to = ([datetime]$run.finished).AddSeconds(1)
        $hit = Invoke-Command -Session $session -ArgumentList $from, $to {
            param($from, $to)
            Get-ChildItem 'C:\Windows\System32\drivers\DriverData\Audio_Samples\SimpleAudioSample' -Filter *.wav -ErrorAction SilentlyContinue |
                Where-Object { $_.Length -gt 1000 -and $_.LastWriteTime -ge $from -and $_.LastWriteTime -le $to } |
                Sort-Object Length | Select-Object -Last 1 -ExpandProperty FullName
        }
        if (-not $hit) {
            $problems += 'no audio reached the endpoint'
        } else {
            $wav = Join-Path $OutDir "$case.wav"
            Copy-Item -FromSession $session $hit $wav -Force
            # Tuning, locking and buffering take a few seconds of the time on air; what is asked is that a good part
            # of it is sound, and the right sound. The first seconds are not judged: the emulator streams its
            # previous frequency until the tune request lands, so a capture can open with a second of the channel
            # that was on before. The pitch tolerance is 2%: a live source is rendered against a clock the player
            # has to match, and the tone comes back a fraction of a percent off, varying run to run.
            $checkArgs = @($wav, '--min-seconds', [math]::Max(3, $OnAirSec - 9), '--skip-seconds', 3)
            $tone = $channelTones[$ch.Name]
            if ($tone) { $checkArgs += @('--expect', "$tone,$tone", '--tolerance', [math]::Max(5, [math]::Round($tone * 0.02))) }
            $output = & python (Join-Path $vaudio 'tests\wavcheck.py') @checkArgs 2>&1
            if ($LASTEXITCODE -ne 0) { $problems += (($output | Where-Object { "$_" -match '^FAIL|^silent' }) -join '; ') }
            elseif (-not $tone) { Note DarkGray "$($ch.Name): sound reached the endpoint; its pitch is not declared, so not asserted" }
        }
        Complete-Case "$case ($($ch.Name))" $problems
    }
}
finally {
    Remove-PSSession $session -ErrorAction SilentlyContinue
}

Finish
