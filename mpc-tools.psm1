# mpc-tools.psm1 — MPC-HC dev workflow commands, ported from ~/mpc-scripts/*.sh (WSL)
# to native PowerShell. Run these from within an MPC-HC checkout (e.g. C:\dev\mpc-hc-work).
# The old WSL `git() { ~/scripts/git.sh }` wrapper is gone: on Windows `git` is already
# Git-for-Windows, so it is called directly.
#
# Commands (same names as the old WSL aliases):
#   newbranch                 - fresh patchN branch off upstream/develop (+ submodule reset)
#   upt2                      - refresh the transifex branch and re-sync PO files
#   lavupdate                 - init/sync/update the LAVFilters submodule
#   cleanup-merged-branches   - delete local/fork branches whose PRs merged upstream
#   utf16util                 - run grep/sed/etc. against a UTF-16LE file (e.g. mpc-hc.rc)
#   mpcversion                - regenerate build/version_rev.h + the exe manifest

# ---------------------------------------------------------------------------
function newbranch {
    <#  Create a fresh patchN branch off upstream/develop, auto-incrementing N. #>
    $nums = git branch |
        Where-Object { $_ -match 'patch' } |
        ForEach-Object { $_ -replace '[patch* ]', '' } |
        Where-Object { $_ -match '^\d+$' } |
        ForEach-Object { [int]$_ }

    $max = ($nums | Measure-Object -Maximum).Maximum
    if (-not $max) { $max = 0 }
    $next = $max + 1
    Write-Host $next

    git fetch upstream
    git checkout -b "patch$next" upstream/develop

    # Reset any dirty submodules to their pinned commit, then update (mirrors the .sh)
    $status = git submodule status --recursive
    if ($status | Where-Object { $_ -notmatch '^\s' }) {
        foreach ($line in $status) {
            $path = ($line.Trim() -split '\s+')[1]
            if ($path -and (git -C $path status --porcelain)) {
                Write-Host "Resetting dirty submodule: $path"
                git -C $path checkout --force HEAD
            }
        }
        git submodule update --recursive
    }
}

# ---------------------------------------------------------------------------
# Run a .bat inside src/mpc-hc/mpcresources. Push-Location sets the child cmd's working
# directory (it follows $PWD). This environment sets NoDefaultCurrentDirectoryInExePath=1,
# which stops cmd from resolving bare batch names in the current dir (so both the .bat and its
# nested `CALL "common_python.bat"` fail); clear it for the child, then restore it.
function Invoke-MpcresBat([string]$bat) {
    Push-Location src/mpc-hc/mpcresources
    $saved = $env:NoDefaultCurrentDirectoryInExePath
    Remove-Item Env:NoDefaultCurrentDirectoryInExePath -ErrorAction SilentlyContinue
    try { & cmd.exe /c $bat Silent }
    finally {
        if ($null -ne $saved) { $env:NoDefaultCurrentDirectoryInExePath = $saved }
        Pop-Location
    }
}

