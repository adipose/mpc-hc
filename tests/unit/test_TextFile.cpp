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
#include "SubtitleTestUtil.h"
#include "../../src/Subtitles/TextFile.h"

// Encoding detection and decoding, src/Subtitles/TextFile.cpp, and the
// code-page step CSimpleTextSubtitle applies to what CTextFile could not
// decode. Modelled on #1376 (UTF-8 beyond the BMP), #2299 and #2548 (files
// that are not UTF-8), #3413 (saved subtitles lost their diacritics).

using namespace testutil;

namespace
{
    std::vector<CStringW> ReadLines(CTextFile& f)
    {
        std::vector<CStringW> lines;
        CStringW line;
        while (f.ReadString(line)) {
            lines.push_back(line);
        }
        return lines;
    }

    std::string Utf16(const wchar_t* s, bool bigEndian)
    {
        std::string out = bigEndian ? "\xFE\xFF" : "\xFF\xFE";
        for (; *s; s++) {
            const char lo = (char)(*s & 0xff), hi = (char)(*s >> 8);
            out += bigEndian ? hi : lo;
            out += bigEndian ? lo : hi;
        }
        return out;
    }
}

TEST_CASE(TextFile_Utf8WithBom)
{
    CTextFile f(CTextFile::UTF8);
    REQUIRE(f.Open(WriteTemp(L"utf8bom.txt", std::string("\xEF\xBB\xBF" "h\xC3\xA9llo\r\nw\xC3\xB6rld\r\n"))));
    CHECK_EQ(f.GetEncoding(), CTextFile::UTF8);
    CHECK(f.IsUnicode());
    auto lines = ReadLines(f);
    REQUIRE_EQ(lines.size(), (size_t)2);
    CHECK_EQ(lines[0], L"h\x00e9llo");
    CHECK_EQ(lines[1], L"w\x00f6rld");
}

TEST_CASE(TextFile_Utf8WithoutBomWhenUtf8IsTheDefault)
{
    // How every subtitle file is opened: assume UTF-8 until proven otherwise.
    CTextFile f(CTextFile::UTF8);
    REQUIRE(f.Open(WriteTemp(L"utf8.txt", std::string("\xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E\n" "second\n"))));
    auto lines = ReadLines(f);
    REQUIRE_EQ(lines.size(), (size_t)2);
    CHECK_EQ(lines[0], L"\x65e5\x672c\x8a9e");
    CHECK_EQ(lines[1], L"second");
    CHECK_EQ(f.GetEncoding(), CTextFile::UTF8);
}

// #1376: four-byte sequences were truncated to one UTF-16 unit
TEST_CASE(TextFile_Utf8FourByteSequenceBecomesASurrogatePair)
{
    CTextFile f(CTextFile::UTF8);
    REQUIRE(f.Open(WriteTemp(L"utf8-emoji.txt", std::string("a\xF0\x9F\x98\x80" "b\n"))));
    auto lines = ReadLines(f);
    REQUIRE_EQ(lines.size(), (size_t)1);
    CHECK_EQ(lines[0], L"a\xd83d\xde00" L"b");
    CHECK_EQ(f.GetEncoding(), CTextFile::UTF8);
}

TEST_CASE(TextFile_Utf16LittleEndian)
{
    CTextFile f(CTextFile::UTF8);
    REQUIRE(f.Open(WriteTemp(L"utf16le.txt", Utf16(L"h\x00e9llo \x65e5\x672c\r\nline two\r\n", false))));
    CHECK_EQ(f.GetEncoding(), CTextFile::LE16);
    CHECK(f.IsUnicode());
    auto lines = ReadLines(f);
    REQUIRE_EQ(lines.size(), (size_t)2);
    CHECK_EQ(lines[0], L"h\x00e9llo \x65e5\x672c");
    CHECK_EQ(lines[1], L"line two");
}

