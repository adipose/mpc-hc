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

// WebVTT, src/Subtitles/STS.cpp: OpenVTT, WebVTT2SSA, WebVTTCueStrip and the
// VTT branch of CSimpleTextSubtitle::Add. Modelled on #668 (unsupported tags),
// #673 (escapes), #677 (regex_replace broke Unicode), #930/#992/#1023/#1054/
// #1806 (colour classes and STYLE blocks) and #4197 (duplicate-cue check).

using namespace testutil;

namespace
{
    // fixtures/webvtt_tags.vtt, in file order
    enum { PLAIN, INLINE_TAGS, NO_HOURS, VOICE, CLASS_AND_LANG, KARAOKE, ESCAPES, TWO_LINES, ALIGN_START, ALIGN_CENTER, ALIGN_END, TAGS_COUNT };
}

TEST_CASE(WebVTT_HeaderNotesAndCueIdentifiersAreNotCues)
{
    CSimpleTextSubtitle sts;
    REQUIRE(OpenFixture(sts, L"webvtt_tags.vtt"));
    CHECK_EQ(sts.m_subtitleType, Subtitle::VTT);
    REQUIRE_EQ(sts.GetCount(), (size_t)TAGS_COUNT);
    CHECK_EQ(sts[PLAIN].str, L"Plain first cue");
    CHECK_EQ(StartMs(sts, PLAIN), 1000);
    CHECK_EQ(EndMs(sts, PLAIN), 2500);
}

TEST_CASE(WebVTT_TimestampsWithoutHours)
{
    CSimpleTextSubtitle sts;
    REQUIRE(OpenFixture(sts, L"webvtt_tags.vtt"));
    REQUIRE_EQ(sts.GetCount(), (size_t)TAGS_COUNT);
    CHECK_EQ(StartMs(sts, NO_HOURS), 5000);
    CHECK_EQ(EndMs(sts, NO_HOURS), 6250);
}

TEST_CASE(WebVTT_ItalicBoldUnderlineBecomeSSATags)
{
    CSimpleTextSubtitle sts;
    REQUIRE(OpenFixture(sts, L"webvtt_tags.vtt"));
    REQUIRE_EQ(sts.GetCount(), (size_t)TAGS_COUNT);
    const CStringW& raw = sts[INLINE_TAGS].str;
    CHECK(raw.Find(L"{\\i1}italic{\\i}") >= 0);
    CHECK(raw.Find(L"{\\b1}bold{\\b}") >= 0);
    CHECK(raw.Find(L"{\\u1}underline{\\u}") >= 0);
    CHECK_EQ(raw.Find(L'<'), -1);
    // GetStrW() without SSA tags is the plain-text view; it keeps italics as <i>
    CHECK_EQ(sts.GetStrW(INLINE_TAGS), L"<i>italic</i> bold underline");
}

// #668: tags the renderer has no use for are removed, their content kept.
TEST_CASE(WebVTT_UnsupportedTagsAreRemovedAndTheirTextKept)
{
    CSimpleTextSubtitle sts;
    REQUIRE(OpenFixture(sts, L"webvtt_tags.vtt"));
    REQUIRE_EQ(sts.GetCount(), (size_t)TAGS_COUNT);
    CHECK_EQ(sts.GetStrW(VOICE), L"Voice span and another");
    CHECK_EQ(sts.GetStrW(CLASS_AND_LANG), L"class-less unknown class language");
    CHECK_EQ(sts.GetStrW(KARAOKE), L"Karaoke timestamps vanish");
    for (int i : { VOICE, CLASS_AND_LANG, KARAOKE }) {
        CHECK_EQ(sts[i].str.Find(L'<'), -1);
        CHECK_EQ(sts[i].str.Find(L'>'), -1);
    }
}

// #673
TEST_CASE(WebVTT_EscapeSequences)
{
    CSimpleTextSubtitle sts;
    REQUIRE(OpenFixture(sts, L"webvtt_tags.vtt"));
    REQUIRE_EQ(sts.GetCount(), (size_t)TAGS_COUNT);
    CHECK_EQ(sts[ESCAPES].str, L"Tom & Jerry: 1 < 2 > 0, non\\hbreaking, marks gone");
}

