#!/bin/bash

git() { ~/scripts/git.sh "$@"; }

git fetch upstream
git fetch origin

# Reset transifex branch to upstream/develop without changing tracking
git checkout transifex 2>/dev/null || git checkout -b transifex origin/transifex
git reset --hard upstream/develop
git submodule update src/thirdparty/LAVFilters/src

# Restore only the PO directory from transifex
git checkout origin/transifex -- src/mpc-hc/mpcresources/PO

# Sync: merge new upstream strings into PO files
cd src/mpc-hc/mpcresources
/mnt/c/Windows/System32/cmd.exe /c sync.bat Silent
cd ../../..

git commit -am "Transifex translations"

echo "To push: git push -u origin transifex --force"
