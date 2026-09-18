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
#include "../../src/Subtitles/LibassContext.h"

// SubRip and SSA/ASS through CSimpleTextSubtitle::Open (src/Subtitles/STS.cpp),
// and the SRT-to-ASS tag converter libass rendering uses
// (src/Subtitles/LibassContext.cpp: ParseSrtLine, GetTag, ConsumeAttribute).
// Modelled on #1038 ('=' in ASS lines) and #4185 (long tags).

using namespace testutil;

namespace
{
    // fixtures/subrip_basic.srt, in file order
    enum { PLAIN, INLINE_TAGS, TWO_LINES, NO_MS, HOURS, EQUALS, SRT_COUNT };
}

TEST_CASE(SubRip_TimingsAndText)
{
    CSimpleTextSubtitle sts;
    REQUIRE(OpenFixture(sts, L"subrip_basic.srt"));
    CHECK_EQ(sts.m_subtitleType, Subtitle::SRT);
    REQUIRE_EQ(sts.GetCount(), (size_t)SRT_COUNT);

    CHECK_EQ(sts[PLAIN].str, L"Plain first cue");
    CHECK_EQ(StartMs(sts, PLAIN), 1000);
    CHECK_EQ(EndMs(sts, PLAIN), 2500);

    CHECK_EQ(sts[TWO_LINES].str, L"first line\\Nsecond line");

    CHECK_EQ(StartMs(sts, NO_MS), 7000);
    CHECK_EQ(EndMs(sts, NO_MS), 8000);

    CHECK_EQ(StartMs(sts, HOURS), ((1 * 60 + 2) * 60 + 3) * 1000 + 4);
    CHECK_EQ(EndMs(sts, HOURS), ((1 * 60 + 2) * 60 + 5) * 1000 + 6);

    CHECK_EQ(sts[EQUALS].str, L"2 + 2 = 4, and a = b");
    CHECK_EQ(sts[PLAIN].style, L"Default");
}

TEST_CASE(SubRip_HtmlTagsBecomeSSATags)
{
    CSimpleTextSubtitle sts;
    REQUIRE(OpenFixture(sts, L"subrip_basic.srt"));
    REQUIRE_EQ(sts.GetCount(), (size_t)SRT_COUNT);
    CHECK_EQ(sts[INLINE_TAGS].str, L"{\\i1}italic{\\i} {\\b1}bold{\\b} {\\u1}underline{\\u}");
}

TEST_CASE(SubRip_Utf8BomCrLfAndNoTrailingNewline)
{
    CSimpleTextSubtitle sts;
    REQUIRE(OpenText(sts, L"bom.srt", "\xEF\xBB\xBF" "1\r\n00:00:01,000 --> 00:00:02,000\r\nna\xC3\xAFve caf\xC3\xA9\r\n\r\n2\r\n00:00:03,000 --> 00:00:04,000\r\nlast"));
    REQUIRE_EQ(sts.GetCount(), (size_t)2);
    CHECK_EQ(sts[0].str, L"na\x00efve caf\x00e9");
    CHECK_EQ(sts[1].str, L"last");
    CHECK_EQ(sts.m_encoding, CTextFile::UTF8);
}

TEST_CASE(SubRip_BlankLineInsideACueDoesNotEndIt)
{
    // A cue ends at a blank line only when a cue number follows it.
    CSimpleTextSubtitle sts;
    REQUIRE(OpenText(sts, L"gap.srt", "1\n00:00:01,000 --> 00:00:02,000\nbefore the gap\n\nafter the gap\n\n2\n00:00:03,000 --> 00:00:04,000\nsecond\n"));
    REQUIRE_EQ(sts.GetCount(), (size_t)2);
    CHECK_EQ(sts[0].str, L"before the gap\\N\\Nafter the gap");
    CHECK_EQ(sts[1].str, L"second");
}

TEST_CASE(SubRip_TextStartingWithANumberIsNotACueNumber)
{
    CSimpleTextSubtitle sts;
    REQUIRE(OpenText(sts, L"number.srt", "1\n00:00:01,000 --> 00:00:02,000\n1984 was a year\n\n2\n00:00:03,000 --> 00:00:04,000\n42\n"));
    REQUIRE_EQ(sts.GetCount(), (size_t)2);
    CHECK_EQ(sts[0].str, L"1984 was a year");
    CHECK_EQ(sts[1].str, L"42");
}

TEST_CASE(SubRip_OutOfOrderCuesAreKept)
{
    CSimpleTextSubtitle sts;
    REQUIRE(OpenText(sts, L"order.srt", "1\n00:00:05,000 --> 00:00:06,000\nlater\n\n2\n00:00:01,000 --> 00:00:02,000\nearlier\n"));
    REQUIRE_EQ(sts.GetCount(), (size_t)2);
    CHECK_EQ(sts[0].str, L"later");
    CHECK_EQ(sts[1].str, L"earlier");
}