# ---------------------------------------------------------------------------
function upt2 {
    <#  Rebuild the transifex branch on top of upstream/develop and re-sync PO files,
        then backfill any translations upstream has that the transifex branch lacks. #>
    git fetch upstream
    git fetch origin

    # Reset transifex branch to upstream/develop without changing tracking
    git checkout transifex 2>$null
    if ($LASTEXITCODE -ne 0) { git checkout -b transifex origin/transifex }
    git reset --hard upstream/develop
    git submodule update src/thirdparty/LAVFilters/src

    # Snapshot upstream's PO before we overwrite it. Upstream can hold translations that never
    # went through the transifex branch (e.g. feature PRs that ship new strings translated
    # inline), and the overwrite below would otherwise revert them.
    $poUp = 'src/mpc-hc/mpcresources/PO_upstream'
    Remove-Item -Recurse -Force $poUp -ErrorAction SilentlyContinue
    Copy-Item -Recurse 'src/mpc-hc/mpcresources/PO' $poUp

    # Restore only the PO directory from transifex (overwrites upstream's PO)
    git checkout origin/transifex -- src/mpc-hc/mpcresources/PO

    # Sync: regenerate the POT from source and msgmerge it into the (transifex) PO files.
    Invoke-MpcresBat 'sync.bat'

    # Backfill: fill any msgstr still empty from the upstream snapshot (transifex wins; upstream
    # only fills gaps), so translations that reached upstream outside transifex are preserved.
    # Uses the repo's own TranslationDataRC for PO parse/write — no hand-rolled PO editing.
    Copy-Item "$HOME/mpc-scripts/backfill_translations.py" 'src/mpc-hc/mpcresources/' -Force
    Copy-Item "$HOME/mpc-scripts/backfill.bat" 'src/mpc-hc/mpcresources/' -Force
    Invoke-MpcresBat 'backfill.bat'

    # Clean up temp artifacts (all untracked; not committed regardless)
    Remove-Item -Recurse -Force $poUp -ErrorAction SilentlyContinue
    Remove-Item -Force 'src/mpc-hc/mpcresources/backfill_translations.py', `
                       'src/mpc-hc/mpcresources/backfill.bat' -ErrorAction SilentlyContinue

    git commit -am "Transifex translations"
    Write-Host "To push: git push -u origin transifex --force"
}

# ---------------------------------------------------------------------------
function submit-transifex {
    <#  One-shot translation publish: refresh translations (upt2), force-push the transifex
        branch to your fork, and open the PR upstream. Prompts before the push/PR unless -Yes.
        Fork owner and upstream repo are read from the origin/upstream remotes. #>
    param(
        [string]$Title,
        [string]$Body = "Sync translations from Transifex / local translation system.",
        [switch]$SkipSync,   # skip upt2 (use if you already ran it and reviewed the result)
        [switch]$Web,        # open the PR compare page in a browser instead of creating it directly
        [switch]$Yes,        # skip the confirmation prompt
        [switch]$Force       # publish even if the regression guard trips
    )

    # 1. Refresh + commit translations on the transifex branch
    if (-not $SkipSync) { upt2 }

    $branch = git rev-parse --abbrev-ref HEAD
    if ($branch -ne 'transifex') {
        Write-Error "Expected to be on 'transifex' after sync but on '$branch'. Aborting."
        return
    }

    # Regression guard: never publish a commit with FEWER translated strings than upstream.
    # (This is what would have caught PR #3963 before it was pushed.)
    Write-Host "Checking translation counts vs upstream/develop..."
    $poList = git ls-tree -r --name-only HEAD -- src/mpc-hc/mpcresources/PO | Where-Object { $_ -match '\.po$' }
    $upN = 0; $ourN = 0
    foreach ($pf in $poList) {
        $upN  += (git show "upstream/develop:$pf" 2>$null | Where-Object { $_ -match '^msgstr' -and $_ -notmatch '^msgstr ""$' }).Count
        $ourN += (git show "HEAD:$pf"             2>$null | Where-Object { $_ -match '^msgstr' -and $_ -notmatch '^msgstr ""$' }).Count
    }
    Write-Host "  translated strings: upstream=$upN  this commit=$ourN  (delta $($ourN - $upN))"
    if ($ourN -lt $upN -and -not $Force) {
        Write-Error ("This commit has {0} FEWER translated strings than upstream/develop. Refusing to publish " -f ($upN - $ourN) +
                     "(the transifex branch is missing translations upstream has). Investigate, or pass -Force to override.")
        return
    }

    # Derive fork owner (origin) and upstream repo (upstream) so nothing is hard-coded
    $forkOwner = $null
    if ((git remote get-url origin 2>$null) -match 'github\.com[:/]([^/]+)/') { $forkOwner = $Matches[1] }
    $upstreamRepo = ((git remote get-url upstream 2>$null) -replace '\.git$', '')
    if ($upstreamRepo -match 'github\.com[:/](.+)$') { $upstreamRepo = $Matches[1] } else { $upstreamRepo = $null }
    if (-not $forkOwner)    { Write-Error "could not determine fork owner from origin remote."; return }
    if (-not $upstreamRepo) { Write-Error "could not determine upstream repo from upstream remote."; return }

    if (-not $Title) { $Title = "Transifex updates $(Get-Date -Format 'yyyy-MM-dd')" }

    # Show what will be published and confirm
    $statSummary = git show HEAD --stat --format='' | Where-Object { $_ -match 'file.* changed' } | Select-Object -Last 1
    Write-Host ""
    Write-Host "About to publish translations:" -ForegroundColor Cyan
    Write-Host "  commit : $(git log -1 --oneline)"
    if ($statSummary) { Write-Host "  changes:$statSummary" }
    Write-Host "  push   : $forkOwner/transifex --force"
    Write-Host "  PR     : $($forkOwner):transifex -> $($upstreamRepo):develop"
    Write-Host "  title  : $Title"
    if (-not $Yes) {
        $ans = Read-Host "Proceed with force-push and PR? (y/N)"
        if ($ans -notmatch '^(y|yes)$') { Write-Host "Aborted; nothing pushed."; return }
    }

    # 2. Force-push the fork branch
    git push -u origin transifex --force
    if ($LASTEXITCODE -ne 0) { Write-Error "push failed; not creating PR."; return }

    # 3. Open the PR upstream
    if ($Web) {
        gh pr create --repo $upstreamRepo --base develop --head "$($forkOwner):transifex" --title $Title --web
    }
    else {
        gh pr create --repo $upstreamRepo --base develop --head "$($forkOwner):transifex" --title $Title --body $Body
    }
}

# ---------------------------------------------------------------------------
function lavupdate {
    <#  Initialise, sync and update the LAVFilters submodule. #>
    git submodule init src/thirdparty/LAVFilters/src
    git submodule sync
    git submodule update
}

# ---------------------------------------------------------------------------
function cleanup-merged-branches {
    <#  Delete local + fork branches whose PRs have merged upstream. Dry-run by default. #>
    param([Parameter(ValueFromRemainingArguments = $true)] $Arguments)

    $execute = $false
    $includeNonPatch = $false
    foreach ($a in $Arguments) {
        switch -Regex ($a) {
            '^(--execute|-Execute)$'                       { $execute = $true }
            '^(--include-non-patch|-IncludeNonPatch)$'     { $includeNonPatch = $true }
            '^(--help|-h|-Help)$' {
                Write-Host "Usage: cleanup-merged-branches [--execute] [--include-non-patch]"
                Write-Host "  Without --execute, runs in dry-run mode (no changes made)."
                Write-Host "  --include-non-patch: also auto-delete merged branches not starting with 'patch'."
                return
            }
            default { Write-Error "Unknown argument: $a"; return }
        }
    }

    if (-not $execute) {
        Write-Host "[DRY RUN] Pass --execute to apply changes."
        Write-Host ""
    }

    function Get-GithubRepo([string]$url) {
        if (-not $url) { return $null }
        $url = $url -replace '\.git$', ''
        if ($url -match 'github\.com[:/](.+)$') { return $Matches[1] }
        return $null
    }

    $forkRepo = Get-GithubRepo (git remote get-url origin 2>$null)
    if (-not $forkRepo) { Write-Error "could not determine fork repo from origin remote."; return }
    $upstreamRepo = Get-GithubRepo (git remote get-url upstream 2>$null)
    if (-not $upstreamRepo) { Write-Error "could not determine upstream repo from upstream remote."; return }

    $protected = @('main', 'master', 'develop')

    Write-Host "==> Fetching merged PR branches from $upstreamRepo (author: @me)..."
    $mergedSet = @{}
    $prJson = gh pr list --repo $upstreamRepo --state merged --author '@me' --limit 1000 `
        --json headRefName, number, mergedAt 2>$null
    if ($prJson) {
        foreach ($pr in ($prJson | ConvertFrom-Json)) {
            $date = ($pr.mergedAt -split 'T')[0]
            $mergedSet[$pr.headRefName] = "#$($pr.number)  merged $date"
        }
    }
    if ($mergedSet.Count -eq 0) { Write-Host "    No merged PRs found (or gh API error)." }

    Write-Host "==> Processing local branches..."
    $forkDeleted = 0
    $localDeleted = 0
    $stale = @()   # array of [pscustomobject]@{ Branch; Note }

    foreach ($line in (git branch -vv)) {
        $branch = (($line -replace '^[\s*+]+', '').Trim() -split '\s+')[0]
        if (-not $branch) { continue }
        if ($protected -contains $branch) { continue }

        if ($mergedSet.ContainsKey($branch)) {
            $prInfo = $mergedSet[$branch]
            if ($branch -notlike 'patch*' -and -not $includeNonPatch) {
                $stale += [pscustomobject]@{ Branch = $branch; Note = "($prInfo) [merged upstream - use --include-non-patch to delete]" }
                continue
            }
            if ($line -match '\[origin/') {
                if ($execute) {
                    Write-Host "    DELETE from fork: $branch  ($prInfo)"
                    gh api -X DELETE "repos/$forkRepo/git/refs/heads/$branch" 2>$null | Out-Null
                    if ($LASTEXITCODE -eq 0) { $forkDeleted++ }
                    else { Write-Host "    WARN: failed to delete $branch from fork (may already be gone)" }
                }
                else { Write-Host "    [dry-run] would DELETE from fork: $branch  ($prInfo)"; $forkDeleted++ }
            }
            if ($execute) {
                Write-Host "    DELETE local: $branch  ($prInfo)"
                git branch -D $branch 2>&1 | Out-Null
                if ($LASTEXITCODE -eq 0) { $localDeleted++ }
                else { Write-Host "    WARN: 'git branch -D $branch' failed; skipping" }
            }
            else { Write-Host "    [dry-run] would DELETE local: $branch  ($prInfo)"; $localDeleted++ }
        }
        elseif ($line -match '\[.*: gone\]') {
            $stale += [pscustomobject]@{ Branch = $branch; Note = '' }
        }
    }

    Write-Host "    Fork branches deleted: $forkDeleted"
    Write-Host "    Local branches deleted: $localDeleted"

    Write-Host ""
    Write-Host "==> Pruning stale remote-tracking refs (git remote prune)..."
    if ($execute) { git remote prune origin }
    else { Write-Host "    [dry-run] would run: git remote prune origin" }

    if ($stale.Count -gt 0) {
        Write-Host ""
        Write-Host "==> Local branches with gone upstream but NOT merged upstream (manual review):"
        foreach ($entry in $stale) {
            if ($entry.Note) {
                Write-Host "    $($entry.Branch)  $($entry.Note)"
            }
            else {
                $lastDate = (git log -1 --format="%ci" $entry.Branch 2>$null) -split ' ' | Select-Object -First 1
                Write-Host "    $($entry.Branch)  (last commit: $lastDate)"
            }
        }
    }

    Write-Host ""
    Write-Host "Done."
}

