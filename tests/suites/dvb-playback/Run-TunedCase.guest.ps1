# Runs on the target, in the console session. Live television never ends, so unlike a file case the player is
# given a fixed time on air and then closed. Optionally captures the virtual monitor while it is tuned.
# Writes one JSON object to -Out; the host does the asserting.
param(
    [Parameter(Mandatory)] [string] $Exe,
    [Parameter(Mandatory)] [string] $ArgumentLine,
    [Parameter(Mandatory)] [string] $Out,
    [int] $OnAirSec = 12,
    [int] $ExitTimeoutSec = 600,        # for runs that end by themselves (a headless scan)
    [switch] $WaitForExit,
    [double] $CaptureAtSec = 0,
    [int] $CaptureConnector = 0,
    [string] $CapturePath = ''
)

$ErrorActionPreference = 'Continue'
$result = [ordered]@{ started = (Get-Date).ToString('o'); args = $ArgumentLine }

$p = Start-Process -FilePath $Exe -ArgumentList $ArgumentLine -PassThru
$result.pid = $p.Id

if ($WaitForExit) {
    $result.timedOut = -not $p.WaitForExit($ExitTimeoutSec * 1000)
    if ($result.timedOut) { Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue } else { $result.exitCode = $p.ExitCode }
}
else {
    if ($CaptureAtSec -gt 0 -and $CaptureAtSec -lt $OnAirSec) {
        Start-Sleep -Milliseconds ([int]($CaptureAtSec * 1000))
        $result.aliveAtCapture = -not $p.HasExited
        $result.capture = (& 'C:\vdisplay\vdisplayctl.exe' capture $CaptureConnector $CapturePath | Out-String).Trim()
        Start-Sleep -Milliseconds ([int](($OnAirSec - $CaptureAtSec) * 1000))
    } else {
        Start-Sleep -Seconds $OnAirSec
    }
    # Still running is the healthy outcome here; having gone away by itself is a crash or a refusal to tune.
    $result.aliveAtEnd = -not $p.HasExited
    if ($p.HasExited) { $result.exitCode = $p.ExitCode }
    else {
        # Ask politely first, so the player shuts its graph down and the audio device sees a clean stop.
        [void]$p.CloseMainWindow()
        $result.closedGracefully = $p.WaitForExit(15000)
        if (-not $result.closedGracefully) { Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue }
        else { $result.exitCode = $p.ExitCode }
    }
}

Start-Sleep -Seconds 3      # the audio driver writes its capture from a work item after the stream closes
$result.finished = (Get-Date).ToString('o')
$result | ConvertTo-Json -Depth 4 | Set-Content $Out