TEST_CASE(WebVTT_EscapedAmpersandIsNotDecodedTwice)
{
    CSimpleTextSubtitle sts;
    REQUIRE(OpenText(sts, L"amp.vtt", "WEBVTT\n\n00:01.000 --> 00:02.000\n&amp;lt; is how you write &lt;\n"));
    REQUIRE_EQ(sts.GetCount(), (size_t)1);
    CHECK_EQ(sts[0].str, L"&lt; is how you write <");
}

TEST_CASE(WebVTT_MultiLineCue)
{
    CSimpleTextSubtitle sts;
    REQUIRE(OpenFixture(sts, L"webvtt_tags.vtt"));
    REQUIRE_EQ(sts.GetCount(), (size_t)TAGS_COUNT);
    CHECK_EQ(sts[TWO_LINES].str, L"first line\\Nsecond line");
}

TEST_CASE(WebVTT_AlignCueSettingBecomesAlignmentTag)
{
    CSimpleTextSubtitle sts;
    REQUIRE(OpenFixture(sts, L"webvtt_tags.vtt"));
    REQUIRE_EQ(sts.GetCount(), (size_t)TAGS_COUNT);
    CHECK_EQ(sts[ALIGN_START].str, L"{\\an1}Aligned to the start");
    CHECK_EQ(sts[ALIGN_CENTER].str, L"{\\an2}Aligned to the centre");
    CHECK_EQ(sts[ALIGN_END].str, L"{\\an3}Aligned to the end");
    CHECK_EQ(StartMs(sts, ALIGN_START), 17000);
}

// #677: std::regex_replace on a narrow copy mangled everything outside ASCII.
// Every cue here goes through the tag-removal regexes.
TEST_CASE(WebVTT_UnicodeSurvivesTagRemoval)
{
    CSimpleTextSubtitle sts;
    REQUIRE(OpenFixture(sts, L"webvtt_unicode.vtt"));
    REQUIRE_EQ(sts.GetCount(), (size_t)4);
    CHECK_EQ(sts.GetStrW(0), L"Za\x017c\x00f3\x0142\x0107 g\x0119\x015bl\x0105 ja\x017a\x0144");
    CHECK_EQ(sts.GetStrW(1), L"\x65e5\x672c\x8a9e\x306e\x5b57\x5e55");
    CHECK_EQ(sts.GetStrW(2), L"\x0395\x03bb\x03bb\x03b7\x03bd\x03b9\x03ba\x03ac \x0438 \x043a\x0438\x0440\x0438\x043b\x043b\x0438\x0446\x0430");
    CHECK_EQ(sts.GetStrW(3), L"<i>emoji \xd83d\xde00 outside the BMP</i>");
    CHECK(sts[0].fUnicode);
}

TEST_CASE(WebVTT_Utf8BomAndCrLf)
{
    CSimpleTextSubtitle sts;
    REQUIRE(OpenText(sts, L"bom.vtt", "\xEF\xBB\xBFWEBVTT\r\n\r\n00:00:01.000 --> 00:00:02.000\r\nline one\r\nline two\r\n\r\n00:00:03.000 --> 00:00:04.000\r\nnext\r\n"));
    REQUIRE_EQ(sts.GetCount(), (size_t)2);
    CHECK_EQ(sts[0].str, L"line one\\Nline two");
    CHECK_EQ(sts[1].str, L"next");
}

TEST_CASE(WebVTT_FileWithoutSignatureIsNotWebVTT)
{
    // The .vtt extension only decides which parser is tried first. Without
    // the signature the WebVTT parser declines and the SubRip one, which does
    // not insist on cue numbers, takes the file.
    CSimpleTextSubtitle sts;
    REQUIRE(OpenText(sts, L"nosig.vtt", "00:00:01.000 --> 00:00:02.000\n<c.yellow>no header</c>\n"));
    CHECK_EQ(sts.m_subtitleType, Subtitle::SRT);
    REQUIRE_EQ(sts.GetCount(), (size_t)1);
    CHECK_EQ(sts[0].str, L"<c.yellow>no header</c>");
}