# ---------------------------------------------------------------------------
function utf16util {
    <#  Run a text command (grep/sed/awk/perl/...) against a UTF-16LE file such as mpc-hc.rc.
        The file is converted to UTF-8 in a temp file, the command runs against it, and (for
        `sed/awk/perl -i`) the result is converted back to UTF-16LE. Uses the real grep/sed
        from Git-for-Windows so their semantics are exact; .NET replaces the old `iconv`.

        Args are read from $args (not a param block) so tool flags like -c / -i / -n pass
        through verbatim instead of being captured by PowerShell's parameter binding. #>
    $a = @($args)
    if ($a.Count -lt 2) {
        Write-Host "Usage: utf16util <utf16-file> <command> [args...]"
        Write-Host "  utf16util src/mpc-hc/mpc-hc.rc grep 'IDD_FAVORGANIZE'"
        Write-Host "  utf16util src/mpc-hc/mpc-hc.rc sed -i '100s/old/new/'"
        return
    }
    $File = $a[0]
    $Command = $a[1]
    $Rest = if ($a.Count -gt 2) { $a[2..($a.Count - 1)] } else { @() }

    if (-not (Test-Path -LiteralPath $File)) { Write-Error "File '$File' not found"; return }

    # Resolve the tool: PATH first, then Git's bundled usr\bin (grep/sed/awk/perl live there)
    $tool = $null
    $cmd = Get-Command $Command -ErrorAction SilentlyContinue
    if ($cmd) { $tool = $cmd.Source }
    if (-not $tool) {
        $git = Get-Command git -ErrorAction SilentlyContinue
        if ($git) {
            $usrbin = Join-Path (Split-Path (Split-Path $git.Source)) 'usr\bin'
            $p = Join-Path $usrbin "$Command.exe"
            if (Test-Path $p) { $tool = $p }
        }
    }
    if (-not $tool) { Write-Error "Command '$Command' not found (install Git for Windows or add its usr\bin to PATH)"; return }

    $full = (Resolve-Path -LiteralPath $File).Path
    $utf8 = New-Object System.Text.UTF8Encoding($false)

    # UTF-16LE -> UTF-8 temp. Strip the BOM before handing the text to the tool so
    # line-1 ^-anchored patterns work; remember it so the write-back can restore it.
    $text = [System.Text.Encoding]::Unicode.GetString([System.IO.File]::ReadAllBytes($full))
    $hadBom = $text.Length -gt 0 -and $text[0] -eq [char]0xFEFF
    if ($hadBom) { $text = $text.Substring(1) }
    $tmp = [System.IO.Path]::GetTempFileName()
    [System.IO.File]::WriteAllText($tmp, $text, $utf8)

    try {
        $writeBack = $false
        if ($Command -in 'sed', 'awk', 'perl') {
            foreach ($a in $Rest) { if ($a -eq '-i' -or $a -like '-i*') { $writeBack = $true; break } }
        }

        & $tool @Rest $tmp

        if ($writeBack) {
            # UTF-8 -> UTF-16LE back onto the original file. MSYS sed/awk/perl may strip
            # the BOM and emit bare-LF line endings (this silently corrupted mpc-hc.rc on
            # 2026-08-02: whole file rewritten without BOM, CRLF -> LF). Restore the BOM,
            # and normalize every line ending to CRLF as a rule — MPC-HC files are CRLF.
            $newText = [System.IO.File]::ReadAllText($tmp, $utf8)
            if ($newText.Length -gt 0 -and $newText[0] -eq [char]0xFEFF) { $newText = $newText.Substring(1) }
            $newText = $newText -replace '(?<!\r)\n', "`r`n"
            if ($hadBom) { $newText = [char]0xFEFF + $newText }
            [System.IO.File]::WriteAllBytes($full, [System.Text.Encoding]::Unicode.GetBytes($newText))
        }
    }
    finally { Remove-Item $tmp -ErrorAction SilentlyContinue }
}

