#!/bin/bash

git() { ~/scripts/git.sh "$@"; }

EXECUTE=0
INCLUDE_NON_PATCH=0
PROTECTED="main master develop"

for arg in "$@"; do
    case "$arg" in
        --execute) EXECUTE=1 ;;
        --include-non-patch) INCLUDE_NON_PATCH=1 ;;
        --help|-h)
            echo "Usage: $0 [--execute] [--include-non-patch]"
            echo "  Without --execute, runs in dry-run mode (no changes made)."
            echo "  --include-non-patch: also auto-delete merged branches not starting with 'patch'."
            exit 0
            ;;
        *)
            echo "Unknown argument: $arg" >&2
            exit 1
            ;;
    esac
done

if [[ $EXECUTE -eq 0 ]]; then
    echo "[DRY RUN] Pass --execute to apply changes."
    echo ""
fi

parse_github_repo() {
    echo "$1" | sed -n 's|.*github\.com[:/]\(.*\)\.git$|\1|p; s|.*github\.com[:/]\(.*[^/]\)$|\1|p' | head -1
}

FORK_REPO=$(parse_github_repo "$(git remote get-url origin 2>/dev/null)")
if [[ -z "$FORK_REPO" ]]; then
    echo "Error: could not determine fork repo from origin remote." >&2
    exit 1
fi

UPSTREAM_REPO=$(parse_github_repo "$(git remote get-url upstream 2>/dev/null)")
if [[ -z "$UPSTREAM_REPO" ]]; then
    echo "Error: could not determine upstream repo from upstream remote." >&2
    exit 1
fi

is_protected() {
    local branch="$1"
    for p in $PROTECTED; do
        [[ "$branch" == "$p" ]] && return 0
    done
    return 1
}

# Build set of branches merged upstream
echo "==> Fetching merged PR branches from $UPSTREAM_REPO (author: @me)..."
MERGED_BRANCHES=$(gh pr list --repo "$UPSTREAM_REPO" --state merged --author @me --limit 1000 --json headRefName,number,mergedAt -q '.[] | "\(.headRefName)\t#\(.number)\t\(.mergedAt | split("T")[0])"' 2>/dev/null)

declare -A MERGED_SET
while IFS=$'\t' read -r branch pr date; do
    [[ -n "$branch" ]] && MERGED_SET["$branch"]="$pr  merged $date"
done <<< "$MERGED_BRANCHES"

if [[ ${#MERGED_SET[@]} -eq 0 ]]; then
    echo "    No merged PRs found (or gh API error)."
fi

# Step 1: for each local branch, delete from fork + locally if merged upstream
echo "==> Processing local branches..."
FORK_DELETED=0
LOCAL_DELETED=0
STALE_BRANCHES=()

while IFS= read -r line; do
    # Strip leading marker (* or +) and whitespace to get branch name
    branch=$(echo "$line" | sed 's/^[[:space:]*+]*//' | awk '{print $1}')
    [[ -z "$branch" ]] && continue
    is_protected "$branch" && continue

    if [[ -n "${MERGED_SET[$branch]}" ]]; then
        pr_info="${MERGED_SET[$branch]}"
        if [[ "$branch" != patch* && $INCLUDE_NON_PATCH -eq 0 ]]; then
            STALE_BRANCHES+=("$branch|($pr_info) [merged upstream — use --include-non-patch to delete]")
            continue
        fi
        # Merged upstream: delete from fork if it exists there, and delete locally
        has_origin=$(echo "$line" | grep -c '\[origin/')
        if [[ $has_origin -gt 0 ]]; then
            if [[ $EXECUTE -eq 1 ]]; then
                echo "    DELETE from fork: $branch  ($pr_info)"
                if gh api -X DELETE "repos/$FORK_REPO/git/refs/heads/$branch" </dev/null 2>/dev/null; then
                    ((++FORK_DELETED))
                else
                    echo "    WARN: failed to delete $branch from fork (may already be gone)"
                fi
            else
                echo "    [dry-run] would DELETE from fork: $branch  ($pr_info)"
                ((++FORK_DELETED))
            fi
        fi
        if [[ $EXECUTE -eq 1 ]]; then
            echo "    DELETE local: $branch  ($pr_info)"
            if git branch -D "$branch" </dev/null 2>&1; then
                ((++LOCAL_DELETED))
            else
                echo "    WARN: 'git branch -D $branch' failed; skipping"
            fi
        else
            echo "    [dry-run] would DELETE local: $branch  ($pr_info)"
            ((++LOCAL_DELETED))
        fi
    elif echo "$line" | grep -q '\[.*: gone\]'; then
        STALE_BRANCHES+=("$branch")
    fi
done < <(git branch -vv)

echo "    Fork branches deleted: $FORK_DELETED"
echo "    Local branches deleted: $LOCAL_DELETED"

# Step 2: prune stale remote-tracking refs
echo ""
echo "==> Pruning stale remote-tracking refs (git remote prune)..."
if [[ $EXECUTE -eq 1 ]]; then
    git remote prune origin
else
    echo "    [dry-run] would run: git remote prune origin"
fi

if [[ ${#STALE_BRANCHES[@]} -gt 0 ]]; then
    echo ""
    echo "==> Local branches with gone upstream but NOT merged upstream (manual review):"
    for entry in "${STALE_BRANCHES[@]}"; do
        branch="${entry%%|*}"
        note="${entry#*|}"
        if [[ "$note" == "$branch" ]]; then
            note=""
        fi
        last_date=$(git log -1 --format="%ci" "$branch" </dev/null 2>/dev/null | cut -d' ' -f1)
        if [[ -n "$note" ]]; then
            echo "    $branch  $note"
        else
            echo "    $branch  (last commit: $last_date)"
        fi
    done
fi

echo ""
echo "Done."