TEST_CASE(TextFile_Utf16BigEndian)
{
    CTextFile f(CTextFile::UTF8);
    REQUIRE(f.Open(WriteTemp(L"utf16be.txt", Utf16(L"h\x00e9llo \x65e5\x672c\nline two\n", true))));
    CHECK_EQ(f.GetEncoding(), CTextFile::BE16);
    auto lines = ReadLines(f);
    REQUIRE_EQ(lines.size(), (size_t)2);
    CHECK_EQ(lines[0], L"h\x00e9llo \x65e5\x672c");
    CHECK_EQ(lines[1], L"line two");
}

// #2299, #2548: a file that is not valid UTF-8 must be noticed and re-read,
// from the start of the offending line, in the fallback encoding.
TEST_CASE(TextFile_InvalidUtf8FallsBackMidFile)
{
    CTextFile f(CTextFile::UTF8);
    REQUIRE(f.Open(WriteTemp(L"ansi.txt", std::string("plain ascii\r\ncaf\xE9 cr\xE8me\r\nthird\r\n"))));
    auto lines = ReadLines(f);
    REQUIRE_EQ(lines.size(), (size_t)3);
    CHECK_EQ(lines[0], L"plain ascii");
    // Undecoded: one UTF-16 unit per byte, for the caller to run through a code page.
    CHECK_EQ(lines[1], L"caf\x00e9 cr\x00e8me");
    CHECK_EQ(lines[2], L"third");
    CHECK_EQ(f.GetEncoding(), CTextFile::DEFAULT_ENCODING);
    CHECK_FALSE(f.IsUnicode());
}

TEST_CASE(TextFile_InvalidUtf8AfterABomIsReplacedNotReinterpreted)
{
    // A BOM is a promise; a bad byte after it is damage, not another encoding.
    CTextFile f(CTextFile::UTF8);
    REQUIRE(f.Open(WriteTemp(L"bom-bad.txt", std::string("\xEF\xBB\xBF" "ab\xFF" "cd\nsecond\n"))));
    auto lines = ReadLines(f);
    REQUIRE(lines.size() >= 2);
    CHECK_EQ(lines[0].Left(3), L"ab?");
    CHECK_EQ(lines.back(), L"second");
    CHECK_EQ(f.GetEncoding(), CTextFile::UTF8);
}

// #4217: in the UTF-8 branch of CTextFile::ReadString an invalid byte used to
// end the read loop, so in a file with a BOM the line came back in two pieces.
TEST_CASE(TextFile_InvalidUtf8AfterABomDoesNotSplitTheLine)
{
    CTextFile f(CTextFile::UTF8);
    REQUIRE(f.Open(WriteTemp(L"bom-bad2.txt", std::string("\xEF\xBB\xBF" "ab\xFF" "cd\n"))));
    auto lines = ReadLines(f);
    REQUIRE_EQ(lines.size(), (size_t)1);
    CHECK_EQ(lines[0], L"ab?cd");
}

TEST_CASE(TextFile_TruncatedUtf8SequenceAtEndOfFile)
{
    CTextFile f(CTextFile::UTF8);
    REQUIRE(f.Open(WriteTemp(L"truncated.txt", std::string("\xEF\xBB\xBF" "ok\nbad \xE6\x97"))));
    auto lines = ReadLines(f);
    REQUIRE(lines.size() >= 1);
    CHECK_EQ(lines[0], L"ok");
    CHECK_EQ(f.GetEncoding(), CTextFile::UTF8);
}

TEST_CASE(TextFile_MultiByteSequenceAcrossTheReadBuffer)
{
    // The read buffer is 64 KiB. Put a two-byte and a four-byte character
    // exactly across its edge, in one line longer than the buffer.
    const size_t bufferSize = 64 * 1024;
    for (int lead = 1; lead <= 3; lead++) {
        std::string doc(bufferSize - lead, 'a');
        doc += "\xF0\x9F\x98\x80";  // U+1F600: 'lead' bytes before the edge, the rest after
        doc += "\xC3\xA9 tail\nnext line\n";
        CStringW name;
        name.Format(L"edge-%d.txt", lead);

        CTextFile f(CTextFile::UTF8);
        REQUIRE(f.Open(WriteTemp(name, doc)));
        auto lines = ReadLines(f);
        REQUIRE_EQ(lines.size(), (size_t)2);
        CHECK_EQ((size_t)lines[0].GetLength(), bufferSize - lead + 2 + 1 + 5);
        CHECK_EQ(lines[0].Right(8), L"\xd83d\xde00\x00e9 tail");
        CHECK_EQ(lines[1], L"next line");
        CHECK_EQ(f.GetEncoding(), CTextFile::UTF8);
    }
}

