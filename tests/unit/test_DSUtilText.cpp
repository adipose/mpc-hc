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
#include "TestUtil.h"
#include "../../src/DSUtil/ISOLang.h"
#include "../../src/DSUtil/PathUtils.h"
#include "../../src/DSUtil/text.h"

// Pure helpers in src/DSUtil: language-code mapping (ISOLang.cpp), path and
// filename handling (PathUtils.cpp) and the text helpers (text.cpp, DSUtil.cpp).
// Modelled on #632/#927/#3321/#3452 (language tags), #2612/#2525 (paths) and
// #334/#591/#1376 (URL and UTF-8 decoding).

using namespace testutil;

// --- ISOLang ----------------------------------------------------------------

TEST_CASE(ISOLang_TwoAndThreeLetterCodes)
{
    CHECK_EQ(ISOLang::ISO6391ToLanguage("en"), L"English");
    CHECK_EQ(ISOLang::ISO6392ToLanguage("eng"), L"English");
    CHECK_EQ(ISOLang::ISO6391ToLanguage("fr"), L"French");
    CHECK_EQ(ISOLang::ISO6392ToLanguage("deu"), L"German"); // ISO 639-2/T
    CHECK_EQ(ISOLang::ISO6392ToLanguage("ger"), L"German"); // ISO 639-2/B
    CHECK_EQ(ISOLang::ISO6391ToLcid("en"), MAKELCID(MAKELANGID(LANG_ENGLISH, SUBLANG_DEFAULT), SORT_DEFAULT));
    CHECK_EQ(ISOLang::ISO6392ToLcid("fre"), MAKELCID(MAKELANGID(LANG_FRENCH, SUBLANG_DEFAULT), SORT_DEFAULT));
}

TEST_CASE(ISOLang_UnknownCode)
{
    CHECK_EQ(ISOLang::ISO6391ToLanguage("xx"), L"");
    // ISO6392ToLanguage echoes the input when it has no entry
    CHECK_EQ(ISOLang::ISO6392ToLanguage("xyz"), L"xyz");
    CHECK_EQ(ISOLang::ISO6391ToLcid("xx"), (LCID)0);
    CHECK_EQ(ISOLang::ISO6392ToLcid("xyz"), (LCID)0);
}

// #632: a truncated BCP-47 code (639-1 with a region, e.g. "en-US") should
// fall back to the 639-1 lookup rather than fail.
TEST_CASE(ISOLang_TruncatedBcp47FallsBackTo6391)
{
    CHECK_EQ(ISOLang::ISO6392ToLcid("en-"), MAKELCID(MAKELANGID(LANG_ENGLISH, SUBLANG_DEFAULT), SORT_DEFAULT));
    CHECK_EQ(ISOLang::ISO639XToLanguage("en-"), L"English");
    CHECK_EQ(ISOLang::ISO639XToLanguage("fr"), L"French");
    CHECK_EQ(ISOLang::ISO639XToLanguage("eng"), L"English");
}

// #927: the Traditional/Simplified region tags
TEST_CASE(ISOLang_ChineseRegionTags)
{
    CHECK_EQ(ISOLang::ISO639XToLanguage("zh-CN"), L"Chinese (Simplified)");
    CHECK_EQ(ISOLang::ISO639XToLanguage("zh-TW"), L"Chinese (Traditional)");
    CHECK_EQ(ISOLang::ISO639XToLanguage("pt-BR"), L"Portuguese (Brazil)");
    CHECK_EQ(ISOLang::ISO639XToLanguage("pt-PT"), L"Portuguese");
}

// #3452: bare "zh" is just "Chinese", not "Chinese (Simplified)"
TEST_CASE(ISOLang_BareChineseIsNotSimplified)
{
    CHECK_EQ(ISOLang::ISO6391ToLanguage("zh"), L"Chinese");
    CHECK_EQ(ISOLang::ISO6392ToLanguage("chi"), L"Chinese");
    CHECK_EQ(ISOLang::ISO6392ToLanguage("zho"), L"Chinese");
}

// #3321: "srp" is Serbian, not Croatian
TEST_CASE(ISOLang_SerbianAndCroatianAreDistinct)
{
    CHECK_EQ(ISOLang::ISO6392ToLanguage("srp"), L"Serbian");
    CHECK_EQ(ISOLang::ISO6392ToLanguage("scc"), L"Serbian");
    CHECK_EQ(ISOLang::ISO6392ToLanguage("hrv"), L"Croatian");
    CHECK_NE(ISOLang::ISO6392ToLcid("srp"), ISOLang::ISO6392ToLcid("hrv"));
    CHECK_EQ(PRIMARYLANGID(LANGIDFROMLCID(ISOLang::ISO6392ToLcid("srp"))), (WORD)LANG_SERBIAN);
}