TEST_CASE(SubRip_GarbageIsRejectedWithoutADialog)
{
    CSimpleTextSubtitle sts;
    CHECK_FALSE(OpenText(sts, L"garbage.srt", "this is not a subtitle file\nat all\n"));
    CHECK_EQ(sts.GetCount(), (size_t)0);
    CHECK_EQ(mpctest::MessageBoxCount(), 0);
}

TEST_CASE(SubRip_EmptyFileIsRejected)
{
    CSimpleTextSubtitle sts;
    CHECK_FALSE(OpenText(sts, L"empty.srt", ""));
    CHECK_EQ(sts.GetCount(), (size_t)0);
}

// --- SSA / ASS --------------------------------------------------------------

// #1038: an '=' anywhere in an ASS dialogue line raised "Syntax error".
TEST_CASE(ASS_EqualsSignInDialogueText)
{
    CSimpleTextSubtitle sts;
    REQUIRE(OpenFixture(sts, L"ass_equals.ass"));
    CHECK_EQ(mpctest::MessageBoxCount(), 0);
    REQUIRE_EQ(sts.GetCount(), (size_t)4);

    CHECK_EQ(sts[0].str, L"No equals sign here");
    CHECK_EQ(sts[1].str, L"E = mc^2, obviously");
    CHECK_EQ(sts[2].str, L"=== SIGN ===");
    CHECK_EQ(sts[3].str, L"{\\i1}a=b{\\i0}, with commas, too");

    CHECK_EQ(StartMs(sts, 1), 3000);
    CHECK_EQ(EndMs(sts, 1), 4000);
    CHECK_EQ(sts[2].style, L"Sign");
    CHECK_EQ(sts[2].actor, L"Narrator");
    CHECK_EQ(sts[2].layer, 1);
}

TEST_CASE(ASS_ScriptInfoAndStyles)
{
    CSimpleTextSubtitle sts;
    REQUIRE(OpenFixture(sts, L"ass_equals.ass"));
    CHECK(sts.m_subtitleType == Subtitle::ASS || sts.m_subtitleType == Subtitle::SSA);
    CHECK_EQ(sts.m_playRes.cx, 1280);
    CHECK_EQ(sts.m_playRes.cy, 720);

    STSStyle* sign = nullptr;
    REQUIRE(sts.m_styles.Lookup(L"Sign", sign));
    REQUIRE(sign != nullptr);
    CHECK_EQ(sign->fontName, L"Arial");
    CHECK_EQ(sign->fontSize, 36.0);
    CHECK_EQ(sign->scrAlignment, 8);
    CHECK_EQ(sign->colors[0], (COLORREF)0x00FFFF); // &H0000FFFF: yellow, stored BGR
    CHECK(sign->fontWeight > FW_NORMAL);
}

TEST_CASE(SSA_MarkedFieldAndEqualsSignInText)
{
    // In SSA v4 the first event field is "Marked=0"; the parser splits on
    // that '=' and must not go looking for another one in the text.
    CSimpleTextSubtitle sts;
    REQUIRE(OpenFixture(sts, L"ssa_equals.ssa"));
    CHECK_EQ(mpctest::MessageBoxCount(), 0);
    REQUIRE_EQ(sts.GetCount(), (size_t)2);
    CHECK_EQ(sts[0].str, L"x = y");
    CHECK_EQ(sts[1].str, L"no sign");
    CHECK_EQ(StartMs(sts, 0), 1000);
}

TEST_CASE(ASS_BrokenDialogueLineReportsASyntaxError)
{
    // The one place a parser talks to the user: a line that starts like a
    // dialogue and is not one. The harness swallows the dialog.
    mpctest::ExpectMessageBox();
    CSimpleTextSubtitle sts;
    const std::string doc =
        "[Script Info]\nScriptType: v4.00+\n\n[Events]\n"
        "Format: Layer, Start, End, Style, Name, MarginL, MarginR, MarginV, Effect, Text\n"
        "Dialogue: 0,0:00:01.00,0:00:02.00,Default,,0,0,0,,fine\n"
        "Dialogue: 0,not a time,0:00:04.00,Default,,0,0,0,,broken\n";
    CHECK_FALSE(OpenText(sts, L"broken.ass", doc));
    CHECK_EQ(mpctest::MessageBoxCount(), 1);
    CHECK_EQ(sts.GetCount(), (size_t)0);
}

// --- the SRT tag converter used when SRT is rendered through libass ---------

namespace
{
    std::string SrtToAss(std::string line)
    {
        STSStyle style; // the defaults: what </font> restores
        ParseSrtLine(line, style);
        return line;
    }
}

