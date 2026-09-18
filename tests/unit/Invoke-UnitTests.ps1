<#
.SYNOPSIS
    Build and run the native unit tests.

.DESCRIPTION
    Builds MpcUnitTests.vcxproj -- and, through its project references, the
    player's own static libraries (DSUtil, Subtitles, SubPic, BaseClasses and
    the libass chain) -- then runs the resulting console executable and writes
    its results as JSON. No player process, no test rig, no GPU.

    The project is deliberately not in mpc-hc.sln. It reuses src\platform.props
    and src\common.props, so toolset, MFC-static and runtime settings match
    the libraries, and it places them in bin\lib\<cfg>_x64 exactly where a
    solution build would: whichever build ran first, the other is incremental.

    A first build compiles the libass chain (freetype, harfbuzz, fribidi...)
    and takes minutes; after that only what changed is rebuilt.

    Needs eight of the repository's submodules, which -InitSubmodules fetches
    (the libass chain, plus tinyxml2 for DSUtil and stb for SubPic):
      src/thirdparty/libass/libass      src/thirdparty/freetype2/freetype2
      src/thirdparty/fribidi/fribidi    src/thirdparty/harfbuzz/harfbuzz
      src/thirdparty/libiconv/libiconv-for-Windows
      src/thirdparty/libunibreak/libunibreak
      src/thirdparty/tinyxml2/library   src/thirdparty/stb
    and nasm.exe on PATH (libass assembles its x86 kernels with it), as the
    player's own build does.

.PARAMETER Filter
    Run only the tests whose name contains one of these strings.

.PARAMETER NoBuild
    Run the executable that is already there.

.PARAMETER List
    List the test names instead of running them.

.OUTPUTS
    With -PassThru, the parsed results object. The exit code is the test
    executable's: 0 all passed, 1 test failures, 2 usage or infrastructure.

.EXAMPLE
    .\Invoke-UnitTests.ps1
    .\Invoke-UnitTests.ps1 -Filter WebVTT, TextFile
    .\Invoke-UnitTests.ps1 -NoBuild -List