TEST_CASE(TextFile_LineEndings)
{
    CTextFile f(CTextFile::UTF8);
    REQUIRE(f.Open(WriteTemp(L"eol.txt", std::string("unix\nwindows\r\n\nafter a blank line\r\nlast"))));
    auto lines = ReadLines(f);
    REQUIRE_EQ(lines.size(), (size_t)5);
    CHECK_EQ(lines[0], L"unix");
    CHECK_EQ(lines[1], L"windows");
    CHECK_EQ(lines[2], L"");
    CHECK_EQ(lines[3], L"after a blank line");
    CHECK_EQ(lines[4], L"last");
}

TEST_CASE(TextFile_LoneCarriageReturnEndsALine)
{
    // Classic Mac line endings. UTF-16 and ANSI input drop the CR.
    CTextFile f(CTextFile::UTF8);
    REQUIRE(f.Open(WriteTemp(L"cr-le16.txt", Utf16(L"old mac\rlast", false))));
    auto lines = ReadLines(f);
    REQUIRE_EQ(lines.size(), (size_t)2);
    CHECK_EQ(lines[0], L"old mac");
    CHECK_EQ(lines[1], L"last");
}

// #4217: the UTF-8 branch of CTextFile::ReadString ended the line at a lone CR
// but left the CR in the returned string, unlike the UTF-16 and ANSI branches.
TEST_CASE(TextFile_LoneCarriageReturnEndsALineUtf8)
{
    CTextFile f(CTextFile::UTF8);
    REQUIRE(f.Open(WriteTemp(L"cr-utf8.txt", std::string("old mac\rlast"))));
    auto lines = ReadLines(f);
    REQUIRE_EQ(lines.size(), (size_t)2);
    CHECK_EQ(lines[0], L"old mac");
    CHECK_EQ(lines[1], L"last");
}

TEST_CASE(TextFile_DuplicateUtf8Bom)
{
    // Files assembled by concatenation start with two BOMs.
    CTextFile f(CTextFile::UTF8);
    REQUIRE(f.Open(WriteTemp(L"bom2.txt", std::string("\xEF\xBB\xBF\xEF\xBB\xBF" "text\n"))));
    auto lines = ReadLines(f);
    REQUIRE_EQ(lines.size(), (size_t)1);
    CHECK_EQ(lines[0], L"text");
}

// #4217: the duplicate-BOM workaround in CTextFile::FillBuffer tested for
// FF EF where the UTF-16 LE BOM is FF FE, so a second LE BOM leaked into the
// first line as U+FEFF.
TEST_CASE(TextFile_DuplicateUtf16LeBom)
{
    CTextFile f(CTextFile::UTF8);
    REQUIRE(f.Open(WriteTemp(L"bom2-le.txt", "\xFF\xFE" + Utf16(L"text\n", false))));
    auto lines = ReadLines(f);
    REQUIRE_EQ(lines.size(), (size_t)1);
    CHECK_EQ(lines[0], L"text");
}

TEST_CASE(TextFile_MissingFile)
{
    CTextFile f(CTextFile::UTF8);
    CHECK_FALSE(f.Open(mpctest::TempDir() + L"does-not-exist.txt"));
}