# ---------------------------------------------------------------------------
function mpcversion {
    <#  Regenerate build/version_rev.h and the exe manifest from git state. Port of version.sh. #>
    param([switch]$Quiet)
    if ($args -contains '--quiet') { $Quiet = $true }

    $versionfileFixed = './include/version.h'
    $versionfile = './build/version_rev.h'
    $manifestfile = './src/mpc-hc/res/mpc-hc.exe.manifest'

    $verMajor = $verMinor = $verPatch = $null
    foreach ($line in (Get-Content $versionfileFixed)) {
        if     ($line -match 'MPC_VERSION_MAJOR\s+(\S+)') { $verMajor = $Matches[1].Trim() }
        elseif ($line -match 'MPC_VERSION_MINOR\s+(\S+)') { $verMinor = $Matches[1].Trim() }
        elseif ($line -match 'MPC_VERSION_PATCH\s+(\S+)') { $verPatch = $Matches[1].Trim() }
    }
    $verFixed = "$verMajor.$verMinor.$verPatch"
    if (-not $Quiet) { Write-Host "Version:   $verFixed" }

    $versionInfo = ''
    git rev-parse --git-dir *> $null
    if ($LASTEXITCODE -ne 0) {
        $hash = '0000000'; $ver = 0; $verAdditional = ''
        Write-Host "Warning: Git not available or not a git repo. Using dummy values for hash and version number."
    }
    else {
        $describe = git describe --long
        if (-not $Quiet) { Write-Host "Describe:  $describe" }
        $hash = $describe -replace '.*-g', ''          # ${describe##*-g}
        $ver = $describe -replace '^[^-]*-', ''        # ${describe#*-}
        $ver = $ver -replace '-g.*', ''                # ${ver%-g*}
        if (-not $hash) { $hash = '0000000' }
        if (-not $ver) { $ver = 0 }
        $verAdditional = " ($hash)"

        $branch = git symbolic-ref -q HEAD
        if ($LASTEXITCODE -eq 0 -and $branch) { $branch = $branch -replace '^refs/heads/', '' } else { $branch = 'no branch' }
        if (-not $Quiet) {
            Write-Host "On branch: $branch"
            Write-Host "Hash:      $hash"
            Write-Host "Revision:  $ver"
        }

        if ($branch -ne 'develop' -and $branch -ne 'master') {
            $versionInfo = '#define MPCHC_BRANCH _T("' + $branch + '")' + "`n"
            $verAdditional += " ($branch)"
            git show-ref --verify --quiet refs/heads/develop
            if ($LASTEXITCODE -eq 0) {
                $base = (git merge-base develop HEAD).Substring(0, 7)
                $verAdditional += " (develop@$base)"
                if (-not $Quiet) { Write-Host "Mergebase: develop@$base" }
            }
        }
    }

    # Optional MinGW GCC version strings (empty on an MSVC-only box)
    $gccVersions = @()
    foreach ($gcc in @('gcc', 'x86_64-w64-mingw32-gcc')) {
        if (Get-Command $gcc -ErrorAction SilentlyContinue) {
            $machine = & $gcc -dumpmachine 2>$null
            if ($machine) {
                if     ($machine -match 'w64-mingw32$') { $name = 'MinGW-w64' }
                elseif ($machine -match '-mingw32$')    { $name = 'MinGW' }
                else { $name = '' }
                $gccVersions += "$name GCC $(& $gcc -dumpversion 2>$null)"
            }
        }
    }
    $gcc32 = if ($gccVersions.Count -ge 1) { $gccVersions[0] } else { '' }
    $gcc64 = if ($gccVersions.Count -ge 2) { $gccVersions[1] } else { '' }

    $versionInfo += '#define MPCHC_HASH _T("' + $hash + '")' + "`n"
    $versionInfo += "#define MPC_VERSION_REV $ver" + "`n"
    $versionInfo += '#define MPC_VERSION_ADDITIONAL _T("' + $verAdditional + '")' + "`n"
    $versionInfo += '#define GCC32_VERSION _T("' + $gcc32 + '")' + "`n"
    $versionInfo += '#define GCC64_VERSION _T("' + $gcc64 + '")'

    # Write version_rev.h only if changed
    $existing = if (Test-Path $versionfile) { (Get-Content -Raw $versionfile) -replace '(\r?\n)+$', '' } else { $null }
    if ($existing -ne $versionInfo) {
        $out = Join-Path (Get-Location) ($versionfile -replace '^\./', '')
        [System.IO.File]::WriteAllText($out, $versionInfo + "`n")
    }

    # Update manifest only if changed
    $manifestConf = "$manifestfile.conf"
    if (Test-Path $manifestConf) {
        $newManifest = (Get-Content -Raw $manifestConf) -replace '\$VERSION\$', "$verFixed.$ver"
        $newManifest = $newManifest -replace '(\r?\n)+$', ''
        $existingManifest = if (Test-Path $manifestfile) { (Get-Content -Raw $manifestfile) -replace '(\r?\n)+$', '' } else { $null }
        if ($existingManifest -ne $newManifest) {
            $outM = Join-Path (Get-Location) ($manifestfile -replace '^\./', '')
            [System.IO.File]::WriteAllText($outM, $newManifest + "`n")
        }
    }
}

Export-ModuleMember -Function newbranch, upt2, submit-transifex, lavupdate, cleanup-merged-branches, utf16util, mpcversion
