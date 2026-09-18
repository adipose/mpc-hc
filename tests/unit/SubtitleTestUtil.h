/*
 * (C) 2026 see Authors.txt
 *
 * This file is part of MPC-HC.
 *
 * MPC-HC is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * MPC-HC is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#pragma once

#include "TestUtil.h"
#include "../../src/Subtitles/STS.h"

// Helpers for the text subtitle tests: load a fixture or an in-line document
// through the same entry point the player uses for a subtitle file.

namespace testutil
{
    // UTF-8 encode, for building in-line documents that contain non-ASCII text.
    inline std::string Utf8(const wchar_t* s)
    {
        return mpctest::ToUtf8(s);
    }

    inline bool OpenFixture(CSimpleTextSubtitle& sts, LPCWSTR name, int charSet = DEFAULT_CHARSET)
    {
        return sts.Open(Fixture(name), charSet, L"test");
    }

    // The file name decides which parser is tried first, as it does in the player.
    inline bool OpenText(CSimpleTextSubtitle& sts, LPCWSTR fileName, const std::string& bytes, int charSet = DEFAULT_CHARSET)
    {
        return sts.Open(WriteTemp(fileName, bytes), charSet, L"test");
    }

    inline LONGLONG StartMs(CSimpleTextSubtitle& sts, size_t i) { return sts[i].start / 10000; }
    inline LONGLONG EndMs(CSimpleTextSubtitle& sts, size_t i) { return sts[i].end / 10000; }
}