TEST_CASE(LibassSrt_BasicTags)
{
    CHECK_EQ(SrtToAss("<i>italic</i> and <b>bold</b>"), "{\\i1}italic{\\i0} and {\\b1}bold{\\b0}");
    CHECK_EQ(SrtToAss("<u>u</u><s>s</s>"), "{\\u1}u{\\u0}{\\s1}s{\\s0}");
    CHECK_EQ(SrtToAss("one<br>two"), "one\\Ntwo");
    CHECK_EQ(SrtToAss("one\ntwo\r\nthree"), "one\\Ntwo\\Nthree");
    CHECK_EQ(SrtToAss("<I>upper case tag</I>"), "{\\i1}upper case tag{\\i0}");
}

TEST_CASE(LibassSrt_UnknownTagsAreHiddenAndLoneBracketsKept)
{
    CHECK_EQ(SrtToAss("<blink>text</blink>"), "text");
    CHECK_EQ(SrtToAss("1 < 2 and 3 > 2"), "1 < 2 and 3 > 2");
    CHECK_EQ(SrtToAss("a <- b"), "a <- b");
}

TEST_CASE(LibassSrt_FontTag)
{
    STSStyle style;
    const std::string restore = "{\\c}{\\fn" + std::string(CT2CA(style.fontName)) + "}{\\fs" + std::to_string((int)std::round(style.fontSize)) + "}";

    // HTML colours are RGB, ASS colours BGR
    CHECK_EQ(SrtToAss("<font color=\"#ff0000\">red</font>"), "{\\c&H0000ff&}red" + restore);
    CHECK_EQ(SrtToAss("<font color=\"red\">red</font>"), "{\\c&H0000FF&}red" + restore);
    CHECK_EQ(SrtToAss("<font face=\"Comic Sans MS\" size=\"24\">x</font>"), "{\\fnComic Sans MS}{\\fs24}x" + restore);
    CHECK_EQ(SrtToAss("<font color=nonsense>x</font>"), "{\\c&HFFFFFF&}x" + restore);
}

TEST_CASE(LibassSrt_MicroDvdExtensions)
{
    CHECK_EQ(SrtToAss("{y:i}italic"), "{\\i1}italic");
    CHECK_EQ(SrtToAss("{c:$0000FF}red"), "{\\c&H0000FF&}red");
    CHECK_EQ(SrtToAss("{s:30}big"), "{\\fs30}big");
}

TEST_CASE(LibassSrt_GetTagAndConsumeAttribute)
{
    const char* p = "<font color=\"#00ff00\" face='Arial'>x";
    CHECK_EQ(GetTag(&p, false), "font");
    std::string value;
    CHECK_EQ(ConsumeAttribute(&p, value), "color");
    CHECK_EQ(value, "00ff00"); // the '#' is dropped here, not by the caller
    CHECK_EQ(ConsumeAttribute(&p, value), "face");
    CHECK_EQ(value, "Arial");
    CHECK_EQ(ConsumeAttribute(&p, value), "");
    CHECK_EQ(std::string(p), ">x");

    const char* notATag = "<3 hearts";
    CHECK_EQ(GetTag(&notATag, false), "");
    CHECK_EQ(std::string(notATag), "<3 hearts");
}

// #4185. Tag and attribute names and values were copied into BUFSIZ (512)
// byte stack buffers with strncpy_s, which treats a too-small destination as
// a fatal error, and were then terminated at an index past the buffer. One
// long run of letters after '<' ended the player (#4185). Isolated because
// before the fix this did not fail, it died -- and would again.
TEST_CASE_ISOLATED(LibassSrt_LongTagName)
{
    const std::string name(3000, 'a');
    CHECK_EQ(SrtToAss("<" + name + ">text</" + name + ">"), "text");

    const char* p = nullptr;
    const std::string tag = "<" + name + ">";
    p = tag.c_str();
    CHECK_EQ(GetTag(&p, false).size(), name.size());
}

// #4185 again: an attribute name or value longer than BUFSIZ overran a
// stack buffer in ConsumeAttribute.
TEST_CASE_ISOLATED(LibassSrt_LongAttribute)
{
    const std::string longValue(3000, 'x');
    const std::string longName(3000, 'n');
    STSStyle style;

    const std::string withLongValue = "<font face=\"" + longValue + "\">text</font>";
    CHECK(SrtToAss(withLongValue).find("{\\fn" + longValue + "}text") == 0);

    std::string value;
    const std::string attr = " " + longName + "=\"v\">";
    const char* p = attr.c_str();
    CHECK_EQ(ConsumeAttribute(&p, value).size(), longName.size());
    CHECK_EQ(value, "v");
}
