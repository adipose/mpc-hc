# Backfill translations that exist in the upstream PO snapshot but are empty in the
# post-sync (transifex-sourced) PO. This preserves translations that reached upstream
# OUTSIDE the transifex branch — e.g. via feature PRs that ship new dialog strings with
# their translations inline (PR #3923 added Seek Bar / Menu / Controls / ... translated).
#
# Rules:
#   * Transifex ALWAYS wins: only entries whose msgstr is empty are considered.
#   * Upstream only fills gaps, and only for the exact same (msgctxt, msgid) key.
#   * All PO parsing/writing is done by the repo's own TranslationDataRC — no hand-rolled
#     PO text editing, so escaping/encoding/keying match sync.py exactly.
#
# Run from the mpcresources dir (like sync.py). Expects the upstream PO snapshot in
# ./PO_upstream and the working PO in ./PO.

import os
import fnmatch
import traceback

from TranslationDataRC import *

UPSTREAM = 'PO_upstream'


def backfillLang(base):
    result = TranslationDataRC()
    result.loadFromPO('PO/' + base, 'po')

    upstream = TranslationDataRC()
    upstream.loadFromPO(UPSTREAM + '/' + base, 'po')

    filled = 0
    for dst, src in ((result.dialogs, upstream.dialogs),
                     (result.menus, upstream.menus),
                     (result.strings, upstream.strings)):
        for key in dst:
            if not dst[key] and src.get(key):
                dst[key] = src[key]
                filled += 1

    if filled:
        result.writePO('PO/' + base, 'po')
    return filled


if __name__ == '__main__':
    total = 0
    for f in sorted(os.listdir('PO')):
        if fnmatch.fnmatch(f, 'mpc-hc.*.menus.po'):
            base = f[:-len('.menus.po')]
            if not os.path.exists(os.path.join(UPSTREAM, base + '.dialogs.po')):
                continue
            try:
                n = backfillLang(base)
            except Exception:
                print('  %s: SKIPPED' % base)
                print(traceback.format_exc())
                continue
            if n:
                print('  %s: backfilled %d' % (base, n))
            total += n
    print('TOTAL backfilled: %d' % total)
