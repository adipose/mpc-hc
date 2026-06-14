#!/bin/bash -v

git() { ~/scripts/git.sh "$@"; }

MAXBRANCH=`git branch | grep patch | sed -e 's/[patch\* ]//g' | sort -rn | head -1`

((MAXBRANCH=MAXBRANCH+1))

echo $MAXBRANCH

git fetch upstream
#git checkout develop
#git merge upstream/develop

git checkout -b patch$MAXBRANCH upstream/develop
#git merge upstream/develop --allow-unrelated-histories
#git submodule update --force 

if git submodule status --recursive | grep -q '^[^\ ]'; then
    git submodule foreach --recursive '
        if [ -n "$(git status --porcelain 2>/dev/null)" ]; then
            echo "Resetting dirty submodule: $name"
            git checkout --force HEAD
        fi
    '
    git submodule update --recursive
fi


