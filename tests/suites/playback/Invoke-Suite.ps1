<#
.SYNOPSIS
    The playback suite: plays generated clips in the player under test and
    asserts on what reached the (virtual) speakers and the (virtual) monitor.

.DESCRIPTION
    Every case starts the player from the command line with a fresh portable
    profile, lets it play a clip whose content is declared in clips.json, and
    checks evidence the player has no hand in producing:

      sound    the WAV the virtual audio endpoint wrote -- which tones, on
               which channels, for how long;
      picture  the frame the virtual monitor was sent -- which colour is in
               which corner of where the video should be;
      process  whether the player exited by itself, and with what code.

    It needs two virtual devices on the target, from sibling repositories of
    the tuner emulator, both submodules here: tests\vaudio (vaudio-endpoint,
    a sound card that records) and tests\vdisplay (idd-vdisplay, a monitor
    that can be plugged and read back). $env:MPC_TEST_VAUDIO and
    $env:MPC_TEST_VDISPLAY point the suite at other checkouts, for working on
    a driver and the suite together.

    Installing a driver changes what a guest is, so the suite does not do it
    unasked: if the devices are absent it reports not-ready-on-this-guest and
    skips. -InstallDrivers installs them first (use it on a guest you hold
    and will revert, or as part of provisioning the pool).

.PARAMETER InstallDrivers
    Install or update both virtual devices on the target before running.