TEST_CASE(ISOLang_RoundTripThrough6391And6392)
{
    CHECK_EQ(ISOLang::ISO6391To6392("en"), "eng");
    CHECK_EQ(ISOLang::ISO6392To6391("eng"), L"en");
    CHECK(ISOLang::IsISO6391("en"));
    CHECK(ISOLang::IsISO6392("eng"));
    CHECK_FALSE(ISOLang::IsISO6392("en"));
}

// --- PathUtils --------------------------------------------------------------

TEST_CASE(PathUtils_NameAndExtension)
{
    // BaseName keeps the extension, FileName drops it
    CHECK_EQ(PathUtils::BaseName(L"C:\\movies\\clip.mkv"), L"clip.mkv");
    CHECK_EQ(PathUtils::FileName(L"C:\\movies\\clip.mkv"), L"clip");
    CHECK_EQ(PathUtils::FileExt(L"C:\\movies\\clip.mkv"), L".mkv");
    CHECK_EQ(PathUtils::DirName(L"C:\\movies\\clip.mkv"), L"C:\\movies");
}

// #2612: control characters are not valid in a filename either
TEST_CASE(PathUtils_FilterInvalidChars)
{
    CHECK_EQ(PathUtils::FilterInvalidCharsFromFileName(L"a<b>c:d\"e/f\\g|h?i*j"), L"a_b_c_d_e_f_g_h_i_j");
    CHECK_EQ(PathUtils::FilterInvalidCharsFromFileName(L"tab\tnewline\r\nhere"), L"tab_newline__here");
    CHECK_EQ(PathUtils::FilterInvalidCharsFromFileName(L"perfectly.valid_name"), L"perfectly.valid_name");
    CHECK_EQ(PathUtils::FilterInvalidCharsFromFileName(L"x/y", L'-'), L"x-y");
}

// #2525: a trailing backslash must not become a double one when combined
TEST_CASE(PathUtils_CombineWithTrailingBackslash)
{
    CHECK_EQ(PathUtils::CombinePaths(L"\\\\NAS\\share\\", L"BDMV\\index.bdmv"), L"\\\\NAS\\share\\BDMV\\index.bdmv");
    CHECK_EQ(PathUtils::CombinePaths(L"C:\\dir", L"file.txt"), L"C:\\dir\\file.txt");
    CHECK_EQ(PathUtils::CombinePaths(L"C:\\dir\\", L"file.txt"), L"C:\\dir\\file.txt");
}

TEST_CASE(PathUtils_UrlAndFullPathClassification)
{
    CString http = L"http://example.com/a.mkv";
    CString unc = L"\\\\server\\share\\a.mkv";
    CString drive = L"C:\\a.mkv";
    CString rel = L"sub\\a.mkv";
    CString fileUrl = L"file://server/a.mkv";
    CHECK(PathUtils::IsURL(http));
    CHECK_FALSE(PathUtils::IsURL(drive));
    CHECK_FALSE(PathUtils::IsURL(fileUrl)); // file: is a local path, not a remote URL
    CHECK(PathUtils::IsFullFilePath(drive));
    CHECK(PathUtils::IsFullFilePath(unc));
    CHECK_FALSE(PathUtils::IsFullFilePath(rel));
    CHECK_FALSE(PathUtils::IsFullFilePath(http));
}

// #1717, #3766, #3992: long paths get the \\?\ prefix, URLs do not
TEST_CASE(PathUtils_ExtendMaxPathLength)
{
    CString shortPath = L"C:\\short\\path.mkv";
    ExtendMaxPathLengthIfNeeded(shortPath);
    CHECK_EQ(shortPath, L"C:\\short\\path.mkv");

    CString longPath = L"C:\\";
    longPath.Append(CString('a', 300));
    longPath += L"\\file.mkv";
    ExtendMaxPathLengthIfNeeded(longPath);
    CHECK_EQ(longPath.Left(4), L"\\\\?\\");

    CString longUnc = L"\\\\server\\share\\";
    longUnc.Append(CString('b', 300));
    ExtendMaxPathLengthIfNeeded(longUnc);
    CHECK_EQ(longUnc.Left(8), L"\\\\?\\UNC\\");

    CString url = L"http://example.com/";
    url.Append(CString('c', 300));
    ExtendMaxPathLengthIfNeeded(url);
    CHECK_EQ(url.Left(4), L"http");
}

TEST_CASE(PathUtils_StripPathOrUrl)
{
    CHECK_EQ(PathUtils::StripPathOrUrl(L"C:\\movies\\clip.mkv"), L"clip.mkv");
    CHECK_EQ(PathUtils::StripPathOrUrl(L"http://example.com/path/clip%20one.mkv"), L"clip one.mkv");
}

// --- text.cpp / DSUtil.cpp --------------------------------------------------