#>
[CmdletBinding()]
param(
    [Parameter(Position = 0, ValueFromRemainingArguments)]
    [string[]] $Filter,
    [ValidateSet('Release', 'Debug')]
    [string]   $Configuration = 'Release',
    [string]   $OutDir,
    [switch]   $NoBuild,
    [switch]   $List,
    [switch]   $InitSubmodules,
    [switch]   $PassThru
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

# "a,b" arrives as one string when the script is started with powershell -File
$Filter = @($Filter | Where-Object { $_ } | ForEach-Object { $_ -split ',' } | ForEach-Object { $_.Trim() } | Where-Object { $_ })

$UnitRoot = $PSScriptRoot
$RepoRoot = (Resolve-Path (Join-Path $UnitRoot '..\..')).Path
$Project  = Join-Path $UnitRoot 'MpcUnitTests.vcxproj'
$exeDir   = if ($Configuration -eq 'Debug') { 'bin\tests_x64_Debug' } else { 'bin\tests_x64' }
$Exe      = Join-Path $RepoRoot "$exeDir\MpcUnitTests.exe"

$Submodules = @(
    'src/thirdparty/libass/libass'
    'src/thirdparty/freetype2/freetype2'
    'src/thirdparty/fribidi/fribidi'
    'src/thirdparty/harfbuzz/harfbuzz'
    'src/thirdparty/libiconv/libiconv-for-Windows'
    'src/thirdparty/libunibreak/libunibreak'
    'src/thirdparty/tinyxml2/library'
    'src/thirdparty/stb'
)

function Find-MSBuild {
    # vswhere ships with every installer since VS 2017. -prerelease so a
    # preview-channel VS counts; newest first, and the instance has to carry
    # the C++ toolset and MFC or the libraries cannot build.
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (-not (Test-Path $vswhere)) { throw "vswhere.exe not found at $vswhere -- is Visual Studio installed?" }
    $found = & $vswhere -latest -prerelease -products * `
                 -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 Microsoft.VisualStudio.Component.VC.ATLMFC `
                 -find 'MSBuild\**\Bin\amd64\MSBuild.exe' | Select-Object -First 1
    if (-not $found) { throw 'No Visual Studio instance with the C++ toolset and MFC (ATLMFC component) was found.' }
    $found
}

if (-not $NoBuild) {
    $missing = @($Submodules | Where-Object { -not (Get-ChildItem (Join-Path $RepoRoot $_) -Force -ErrorAction SilentlyContinue | Select-Object -First 1) })
    if ($missing -and $InitSubmodules) {
        Write-Host "Initialising submodules: $($missing -join ', ')" -ForegroundColor Cyan
        & git -C $RepoRoot submodule update --init --depth 1 @missing | Out-Host
        if ($LASTEXITCODE) { throw 'git submodule update failed.' }
    } elseif ($missing) {
        throw "Submodules not initialised: $($missing -join ', '). Re-run with -InitSubmodules, or: git submodule update --init $($missing -join ' ')"
    }
    if (-not (Get-Command nasm.exe -ErrorAction SilentlyContinue)) {
        throw 'nasm.exe is not on PATH; libass needs it (see docs\Compilation.md).'
    }

    $msbuild = Find-MSBuild
    Write-Host "msbuild: $msbuild" -ForegroundColor DarkGray
    $logDir = if ($OutDir) { $OutDir } else { Join-Path $RepoRoot $exeDir }
    New-Item -ItemType Directory -Force $logDir | Out-Null
    $log = Join-Path $logDir 'build.log'

    # MPCHC_WINSDK_VER is what platform.props reads; build.bat takes it from
    # build.user.bat, so do the same. Without either, platform.props falls
    # back to the 8.1 SDK: keep that when it is installed (same SDK as the
    # player build, so the shared libs are interchangeable), else use the
    # newest Windows 10/11 SDK rather than fail.
    $userBat = Join-Path $RepoRoot 'build.user.bat'
    if (-not $env:MPCHC_WINSDK_VER -and (Test-Path $userBat)) {
        $m = Select-String -Path $userBat -Pattern '^\s*SET\s+"?MPCHC_WINSDK_VER=([^"\s]+)' | Select-Object -First 1
        if ($m) { $env:MPCHC_WINSDK_VER = $m.Matches[0].Groups[1].Value }
    }
    $sdk81 = Join-Path ${env:ProgramFiles(x86)} 'Windows Kits\8.1\Include\um\Windows.h'
    if (-not $env:MPCHC_WINSDK_VER -and -not (Test-Path $sdk81)) {
        $sdkInc = Join-Path ${env:ProgramFiles(x86)} 'Windows Kits\10\Include'
        $sdk = Get-ChildItem $sdkInc -Directory -ErrorAction SilentlyContinue |
               Where-Object { Test-Path (Join-Path $_.FullName 'um\Windows.h') } |
               Sort-Object { [version]$_.Name } | Select-Object -Last 1
        if ($sdk) { $env:MPCHC_WINSDK_VER = $sdk.Name }
    }

    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    # Out-Host so msbuild's console output shows but does not join this
    # script's output objects (which -PassThru returns).
    & $msbuild $Project /nologo /m /v:minimal /t:Build `
        "/p:Configuration=$Configuration" /p:Platform=x64 "/p:SolutionDir=$RepoRoot\" `
        "/flp:LogFile=$log;Verbosity=normal" | Out-Host
    if ($LASTEXITCODE) { throw "Build failed (msbuild exit $LASTEXITCODE). Full log: $log" }
    Write-Host ("build: {0:n0} s" -f $sw.Elapsed.TotalSeconds) -ForegroundColor DarkGray
}

if (-not (Test-Path $Exe)) { throw "$Exe not found -- build first (omit -NoBuild)." }

if ($List) {
    & $Exe --list @Filter
    exit $LASTEXITCODE
}

if (-not $OutDir) { $OutDir = Join-Path $UnitRoot ("results\{0:yyyyMMdd-HHmmss}" -f (Get-Date)) }
New-Item -ItemType Directory -Force $OutDir | Out-Null
$json = Join-Path $OutDir 'unit-results.json'

$exeArgs = @('--json', $json, '--fixtures', (Join-Path $UnitRoot 'fixtures'))
if ($Filter) { $exeArgs += $Filter }
# Show the exe's output and save it, but keep it out of the pipeline so that
# -PassThru returns only the result object.
& $Exe @exeArgs | Tee-Object -FilePath (Join-Path $OutDir 'unit-output.txt') | Out-Host
$code = $LASTEXITCODE
Write-Host "Results in $json" -ForegroundColor DarkGray

if ($PassThru) {
    $parsed = if (Test-Path $json) { Get-Content $json -Raw | ConvertFrom-Json } else { $null }
    return [pscustomobject]@{ ExitCode = $code; Results = $parsed; Json = $json }
}
exit $code