TEST_CASE(WebVTT_HeaderOnlyIsAnEmptyButValidTrack)
{
    // Embedded tracks deliver the header first and the cues later.
    CSimpleTextSubtitle sts;
    CHECK(OpenText(sts, L"header.vtt", "WEBVTT\n"));
    CHECK_EQ(sts.GetCount(), (size_t)0);
    CHECK_EQ(sts.m_subtitleType, Subtitle::VTT);
}

// --- colour classes and STYLE blocks: #930, #992, #1023, #1054, #1806 -------

namespace
{
    // fixtures/webvtt_styles.vtt, in file order
    enum { S_WARN, S_SHADE, S_BOTH, S_DEFAULT_CLASS, S_DEFAULT_BG, S_TAG_RULE, S_NESTED, STYLES_COUNT };
}

TEST_CASE(WebVTT_StyleBlockClassColour)
{
    CSimpleTextSubtitle sts;
    REQUIRE(OpenFixture(sts, L"webvtt_styles.vtt"));
    REQUIRE_EQ(sts.GetCount(), (size_t)STYLES_COUNT);
    // SSA colours are BGR: #ff0000 is &H0000ff&
    CHECK_EQ(sts[S_WARN].str, L"{\\c&H0000ff&}red text");
    CHECK_EQ(StartMs(sts, S_WARN), 1000);
}

// #1806: "background-color" must not be read as "color", and rgb() is accepted
TEST_CASE(WebVTT_StyleBlockBackgroundColourRgb)
{
    CSimpleTextSubtitle sts;
    REQUIRE(OpenFixture(sts, L"webvtt_styles.vtt"));
    REQUIRE_EQ(sts.GetCount(), (size_t)STYLES_COUNT);
    CHECK_EQ(sts[S_SHADE].str, L"{\\3c&Hff0000&}blue background");
}

TEST_CASE(WebVTT_DefaultColourClasses)
{
    CSimpleTextSubtitle sts;
    REQUIRE(OpenFixture(sts, L"webvtt_styles.vtt"));
    REQUIRE_EQ(sts.GetCount(), (size_t)STYLES_COUNT);
    // the style is reset after the span so that the rest of the line is plain
    CHECK_EQ(sts[S_DEFAULT_CLASS].str, L"{\\c&H00ffff&}default class{\\r} then plain");
    CHECK_EQ(sts[S_DEFAULT_BG].str, L"{\\3c&H0000ff&}default background class");
}

TEST_CASE(WebVTT_StyleRuleForAPlainTag)
{
    CSimpleTextSubtitle sts;
    REQUIRE(OpenFixture(sts, L"webvtt_styles.vtt"));
    REQUIRE_EQ(sts.GetCount(), (size_t)STYLES_COUNT);
    CHECK_EQ(sts[S_TAG_RULE].str, L"{\\b1}{\\c&H00ffff&}styled bold{\\b}");
}

// #1054: the outer colour comes back when the inner span closes
TEST_CASE(WebVTT_NestedColourSpansRestoreTheOuterColour)
{
    CSimpleTextSubtitle sts;
    REQUIRE(OpenFixture(sts, L"webvtt_styles.vtt"));
    REQUIRE_EQ(sts.GetCount(), (size_t)STYLES_COUNT);
    CHECK_EQ(sts[S_NESTED].str, L"{\\c&H0000ff&}outer {\\c&H00ffff&}inner{\\c&H0000ff&} outer again");
}

// #992: a rule that sets only the foreground
TEST_CASE(WebVTT_StyleRuleColourFirstDeclarationIsApplied)
{
    CSimpleTextSubtitle sts;
    REQUIRE(OpenFixture(sts, L"webvtt_styles.vtt"));
    REQUIRE_EQ(sts.GetCount(), (size_t)STYLES_COUNT);
    CHECK(sts[S_BOTH].str.Find(L"{\\c&H00ff00&}") >= 0);
    CHECK_EQ(sts.GetStrW(S_BOTH), L"lime on black");
}

