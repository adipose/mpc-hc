<#
.SYNOPSIS
    Provisions one guest with the two virtual output devices the playback
    suite needs, proves they work there, and checkpoints the result.

.DESCRIPTION
    This changes what a guest IS. On a pooled rig, run it for every guest of
    the pool with the pool closed to claims (the broker's drain), one guest at
    a time -- the guests do not survive being started together.

    Per guest: start it, confirm it is the guest it claims to be
    (C:\vtuner\rig-id.txt), install vaudio-endpoint and idd-vdisplay, then
    verify rather than trust the installer:

      display  plug a monitor, wait for frames, capture one, unplug;
      audio    play a two-tone test through the endpoint in the console
               session and check the driver's capture for those tones.

    Only a guest that passes both is shut down and checkpointed.

.PARAMETER VMName
    The Hyper-V guest.

.PARAMETER CheckpointName
    Name for the checkpoint taken afterwards. Empty = take none.

.PARAMETER ConsoleUser
    The account logged on at the guest's console. Default: GuestConsoleUser
    from testbed.config.psd1, else whoever is logged on.

.PARAMETER KeepRunning
    Leave the guest running (and take no checkpoint).
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory)] [string] $VMName,
    [string] $CheckpointName = ('virtual-av-drivers-' + (Get-Date -Format 'yyyyMMdd')),
    [string] $ConsoleUser,
    [switch] $KeepRunning
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$testsRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$vaudio = if ($env:MPC_TEST_VAUDIO) { $env:MPC_TEST_VAUDIO } else { Join-Path $testsRoot 'vaudio' }
$vdisplay = if ($env:MPC_TEST_VDISPLAY) { $env:MPC_TEST_VDISPLAY } else { Join-Path $testsRoot 'vdisplay' }
foreach ($need in (Join-Path $vaudio 'build\out\x64\vaudio.sys'), (Join-Path $vaudio 'build\out\x64\wasapiprobe.exe'), (Join-Path $vdisplay 'build\out\x64\vdisplay.dll')) {
    if (-not (Test-Path $need)) { throw "Build the drivers first: $need is missing (tools\Install-Toolchain.ps1, then tools\Build.ps1, in that submodule)." }
}
. (Join-Path $testsRoot 'emulator\tools\GuestTransport.ps1')

function Wait-Heartbeat {
    $deadline = (Get-Date).AddMinutes(6)
    do {
        Start-Sleep -Seconds 5
        $hb = (Get-VMIntegrationService -VMName $VMName -Name Heartbeat).PrimaryStatusDescription
    } until ($hb -eq 'OK' -or (Get-Date) -gt $deadline)
    if ($hb -ne 'OK') { throw "$VMName never reported a heartbeat; judge a wedged guest by heartbeat, not by State." }
}

Write-Host "=== $VMName ===" -ForegroundColor Cyan
if ((Get-VM $VMName).State -ne 'Running') { Start-VM $VMName }
Wait-Heartbeat
Start-Sleep -Seconds 45     # auto-logon and the shell

$session = $null
for ($i = 0; $i -lt 12 -and -not $session; $i++) { try { $session = Connect-TestGuest -Guest $VMName } catch { Start-Sleep -Seconds 10 } }
if (-not $session) { throw "Could not open a session to $VMName." }