TEST_CASE(Text_UrlEncodeDecodeRoundTrip)
{
    CStringA plain = "a b&c=d/e?f";
    CStringA encoded = UrlEncode(plain);
    CHECK(encoded.Find(' ') < 0);
    CHECK_EQ(UrlDecode(encoded), plain);
    CHECK_EQ(UrlDecode("%20%26%3D"), " &=");
}

// #591: '+' means space, and the result is decoded as UTF-8
TEST_CASE(Text_UrlDecodeWithUtf8)
{
    CHECK_EQ(UrlDecodeWithUTF8(L"one+two"), L"one two");
    CHECK_EQ(UrlDecodeWithUTF8(L"caf%C3%A9"), L"caf\x00e9");
    CHECK_EQ(UrlDecodeWithUTF8(L"%E6%97%A5%E6%9C%AC"), L"\x65e5\x672c");
}

TEST_CASE(Text_UrlGetHostName)
{
    CHECK_EQ(URLGetHostName(L"https://www.example.com/path/to/x"), L"example.com");
    CHECK_EQ(URLGetHostName(L"http://host.local:8080/a"), L"host.local:8080");
}

// #334, #1376: UTF-8 to UTF-16, including sequences beyond the BMP
TEST_CASE(Text_Utf8To16)
{
    CHECK_EQ(UTF8To16("plain ascii"), L"plain ascii");
    CHECK_EQ(UTF8To16("caf\xC3\xA9"), L"caf\x00e9");
    CHECK_EQ(UTF8To16("\xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E"), L"\x65e5\x672c\x8a9e");
    CHECK_EQ(UTF8To16("emoji \xF0\x9F\x98\x80"), L"emoji \xd83d\xde00");
    CHECK_EQ(UTF8To16(""), L"");
}

TEST_CASE(Text_Utf8ToStringW)
{
    CHECK_EQ(UTF8ToStringW("caf\xC3\xA9"), L"caf\x00e9");
    CHECK_EQ(UTF8ToStringW("\xE6\x97\xA5\xE6\x9C\xAC"), L"\x65e5\x672c");
    CHECK_EQ(UTF8ToStringW(nullptr), L"");
    // a lone continuation byte is invalid: it yields the empty string
    CHECK_EQ(UTF8ToStringW("\x80"), L"");
}

TEST_CASE_EXPECTED_FAILURE(Text_Utf8ToStringWFourByte,
                           "current bug: the 4-byte branch of UTF8ToStringW (DSUtil.cpp) writes the code point into one wchar_t instead of a "
                           "surrogate pair, and uses '||' where it means '|', so U+1F600 comes out as U+0001. UTF8To16 does this correctly")
{
    CHECK_EQ(UTF8ToStringW("emoji \xF0\x9F\x98\x80"), L"emoji \xd83d\xde00");
}

TEST_CASE(Text_HtmlSpecialCharsDecode)
{
    CHECK_EQ(HtmlSpecialCharsDecode("Tom &amp; Jerry &lt;3 &gt; 2 &quot;q&quot;"), "Tom & Jerry <3 > 2 \"q\"");
}

TEST_CASE(Text_StartsEndsWith)
{
    CHECK(StartsWith(L"hello world", L"hello"));
    CHECK_FALSE(StartsWith(L"hello", L"world"));
    CHECK(EndsWith(L"clip.mkv", L".mkv"));
    CHECK(EndsWithNoCase(L"CLIP.MKV", L".mkv"));
    CHECK_FALSE(EndsWith(L"CLIP.MKV", L".mkv"));
}

TEST_CASE(Text_ExplodeRespectsLimitAndTrims)
{
    CAtlList<CString> parts;
    Explode(CString(L" a , b , c "), parts, L',');
    REQUIRE_EQ(parts.GetCount(), (size_t)3);
    CHECK_EQ(parts.GetHead(), L"a");
    CHECK_EQ(parts.GetTail(), L"c");

    CAtlList<CString> limited;
    Explode(CString(L"a=b=c"), limited, L'=', 2);
    REQUIRE_EQ(limited.GetCount(), (size_t)2);
    CHECK_EQ(limited.GetHead(), L"a");
    CHECK_EQ(limited.GetTail(), L"b=c"); // the rest is left intact
}

// #4130: menu labels made from untrusted names
TEST_CASE(Text_SanitizeMenuLabel)
{
    CHECK_EQ(SanitizeMenuLabel(L"Fish & Chips"), L"Fish && Chips");
    CHECK_EQ(SanitizeMenuLabel(L"tab\tseparated"), L"tab separated");
    CHECK_EQ(SanitizeMenuLabel(L"  spaced  "), L"spaced");
    CHECK_EQ(SanitizeMenuLabel(L""), L" "); // never empty: an item needs something to measure

    CStringW longName(L'x', 400);
    CStringW label = SanitizeMenuLabel(longName);
    CHECK(label.GetLength() <= MENU_NAME_MAX);
    CHECK_EQ(label.Right(1), L"\x2026"); // truncated with a horizontal-ellipsis character
}