TEST_CASE_EXPECTED_FAILURE(WebVTT_StyleRuleWithColourAndBackground,
                           "current bug: since #1806 anchored the declaration regex at the start of the rule body, only the FIRST "
                           "declaration of a ::cue rule is read; '{ color: lime; background: #000080; }' loses its background")
{
    CSimpleTextSubtitle sts;
    REQUIRE(OpenFixture(sts, L"webvtt_styles.vtt"));
    REQUIRE_EQ(sts.GetCount(), (size_t)STYLES_COUNT);
    CHECK(sts[S_BOTH].str.Find(L"{\\c&H00ff00&}") >= 0);
    CHECK(sts[S_BOTH].str.Find(L"{\\3c&H800000&}") >= 0);
}

TEST_CASE_EXPECTED_FAILURE(WebVTT_BlackIsBlack,
                           "current bug: SSAColorTag treats a parsed value of 0 as 'could not parse' and substitutes white, so the default "
                           "class .black, '#000000' and rgb(0,0,0) all render white (the named colour 'black' works)")
{
    CSimpleTextSubtitle sts;
    REQUIRE(OpenText(sts, L"black.vtt",
                     "WEBVTT\n\nSTYLE\n::cue(.ink) { color: #000000; }\n\n"
                     "00:01.000 --> 00:02.000\n<c.black>default class</c>\n\n"
                     "00:03.000 --> 00:04.000\n<c.ink>hex</c>\n"));
    REQUIRE_EQ(sts.GetCount(), (size_t)2);
    CHECK_EQ(sts[0].str, L"{\\c&H000000&}default class");
    CHECK_EQ(sts[1].str, L"{\\c&H000000&}hex");
}

TEST_CASE(WebVTT_NamedBlackWorks)
{
    CSimpleTextSubtitle sts;
    REQUIRE(OpenText(sts, L"named-black.vtt", "WEBVTT\n\nSTYLE\n::cue(.ink) { color: black; }\n\n00:01.000 --> 00:02.000\n<c.ink>named</c>\n"));
    REQUIRE_EQ(sts.GetCount(), (size_t)1);
    CHECK_EQ(sts[0].str, L"{\\c&H000000&}named");
}

TEST_CASE(WebVTT_DefaultCueStyleAppliesToEveryCue)
{
    CSimpleTextSubtitle sts;
    REQUIRE(OpenText(sts, L"cue-default.vtt",
                     "WEBVTT\n\nSTYLE\n::cue {\n  color: rgb(255, 0, 0);\n}\n\n"
                     "00:01.000 --> 00:02.000\none\n\n00:03.000 --> 00:04.000\ntwo\n"));
    REQUIRE_EQ(sts.GetCount(), (size_t)2);
    CHECK_EQ(sts[0].str, L"{\\c&H0000ff&}one");
    CHECK_EQ(sts[1].str, L"{\\c&H0000ff&}two");
}

// Seen in the wild and tolerated by the parser: "Style:" in place of "STYLE".
TEST_CASE(WebVTT_MisspelledStyleBlockHeader)
{
    CSimpleTextSubtitle sts;
    REQUIRE(OpenText(sts, L"style-colon.vtt", "WEBVTT\n\nStyle:\n::cue(.hot) { color: #ff0000; }\n\n00:01.000 --> 00:02.000\n<c.hot>x</c>\n"));
    REQUIRE_EQ(sts.GetCount(), (size_t)1);
    CHECK_EQ(sts[0].str, L"{\\c&H0000ff&}x");
}