try {
    $rigId = Invoke-Command -Session $session { if (Test-Path 'C:\vtuner\rig-id.txt') { (Get-Content 'C:\vtuner\rig-id.txt' -Raw).Trim() } }
    if ($rigId -ne $VMName) { throw "rig-id.txt says '$rigId', expected '$VMName'. Stopping: I do not know which guest this is." }

    if (-not $ConsoleUser) { $ConsoleUser = (Get-TestBedConfig).GuestConsoleUser }
    if (-not $ConsoleUser) { $ConsoleUser = ((Invoke-Command -Session $session { (Get-CimInstance Win32_ComputerSystem).UserName }) -split '\\')[-1] }
    if (-not $ConsoleUser) { throw "Nobody is logged on at the console of $VMName; the audio check needs a desktop session." }

    & (Join-Path $vaudio 'tools\Install-VAudio.ps1') -Session $session 6>&1 | Where-Object { "$_" -match 'Drivers installed|failed' } | ForEach-Object { Write-Host "  audio:   $_" }
    & (Join-Path $vdisplay 'tools\Install-VDisplay.ps1') -Session $session 6>&1 | Where-Object { "$_" -match 'Drivers installed|failed' } | ForEach-Object { Write-Host "  display: $_" }

    # --- verify the display ---------------------------------------------------
    $display = Invoke-Command -Session $session {
        $ErrorActionPreference = 'Continue'
        Set-Location 'C:\vdisplay'
        & .\vdisplayctl.exe plug 0 --modes '1280x720@60' --name PROVISION | Out-Null
        $wait = & .\vdisplayctl.exe wait 0 --timeout 30000 | ConvertFrom-Json
        $waitExit = $LASTEXITCODE
        $capture = & .\vdisplayctl.exe capture 0 'C:\vdisplay\provision-check.png' | ConvertFrom-Json
        $captureExit = $LASTEXITCODE
        & .\vdisplayctl.exe unplug 0 | Out-Null
        [pscustomobject]@{ WaitExit = $waitExit; CaptureExit = $captureExit; Width = $capture.width; Height = $capture.height; Frames = $wait.connectors[0].framesAcquired; Fp16 = $wait.fp16Capable
                           PngBytes = (Get-Item 'C:\vdisplay\provision-check.png' -ErrorAction SilentlyContinue).Length }
    }
    if ($display.WaitExit -ne 0 -or $display.CaptureExit -ne 0 -or $display.Width -ne 1280 -or -not $display.PngBytes) {
        throw "display verification failed on ${VMName}: $($display | ConvertTo-Json -Compress)"
    }
    Write-Host "  display verified: $($display.Width)x$($display.Height) captured, $($display.Frames) frames, HDR-capable OS: $($display.Fp16)" -ForegroundColor Green

    # --- verify the audio -------------------------------------------------------
    Copy-Item -ToSession $session (Join-Path $vaudio 'build\out\x64\wasapiprobe.exe') 'C:\vaudio\' -Force
    $started = Invoke-Command -Session $session -ArgumentList $ConsoleUser {
        param($user)
        $started = Get-Date
        $action = New-ScheduledTaskAction -Execute 'C:\vaudio\wasapiprobe.exe' -Argument 'play --exclusive --rate 48000 --bits 16 --channels 2 --seconds 2'
        $principal = New-ScheduledTaskPrincipal -UserId $user -LogonType Interactive
        Register-ScheduledTask -TaskName 'VAudioProvisionCheck' -Action $action -Principal $principal -Force | Out-Null
        Start-ScheduledTask -TaskName 'VAudioProvisionCheck'
        Start-Sleep -Seconds 9
        Unregister-ScheduledTask -TaskName 'VAudioProvisionCheck' -Confirm:$false
        $started
    }
    $wavGuest = Invoke-Command -Session $session -ArgumentList $started {
        param($from)
        Get-ChildItem 'C:\Windows\System32\drivers\DriverData\Audio_Samples\SimpleAudioSample' -Filter *.wav -ErrorAction SilentlyContinue |
            Where-Object { $_.Length -gt 1000 -and $_.LastWriteTime -ge $from } | Sort-Object Length | Select-Object -Last 1 -ExpandProperty FullName
    }
    if (-not $wavGuest) { throw "audio verification failed on ${VMName}: nothing was captured. Is $ConsoleUser logged on at the console?" }
    $wavLocal = Join-Path ([IO.Path]::GetTempPath()) "provision-$VMName.wav"
    Copy-Item -FromSession $session $wavGuest $wavLocal -Force
    $check = & python (Join-Path $vaudio 'tests\wavcheck.py') $wavLocal --expect '400,700' --rate 48000 --bits 16 --seconds 2
    if ($LASTEXITCODE -ne 0) { throw "audio verification failed on ${VMName}: $($check -join ' | ')" }
    Write-Host "  audio verified: exclusive 48 kHz/16, 400 Hz left, 700 Hz right, 2.0 s" -ForegroundColor Green

    # Leave nothing of the verification behind in the state that gets checkpointed.
    Invoke-Command -Session $session {
        Get-ChildItem 'C:\Windows\System32\drivers\DriverData\Audio_Samples\SimpleAudioSample' -Filter *.wav -ErrorAction SilentlyContinue | ForEach-Object { [IO.File]::Delete($_.FullName) }
        if (Test-Path 'C:\vdisplay\provision-check.png') { [IO.File]::Delete('C:\vdisplay\provision-check.png') }
    }
}
finally {
    Remove-PSSession $session -ErrorAction SilentlyContinue
}

if ($KeepRunning) { Write-Host "  left running, no checkpoint"; return }

Stop-VM $VMName -Force
if ($CheckpointName) {
    Checkpoint-VM -Name $VMName -SnapshotName $CheckpointName
    Write-Host "  shut down; checkpoint '$CheckpointName' taken" -ForegroundColor Green
}