TEST_CASE(TextFile_SaveWritesTheBomForTheEncoding)
{
    // WriteString() writes LF as CR LF in every encoding.
    struct { CTextFile::enc e; const wchar_t* name; std::string expected; } cases[] = {
        { CTextFile::UTF8, L"save-utf8.txt", std::string("\xEF\xBB\xBF" "\xC8\x99\xC8\x9B\r\n") },
        { CTextFile::LE16, L"save-le16.txt", std::string("\xFF\xFE\x19\x02\x1B\x02\r\x00\n\x00", 10) },
        { CTextFile::BE16, L"save-be16.txt", std::string("\xFE\xFF\x02\x19\x02\x1B\x00\r\x00\n", 10) },
    };
    for (const auto& c : cases) {
        const CStringW path = mpctest::TempDir() + c.name;
        {
            CTextFile f;
            REQUIRE(f.Save(path, c.e));
            f.WriteString(L"\x0219\x021b\n"); // Romanian s and t with comma below
            f.Close();
        }
        auto bytes = ReadAll(path);
        CHECK_EQ(std::string(bytes.begin(), bytes.end()), c.expected);
    }
}

// --- the code-page step, in CSimpleTextSubtitle ------------------------------

TEST_CASE(STS_AnsiFileIsDecodedWithTheRequestedCharset)
{
    // Windows-1250: "Zażółć" and Windows-1251: "Привет"
    CSimpleTextSubtitle polish;
    REQUIRE(OpenText(polish, L"cp1250.srt", "1\n00:00:01,000 --> 00:00:02,000\nZa\xBF\xF3\xB3\xE6\n", EASTEUROPE_CHARSET));
    REQUIRE_EQ(polish.GetCount(), (size_t)1);
    CHECK_FALSE(polish[0].fUnicode);
    CHECK_EQ(polish.GetStrW(0), L"Za\x017c\x00f3\x0142\x0107");

    CSimpleTextSubtitle russian;
    REQUIRE(OpenText(russian, L"cp1251.srt", "1\n00:00:01,000 --> 00:00:02,000\n\xCF\xF0\xE8\xE2\xE5\xF2\n", RUSSIAN_CHARSET));
    REQUIRE_EQ(russian.GetCount(), (size_t)1);
    CHECK_EQ(russian.GetStrW(0), L"\x041f\x0440\x0438\x0432\x0435\x0442");
}

TEST_CASE(STS_Utf8FileIgnoresTheRequestedCharset)
{
    CSimpleTextSubtitle sts;
    REQUIRE(OpenText(sts, L"utf8-charset.srt", "1\n00:00:01,000 --> 00:00:02,000\nZa\xC5\xBC\xC3\xB3\xC5\x82\xC4\x87\n", RUSSIAN_CHARSET));
    REQUIRE_EQ(sts.GetCount(), (size_t)1);
    CHECK(sts[0].fUnicode);
    CHECK_EQ(sts.GetStrW(0), L"Za\x017c\x00f3\x0142\x0107");
}

// #3413: a downloaded subtitle was written back in the ANSI code page
TEST_CASE(STS_SaveAsUtf8RoundTripsDiacritics)
{
    const std::string srt = "1\n00:00:01,000 --> 00:00:02,500\n\xC8\x98i \xC8\x9B\x61r\xC4\x83, \xE6\x97\xA5\xE6\x9C\xAC\n\n2\n00:00:03,000 --> 00:00:04,000\n<i>second</i>\n";
    CSimpleTextSubtitle sts;
    REQUIRE(OpenText(sts, L"roundtrip-in.srt", srt));
    REQUIRE_EQ(sts.GetCount(), (size_t)2);

    const CStringW out = mpctest::TempDir() + L"roundtrip-out";
    REQUIRE(sts.SaveAs(out, Subtitle::SRT, -1, 0, CTextFile::UTF8, false));

    CSimpleTextSubtitle again;
    REQUIRE(again.Open(out + L".srt", DEFAULT_CHARSET, L"test"));
    REQUIRE_EQ(again.GetCount(), (size_t)2);
    CHECK_EQ(again.m_encoding, CTextFile::UTF8);
    CHECK_EQ(again.GetStrW(0), L"\x0218i \x021b" L"ar\x0103, \x65e5\x672c");
    CHECK_EQ(StartMs(again, 0), 1000);
    CHECK_EQ(EndMs(again, 0), 2500);
    CHECK_EQ(again[1].str, sts[1].str);
}