TEST_CASE(WebVTT_StyleBlockRunningIntoTheFirstCueLosesNothing)
{
    // No blank line between the STYLE block and the first cue timing.
    CSimpleTextSubtitle sts;
    REQUIRE(OpenText(sts, L"style-nogap.vtt", "WEBVTT\n\nSTYLE\n::cue(.hot) { color: #ff0000; }\n00:01.000 --> 00:02.000\n<c.hot>x</c>\n"));
    REQUIRE_EQ(sts.GetCount(), (size_t)1);
    CHECK_EQ(StartMs(sts, 0), 1000);
    CHECK_EQ(sts[0].str, L"{\\c&H0000ff&}x");
}

// --- duplicates -------------------------------------------------------------

TEST_CASE(WebVTT_RepeatedCueInFileIsDropped)
{
    // HLS segmenters repeat the cue that straddles a segment boundary.
    CSimpleTextSubtitle sts;
    REQUIRE(OpenText(sts, L"dupes.vtt",
                     "WEBVTT\n\n"
                     "00:00:01.000 --> 00:00:02.000\nsame\n\n"
                     "00:00:01.000 --> 00:00:02.000\nsame\n\n"
                     "00:00:02.000 --> 00:00:03.000\nsame\n\n"
                     "00:00:04.000 --> 00:00:05.000\nother\n"));
    REQUIRE_EQ(sts.GetCount(), (size_t)3);
    CHECK_EQ(StartMs(sts, 0), 1000);
    CHECK_EQ(StartMs(sts, 1), 2000);
    CHECK_EQ(sts[2].str, L"other");
}

// #4197. Embedded WebVTT arrives cue by cue through Add(). The duplicate
// check looks up the segment starting at the cue's start time; for a cue
// later than everything so far -- the normal case -- that lookup returns the
// end of the array, which the old code dereferenced. The read stays inside
// the array's spare capacity, so it cannot be caught without ASan: what this
// pins is the behaviour around it -- ordered cues are all kept, a repeat is
// dropped, an overlap is not mistaken for one.
TEST_CASE(WebVTT_AddKeepsOrderedCuesAndDropsARepeat)
{
    CSimpleTextSubtitle sts;
    sts.m_subtitleType = Subtitle::VTT;
    for (int i = 0; i < 40; i++) {
        CStringW text;
        text.Format(L"cue %d", i);
        sts.Add(text, true, MS2RT(1000 * i), MS2RT(1000 * i + 900));
    }
    REQUIRE_EQ(sts.GetCount(), (size_t)40);

    sts.Add(L"cue 7", true, MS2RT(7000), MS2RT(7900));          // exact repeat
    CHECK_EQ(sts.GetCount(), (size_t)40);
    sts.Add(L"overlap", true, MS2RT(7000), MS2RT(8500));        // same start, other end
    CHECK_EQ(sts.GetCount(), (size_t)41);
    sts.Add(L"after all", true, MS2RT(100000), MS2RT(101000));  // beyond the last segment
    CHECK_EQ(sts.GetCount(), (size_t)42);
    sts.Add(L"before all", true, MS2RT(0), MS2RT(500));
    CHECK_EQ(sts.GetCount(), (size_t)43);
}

// The embedded path also has to strip what a demuxer leaves in the payload:
// the cue identifier and the cue settings line.
TEST_CASE(WebVTT_AddStripsCueIdentifierAndSettings)
{
    CSimpleTextSubtitle sts;
    sts.m_subtitleType = Subtitle::VTT;
    sts.Add(L"12\nalign:start position:10%\n<i>Hello</i> &amp; goodbye", true, MS2RT(1000), MS2RT(2000));
    REQUIRE_EQ(sts.GetCount(), (size_t)1);
    CHECK_EQ(sts.GetStrW(0), L"<i>Hello</i> & goodbye");
    CHECK(sts[0].str.Find(L"{\\an1}") == 0);
    CHECK(sts[0].str.Find(L"{\\i1}Hello{\\i}") > 0);
}

TEST_CASE(WebVTT_AddDropsACueThatIsOnlyMarkup)
{
    CSimpleTextSubtitle sts;
    sts.m_subtitleType = Subtitle::VTT;
    sts.Add(L"<c></c>", true, MS2RT(1000), MS2RT(2000));
    CHECK_EQ(sts.GetCount(), (size_t)0);
}
