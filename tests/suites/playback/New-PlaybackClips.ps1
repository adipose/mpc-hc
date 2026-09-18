<#
.SYNOPSIS
    Generates the clips the playback suite plays: a few seconds each, with
    content declared here so that a test can say exactly what should come out
    of the speakers and what should be on the screen.

.DESCRIPTION
    Every clip is synthesised by ffmpeg from test sources; nothing is
    downloaded and nothing is copyrighted. The declarations are written next
    to the clips as clips.json, which is what the suite asserts against -- the
    same arrangement as the emulator's encoding matrix.

    Picture: four flat quadrants, red / green over blue / white. Flat colours
    survive any scaler and any chroma subsampling, and which colour sits in
    which corner says everything about orientation.

    Sound: a different sine on every channel and every track, so a capture of
    what was rendered says which track played and whether channels swapped.

.PARAMETER OutDir
    Where to write. Default: media\ beside this script (gitignored).

.PARAMETER Force
    Regenerate clips that already exist.
#>
[CmdletBinding()]
param(
    [string] $OutDir = (Join-Path $PSScriptRoot 'media'),
    [switch] $Force
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$ffmpeg = (Get-Command ffmpeg -ErrorAction SilentlyContinue).Source
if (-not $ffmpeg) { throw 'ffmpeg not found on PATH. tests\emulator\tools\Install-TestBed.ps1 installs it.' }
New-Item -ItemType Directory -Force $OutDir | Out-Null

$seconds = 4
$quad = "color=c=red:s=640x360:r=30:d=$seconds[r];color=c=lime:s=640x360:r=30:d=$seconds[g];" +
        "color=c=blue:s=640x360:r=30:d=$seconds[b];color=c=white:s=640x360:r=30:d=$seconds[w];" +
        '[r][g]hstack[top];[b][w]hstack[bottom];[top][bottom]vstack[v]'

function Invoke-FFmpeg {
    param([string] $Target, [string[]] $Arguments)
    if ((Test-Path $Target) -and -not $Force) { return }
    & $ffmpeg -hide_banner -loglevel error -y @Arguments $Target
    if ($LASTEXITCODE -ne 0) { throw "ffmpeg failed producing $Target" }
}

function Tone { param([int] $Hz) "sine=frequency=${Hz}:sample_rate=48000:duration=$seconds" }

# --- stereo.mkv: the baseline. Left 440 Hz, right 880 Hz. -------------------
$stereo = Join-Path $OutDir 'stereo.mkv'
Invoke-FFmpeg $stereo @(
    '-f', 'lavfi', '-i', (Tone 440), '-f', 'lavfi', '-i', (Tone 880),
    '-filter_complex', "$quad;[0:a][1:a]join=inputs=2:channel_layout=stereo,volume=0.5[a]",
    '-map', '[v]', '-map', '[a]',
    '-c:v', 'libx264', '-pix_fmt', 'yuv420p', '-preset', 'veryfast', '-c:a', 'pcm_s16le'
)

# --- twotracks.mkv: two audio tracks; the SECOND carries the default flag. --
# Track 1: English, 600 Hz both channels. Track 2: German, 1200 Hz, default.
$twotracks = Join-Path $OutDir 'twotracks.mkv'
Invoke-FFmpeg $twotracks @(
    '-f', 'lavfi', '-i', (Tone 600), '-f', 'lavfi', '-i', (Tone 1200),
    '-filter_complex', "$quad;[0:a]aformat=channel_layouts=stereo,volume=0.5[a1];[1:a]aformat=channel_layouts=stereo,volume=0.5[a2]",
    '-map', '[v]', '-map', '[a1]', '-map', '[a2]',
    '-c:v', 'libx264', '-pix_fmt', 'yuv420p', '-preset', 'veryfast', '-c:a', 'pcm_s16le',
    '-metadata:s:a:0', 'language=eng', '-metadata:s:a:1', 'language=ger',
    '-disposition:a:0', '0', '-disposition:a:1', 'default'
)

# --- rotated90.mp4: the quadrant picture with 90 degrees of display rotation -
# ffmpeg's own autorotation is taken as the reference for what "rotated
# correctly" means; reference-rotated90.png is that rendering.
$plain = Join-Path $OutDir 'quad.mp4'
Invoke-FFmpeg $plain @(
    '-f', 'lavfi', '-i', (Tone 440),
    '-filter_complex', "$quad;[0:a]aformat=channel_layouts=stereo,volume=0.5[a]",
    '-map', '[v]', '-map', '[a]',
    '-c:v', 'libx264', '-pix_fmt', 'yuv420p', '-preset', 'veryfast', '-c:a', 'aac'
)
$rotated = Join-Path $OutDir 'rotated90.mp4'
Invoke-FFmpeg $rotated @('-display_rotation', '90', '-i', $plain, '-c', 'copy')
$reference = Join-Path $OutDir 'reference-rotated90.png'
Invoke-FFmpeg $reference @('-i', $rotated, '-frames:v', '1', '-update', '1')

# Read the reference back: which colour ended up in which corner, and the
# rotated picture's shape. A quarter of the way in from each corner is well
# inside a quadrant.
Add-Type -AssemblyName System.Drawing
$bmp = [System.Drawing.Bitmap]::FromFile($reference)
try {
    $name = {
        param($c)
        if ($c.R -gt 200 -and $c.G -gt 200 -and $c.B -gt 200) { 'white' }
        elseif ($c.R -gt 200 -and $c.G -lt 60 -and $c.B -lt 60) { 'red' }
        elseif ($c.G -gt 200 -and $c.R -lt 60 -and $c.B -lt 60) { 'green' }
        elseif ($c.B -gt 200 -and $c.R -lt 60 -and $c.G -lt 60) { 'blue' }
        else { "other($($c.R),$($c.G),$($c.B))" }
    }
    $w = $bmp.Width; $h = $bmp.Height
    $rotatedCorners = [ordered]@{
        topLeft     = & $name $bmp.GetPixel([int]($w / 4), [int]($h / 4))
        topRight    = & $name $bmp.GetPixel([int]($w * 3 / 4), [int]($h / 4))
        bottomLeft  = & $name $bmp.GetPixel([int]($w / 4), [int]($h * 3 / 4))
        bottomRight = & $name $bmp.GetPixel([int]($w * 3 / 4), [int]($h * 3 / 4))
    }
    $rotatedSize = @($w, $h)
} finally { $bmp.Dispose() }

$clips = [ordered]@{
    generated = (Get-Date).ToString('o')
    seconds   = $seconds
    clips     = [ordered]@{
        'stereo.mkv' = [ordered]@{
            picture = [ordered]@{ width = 1280; height = 720; corners = [ordered]@{ topLeft = 'red'; topRight = 'green'; bottomLeft = 'blue'; bottomRight = 'white' } }
            audio   = @([ordered]@{ track = 1; default = $true; tones = @(440, 880) })
        }
        'twotracks.mkv' = [ordered]@{
            picture = [ordered]@{ width = 1280; height = 720; corners = [ordered]@{ topLeft = 'red'; topRight = 'green'; bottomLeft = 'blue'; bottomRight = 'white' } }
            audio   = @(
                [ordered]@{ track = 1; language = 'eng'; default = $false; tones = @(600, 600) }
                [ordered]@{ track = 2; language = 'ger'; default = $true;  tones = @(1200, 1200) }
            )
        }
        'rotated90.mp4' = [ordered]@{
            picture = [ordered]@{ width = $rotatedSize[0]; height = $rotatedSize[1]; corners = $rotatedCorners; note = 'as rendered by ffmpeg with autorotation' }
            audio   = @([ordered]@{ track = 1; default = $true; tones = @(440, 440) })
        }
    }
}
$clips | ConvertTo-Json -Depth 8 | Set-Content (Join-Path $OutDir 'clips.json')
Write-Host "Clips in $OutDir"
$clips.clips.'rotated90.mp4'.picture | ConvertTo-Json -Compress | Write-Host