#>
[CmdletBinding()]
param(
    [switch] $Probe,
    [string] $VMName = '',
    [string] $OutDir = (Join-Path $PSScriptRoot 'results'),
    [string] $PlayerBinary,
    [switch] $InstallDrivers
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$description = 'Plays generated clips; asserts on the audio and the frames that reached virtual output devices'
$testsRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$repoRoot = Split-Path $testsRoot -Parent
$vaudio = if ($env:MPC_TEST_VAUDIO) { $env:MPC_TEST_VAUDIO } else { Join-Path $testsRoot 'vaudio' }
$vdisplay = if ($env:MPC_TEST_VDISPLAY) { $env:MPC_TEST_VDISPLAY } else { Join-Path $testsRoot 'vdisplay' }
$transport = Join-Path $testsRoot 'emulator\tools\GuestTransport.ps1'

function Resolve-PlayerDir {
    param([string] $Binary)
    if ($Binary) { return (Split-Path (Resolve-Path $Binary).Path -Parent) }
    $own = Join-Path $repoRoot 'bin\mpc-hc_x64\mpc-hc64.exe'
    if (Test-Path $own) { return (Split-Path $own -Parent) }
    return $null
}

if ($Probe) {
    $ready = $true; $reason = ''
    if (-not (Test-Path $transport)) { $ready = $false; $reason = 'emulator submodule not initialised (git submodule update --init tests/emulator)' }
    elseif (-not (Test-Path (Join-Path $vaudio 'tests\wavcheck.py'))) { $ready = $false; $reason = 'vaudio submodule not initialised (git submodule update --init tests/vaudio)' }
    elseif (-not (Test-Path (Join-Path $vdisplay 'tools\Install-VDisplay.ps1'))) { $ready = $false; $reason = 'vdisplay submodule not initialised (git submodule update --init tests/vdisplay)' }
    elseif (-not (Get-Command ffmpeg -ErrorAction SilentlyContinue)) { $ready = $false; $reason = 'ffmpeg not on PATH' }
    elseif (-not (Get-Command python -ErrorAction SilentlyContinue)) { $ready = $false; $reason = 'python (with numpy) not on PATH' }
    return [pscustomobject]@{ Suite = 'playback'; Description = $description; Ready = $ready; Reason = $reason }
}

New-Item -ItemType Directory -Force $OutDir | Out-Null
# Absolute from here on: .NET file APIs resolve against the process directory, not PowerShell's location.
$OutDir = (Resolve-Path $OutDir).Path
$passed = 0; $failed = 0; $skipped = 0
$notes = [System.Collections.Generic.List[string]]::new()
function Note { param([string] $Colour, [string] $Text) $notes.Add($Text); Write-Host "  $Text" -ForegroundColor $Colour }
function Finish { [pscustomobject]@{ Suite = 'playback'; Passed = $passed; Failed = $failed; Skipped = $skipped; Notes = $notes } }

# --- media and player ---------------------------------------------------------

& (Join-Path $PSScriptRoot 'New-PlaybackClips.ps1') | Out-Null
$media = Join-Path $PSScriptRoot 'media'
$clips = (Get-Content (Join-Path $media 'clips.json') -Raw | ConvertFrom-Json)

$playerDir = Resolve-PlayerDir $PlayerBinary
if (-not $playerDir) { throw 'No player build: pass -PlayerBinary or build this repository (bin\mpc-hc_x64\mpc-hc64.exe).' }

# --- target -------------------------------------------------------------------

. $transport
$cfg = Get-TestBedConfig
$session = Connect-TestGuest -Guest $VMName
try {
    if ($InstallDrivers) {
        # Running against a provisioned guest needs nothing built here; installing does.
        foreach ($need in (Join-Path $vaudio 'build\out\x64\vaudio.sys'), (Join-Path $vdisplay 'build\out\x64\vdisplay.dll')) {
            if (-not (Test-Path $need)) { throw "-InstallDrivers needs the drivers built: $need is missing (tools\Build.ps1 in that submodule)." }
        }
        & (Join-Path $vaudio 'tools\Install-VAudio.ps1') -Session $session 6>&1 | Out-Null
        & (Join-Path $vdisplay 'tools\Install-VDisplay.ps1') -Session $session 6>&1 | Out-Null
    }

    $devices = Invoke-Command -Session $session {
        [pscustomobject]@{
            Audio   = [bool](Get-PnpDevice -PresentOnly -ErrorAction SilentlyContinue | Where-Object { $_.HardwareID -contains 'ROOT\VAudioEndpoint' -and $_.Status -eq 'OK' })
            Display = [bool](Get-PnpDevice -PresentOnly -ErrorAction SilentlyContinue | Where-Object { $_.HardwareID -contains 'Root\VDisplay' -and $_.Status -eq 'OK' })
            Console = (Get-CimInstance Win32_ComputerSystem).UserName
        }
    }
    if (-not $devices.Audio -or -not $devices.Display) {
        $skipped = 1
        Note Yellow "not run: this guest lacks the virtual audio endpoint and/or virtual display (audio=$($devices.Audio) display=$($devices.Display)); provision it, or run with -InstallDrivers on a guest you will revert"
        return (Finish)
    }
    $consoleUser = if ($cfg.GuestConsoleUser) { $cfg.GuestConsoleUser } else { ($devices.Console -split '\\')[-1] }
    if (-not $consoleUser) { throw 'Nobody is logged on at the guest console; the player needs a desktop.' }

    # Deploy the player as one archive: exe, icon library, LAV Filters. No symbols, no import libraries.
    $stage = Join-Path $OutDir 'player-stage'
    if (Test-Path $stage) { Get-ChildItem $stage -Recurse -File | ForEach-Object { [IO.File]::Delete($_.FullName) } }
    New-Item -ItemType Directory -Force $stage, (Join-Path $stage 'LAVFilters64') | Out-Null
    Copy-Item (Join-Path $playerDir 'mpc-hc64.exe') $stage
    foreach ($f in 'mpciconlib.dll') { if (Test-Path (Join-Path $playerDir $f)) { Copy-Item (Join-Path $playerDir $f) $stage } }
    Get-ChildItem (Join-Path $playerDir 'LAVFilters64') -File | Where-Object { $_.Extension -in '.ax', '.dll', '.manifest' } | Copy-Item -Destination (Join-Path $stage 'LAVFilters64')
    $zip = Join-Path $OutDir 'player.zip'
    if (Test-Path $zip) { [IO.File]::Delete($zip) }
    Compress-Archive -Path (Join-Path $stage '*') -DestinationPath $zip

    Invoke-Command -Session $session {
        foreach ($d in 'C:\mpc-test', 'C:\mpc-test\media', 'C:\mpc-test\out') { if (-not (Test-Path $d)) { New-Item -ItemType Directory $d | Out-Null } }
        Get-Process mpc-hc64 -ErrorAction SilentlyContinue | Stop-Process -Force
    }
    Copy-Item -ToSession $session $zip 'C:\mpc-test\player.zip' -Force
    Copy-Item -ToSession $session (Join-Path $PSScriptRoot 'Run-PlayerCase.guest.ps1') 'C:\mpc-test\' -Force
    Get-ChildItem $media -File | Where-Object { $_.Extension -in '.mkv', '.mp4' } | ForEach-Object { Copy-Item -ToSession $session $_.FullName 'C:\mpc-test\media\' -Force }
    Invoke-Command -Session $session { Expand-Archive 'C:\mpc-test\player.zip' 'C:\mpc-test\player' -Force }

    $version = Invoke-Command -Session $session { (Get-Item 'C:\mpc-test\player\mpc-hc64.exe').VersionInfo.ProductVersion }
    Note Gray "player under test: $version from $playerDir"

    # --- one case ---------------------------------------------------------------

    function Invoke-PlayerCase {
        param(
            [string] $Name,
            [string] $Clip,
            [string] $Switches = '/play /close',
            [hashtable] $Settings = @{},
            [string] $PlugModes = '',          # non-empty: plug the virtual monitor with these modes for the case
            [double] $CaptureAtSec = 0
        )
        $tag = '{0}-{1}' -f $Name, (Get-Date -Format 'HHmmss')
        $guestOut = "C:\mpc-test\out\$tag.json"
        $guestPng = "C:\mpc-test\out\$tag.png"

        # A fresh portable profile: an ini beside the exe puts the player in ini mode, and nothing of the last
        # case -- or of whoever used this guest before -- carries over. UpdaterAutoCheck must be present or
        # the first-run prompt blocks an unattended player.
        $ini = [ordered]@{ UpdaterAutoCheck = 0; KeepHistory = 0; RememberFilePos = 0; Loop = 0; AllowMultipleInstances = 0; LogoFile = ''; ShowOSD = 0 }
        foreach ($k in $Settings.Keys) { $ini[$k] = $Settings[$k] }
        $iniText = "[Settings]`r`n" + (($ini.GetEnumerator() | ForEach-Object { "$($_.Key)=$($_.Value)" }) -join "`r`n") + "`r`n"

        $argumentLine = ('"C:\mpc-test\media\{0}" {1}' -f $Clip, $Switches).Trim()
        $guest = Invoke-Command -Session $session -ArgumentList $iniText, $PlugModes, $argumentLine, $guestOut, $guestPng, $CaptureAtSec, $consoleUser {
            param($iniText, $plugModes, $argumentLine, $out, $png, $captureAt, $user)
            # The session is shared with the driver install scripts, which leave it on 'Stop'; a native tool
            # writing to stderr would then end the case instead of being a result.
            $ErrorActionPreference = 'Continue'
            Get-Process mpc-hc64 -ErrorAction SilentlyContinue | Stop-Process -Force
            Get-ChildItem 'C:\mpc-test\player' -Filter '*.ini' | ForEach-Object { [IO.File]::Delete($_.FullName) }
            [IO.File]::WriteAllText('C:\mpc-test\player\mpc-hc64.ini', $iniText, [Text.Encoding]::Unicode)

            $plug = $null
            if ($plugModes) {
                $status = & 'C:\vdisplay\vdisplayctl.exe' status | ConvertFrom-Json
                if ($status.connectors[0].plugged) { & 'C:\vdisplay\vdisplayctl.exe' unplug 0 | Out-Null; Start-Sleep -Seconds 2 }
                & 'C:\vdisplay\vdisplayctl.exe' plug 0 --modes $plugModes --name MPCTEST | Out-Null
                $plug = (& 'C:\vdisplay\vdisplayctl.exe' wait 0 --timeout 30000 | Out-String)
                Start-Sleep -Seconds 2      # let the shell settle on the new desktop before a window is placed on it
            }

            $taskArgs = "-NoProfile -ExecutionPolicy Bypass -File C:\mpc-test\Run-PlayerCase.guest.ps1 -Exe C:\mpc-test\player\mpc-hc64.exe -ArgumentLine `"$($argumentLine.Replace('"','\"'))`" -Out $out -CaptureAtSec $captureAt -CapturePath $png"
            $action = New-ScheduledTaskAction -Execute 'powershell.exe' -Argument $taskArgs
            $principal = New-ScheduledTaskPrincipal -UserId $user -LogonType Interactive
            Register-ScheduledTask -TaskName 'MpcPlaybackCase' -Action $action -Principal $principal -Force | Out-Null
            Start-ScheduledTask -TaskName 'MpcPlaybackCase'
            $deadline = (Get-Date).AddSeconds(90)
            while (-not (Test-Path $out) -and (Get-Date) -lt $deadline) { Start-Sleep -Seconds 2 }
            Unregister-ScheduledTask -TaskName 'MpcPlaybackCase' -Confirm:$false
            Get-Process mpc-hc64 -ErrorAction SilentlyContinue | Stop-Process -Force

            if ($plugModes) { & 'C:\vdisplay\vdisplayctl.exe' unplug 0 | Out-Null }

            [pscustomobject]@{
                Json = if (Test-Path $out) { Get-Content $out -Raw } else { $null }
                Plug = $plug
                HasPng = Test-Path $png
            }
        }
        if (-not $guest.Json) { throw "case $Name produced no result on the guest" }
        $run = $guest.Json | ConvertFrom-Json
        Set-Content (Join-Path $OutDir "$Name.guest.json") $guest.Json

        # The audio capture for this case: written by the driver between the case's start and finish.
        $from = [datetime]$run.started; $to = ([datetime]$run.finished).AddSeconds(1)
        $wav = $null
        $hit = Invoke-Command -Session $session -ArgumentList $from, $to {
            param($from, $to)
            Get-ChildItem 'C:\Windows\System32\drivers\DriverData\Audio_Samples\SimpleAudioSample' -Filter *.wav -ErrorAction SilentlyContinue |
                Where-Object { $_.Length -gt 1000 -and $_.LastWriteTime -ge $from -and $_.LastWriteTime -le $to } |
                Sort-Object Length | Select-Object -Last 1 -ExpandProperty FullName
        }
        if ($hit) { $wav = Join-Path $OutDir "$Name.wav"; Copy-Item -FromSession $session $hit $wav -Force }

        $png = $null
        if ($guest.HasPng) { $png = Join-Path $OutDir "$Name.png"; Copy-Item -FromSession $session $guestPng $png -Force }

        [pscustomobject]@{ Run = $run; Wav = $wav; Png = $png; Plug = $guest.Plug }
    }

    function Test-Audio {
        param([string] $Wav, [int[]] $Tones, [double] $Seconds)
        if (-not $Wav) { return 'no audio reached the endpoint' }
        # Shared mode: the engine resamples to the mix format, so rate and depth are the engine's, not the
        # clip's. The player starting and stopping the graph costs a little at each end, hence the tolerance.
        $output = & python (Join-Path $vaudio 'tests\wavcheck.py') $Wav --expect ($Tones -join ',') --seconds $Seconds --duration-tolerance 0.4 2>&1
        if ($LASTEXITCODE -eq 0) { return $null }
        return (($output | Where-Object { "$_" -match '^FAIL' }) -join '; ')
    }

    Add-Type -AssemblyName System.Drawing
    function Get-ColourName {
        param($c)
        if ($c.R -gt 180 -and $c.G -gt 180 -and $c.B -gt 180) { 'white' }
        elseif ($c.R -gt 180 -and $c.G -lt 90 -and $c.B -lt 90) { 'red' }
        elseif ($c.G -gt 180 -and $c.R -lt 90 -and $c.B -lt 90) { 'green' }
        elseif ($c.B -gt 180 -and $c.R -lt 90 -and $c.G -lt 90) { 'blue' }
        elseif ($c.R -lt 40 -and $c.G -lt 40 -and $c.B -lt 40) { 'black' }
        else { "($($c.R),$($c.G),$($c.B))" }
    }

    # Where a picture of the given shape lands when it is fitted, centred, into the screen; then the colour a
    # quarter of the way in from each of its corners.
    function Test-Picture {
        param([string] $Png, $Picture)
        if (-not $Png) { return 'no frame was captured' }
        $bmp = [System.Drawing.Bitmap]::FromFile($Png)
        try {
            $scale = [math]::Min($bmp.Width / $Picture.width, $bmp.Height / $Picture.height)
            $w = $Picture.width * $scale; $h = $Picture.height * $scale
            $x0 = ($bmp.Width - $w) / 2; $y0 = ($bmp.Height - $h) / 2
            $points = [ordered]@{
                topLeft = @(0.25, 0.25); topRight = @(0.75, 0.25); bottomLeft = @(0.25, 0.75); bottomRight = @(0.75, 0.75)
            }
            $wrong = foreach ($corner in $points.Keys) {
                $p = $points[$corner]
                $got = Get-ColourName $bmp.GetPixel([int]($x0 + $w * $p[0]), [int]($y0 + $h * $p[1]))
                $want = $Picture.corners.$corner
                if ($got -ne $want) { "$corner is $got, expected $want" }
            }
            if ($wrong) { return ($wrong -join '; ') }
            return $null
        } finally { $bmp.Dispose() }
    }

    function Complete-Case {
        param([string] $Name, [string[]] $Problems)
        $Problems = @($Problems | Where-Object { $_ })
        if ($Problems.Count -eq 0) { $script:passed++; Note Green "PASS $Name" }
        else { $script:failed++; Note Red "FAIL ${Name}: $($Problems -join ' | ')" }
    }

    function Get-ProcessProblem {
        param($Run)
        if ($Run.timedOut) { return 'the player did not exit by itself (/close) and had to be killed' }
        if ($Run.exitCode -ne 0) { return "the player exited with code $($Run.exitCode)" }
        return $null
    }

    $seconds = [double]$clips.seconds

    # --- cases ------------------------------------------------------------------

    # 1. The baseline every other case stands on: open, play to the end, close, exit 0; the whole clip's audio
    #    arrives, left on the left.
    $c = Invoke-PlayerCase -Name 'plays-to-end-and-exits' -Clip 'stereo.mkv'
    Complete-Case 'plays-to-end-and-exits' @(
        (Get-ProcessProblem $c.Run),
        (Test-Audio $c.Wav $clips.clips.'stereo.mkv'.audio[0].tones $seconds)
    )

    # 2. The container's default flag picks the audio track: track 2 here, not track 1.
    #    (#99, #1551, #2093, #2673, #3935 -- the most re-reported behaviour in the tracker after file position.)
    $c = Invoke-PlayerCase -Name 'default-audio-track' -Clip 'twotracks.mkv'
    $default = @($clips.clips.'twotracks.mkv'.audio | Where-Object default)[0]
    Complete-Case 'default-audio-track' @(
        (Get-ProcessProblem $c.Run),
        (Test-Audio $c.Wav $default.tones $seconds)
    )

    # 3. Fullscreen on a second monitor: the picture fills the monitor it was sent to, the right way up.
    $c = Invoke-PlayerCase -Name 'fullscreen-second-monitor' -Clip 'stereo.mkv' -Switches '/play /close /fullscreen /monitor 2' -PlugModes '1920x1080@60' -CaptureAtSec 2.5
    Complete-Case 'fullscreen-second-monitor' @(
        (Get-ProcessProblem $c.Run),
        (Test-Picture $c.Png $clips.clips.'stereo.mkv'.picture)
    )

    # 4. Display rotation metadata is honoured, in the direction ffmpeg's own autorotation takes as correct.
    #    (#375, #3832, then #3909 "[Regression bug 3832]".)
    $c = Invoke-PlayerCase -Name 'rotation-metadata' -Clip 'rotated90.mp4' -Switches '/play /close /fullscreen /monitor 2' -PlugModes '1920x1080@60' -CaptureAtSec 2.5
    Complete-Case 'rotation-metadata' @(
        (Get-ProcessProblem $c.Run),
        (Test-Picture $c.Png $clips.clips.'rotated90.mp4'.picture)
    )
}
finally {
    Remove-PSSession $session -ErrorAction SilentlyContinue
}

Finish
