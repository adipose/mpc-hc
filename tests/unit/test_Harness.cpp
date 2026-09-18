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

#include "stdafx.h"

// The harness checking itself: the comparisons the other files lean on.

TEST_CASE(Harness_StringComparisonsCompareContent)
{
    CStringW w = L"abc";
    CStringA a = "abc";
    CHECK_EQ(w, L"abc");
    CHECK_EQ(a, "abc");
    CHECK_NE(w, L"abd");
    CHECK_EQ(std::string("x"), "x");
}

TEST_CASE(Harness_FixtureAndTempDirsExist)
{
    CHECK(GetFileAttributesW(mpctest::FixtureDir()) != INVALID_FILE_ATTRIBUTES);
    CStringW p = testutil::WriteTemp(L"harness.bin", std::string("\x01\x02\x03"));
    auto bytes = testutil::ReadAll(p);
    REQUIRE_EQ(bytes.size(), (size_t)3);
    CHECK_EQ(bytes[2], 3);
}
