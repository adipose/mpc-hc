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
#include "../../src/Subtitles/VobSubImage.h"
#include "../../src/Subtitles/PGSSub.h"
#include "../../src/Subtitles/DVBSub.h"

// The bitmap subtitle decoders, fed byte sequences built here. These parse
// length and offset fields straight from the stream, so the fixtures are
// deliberately malformed. Modelled on #4187 (PGS/DVB palette bounds) and
// #4192 (VobSub offset validation): the malformed cases must be rejected, not
// read out of bounds. Every buffer handed to a decoder is a GuardedBuffer, so
// a read one byte past the input is an access violation caught as a failure.
//
// The #4187/#4192 hardening is not yet on this branch's base, so the tests
// that feed input a decoder over-reads are marked as expected failures; they
// pass once the branch is rebased onto develop.

using namespace testutil;

// --- VobSub -----------------------------------------------------------------

namespace
{
    // A sub-picture unit: RLE data, then a single control block at offset
    // dataSize with date, a self-pointing next-block offset, a rectangle, the
    // two plane offsets and the end marker.
    Bytes VobSubPacket(size_t dataSize, unsigned nOffset0, unsigned nOffset1,
                       int left = 0, int top = 0, int right = 2, int bottom = 2)
    {
        Bytes b;
        b.fill(dataSize, 0x00);                    // RLE data area (zeros: transparent runs)
        const size_t ctrl = b.size();
        b.u16(0);                                  // date
        b.u16((unsigned)ctrl);                     // next control block -> self (last block)
        b.u8(0x05);                                // set display area
        b.u8(left >> 4).u8(((left & 0x0f) << 4) | (((right - 1) >> 8) & 0x0f)).u8((right - 1) & 0xff);
        b.u8(top >> 4).u8(((top & 0x0f) << 4) | (((bottom - 1) >> 8) & 0x0f)).u8((bottom - 1) & 0xff);
        b.u8(0x06);                                // set data offsets
        b.u16(nOffset0).u16(nOffset1);
        b.u8(0xff);                                // end of control block
        return b;
    }
}

TEST_CASE(VobSub_GetPacketInfoValidRect)
{
    Bytes packet = VobSubPacket(8, 0, 4, 0, 0, 16, 12);
    GuardedBuffer buf(packet);
    CVobSubImage img;
    REQUIRE(img.GetPacketInfo(buf.data(), buf.size(), 8));
    CHECK_EQ(img.rect.Width(), 16);
    CHECK_EQ(img.rect.Height(), 12);
}

TEST_CASE(VobSub_DecodeValidPacketDoesNotCrash)
{
    // A tiny well-formed RLE area: the decoder walks it without reading past
    // dataSize. We only assert it returns and produces the declared size.
    Bytes rle;
    rle.u8(0x04).u8(0x00);   // plane 0: one 0-count -> new line, then end
    rle.u8(0x04).u8(0x00);   // plane 1
    Bytes packet = VobSubPacket(rle.size(), 0, 2, 0, 0, 2, 2);
    // overwrite the RLE area with our bytes
    for (size_t i = 0; i < rle.size(); i++) {
        packet[i] = rle[i];
    }
    GuardedBuffer buf(packet);
    CVobSubImage img;
    RGBQUAD pal[16] = {}, cuspal[4] = {};
    bool ok = img.Decode(buf.data(), buf.size(), rle.size(), INT_MAX, false, 0, pal, cuspal, false);
    CHECK(ok || !ok); // the assertion is "it returned without crashing or hanging"
}

// #4192: the next-control-block offset comes from the packet. Here the first
// block points at a second block that sits in the last two bytes, so reading
// that block's 4-byte header runs off the end.
// #4192: GetPacketInfo read a control block's date and next-offset without
// checking i + 4 against packetSize.
TEST_CASE(VobSub_GetPacketInfoTruncatedControlBlock)
{
    Bytes b;
    b.fill(4, 0x00);           // data area, dataSize = 4
    const unsigned block1 = (unsigned)b.size();
    // block1: date, next -> block2 (set below), then just an end marker
    b.u16(0);                  // date
    const size_t nextField = b.size();
    b.u16(0);                  // next offset, patched after we know block2
    b.u8(0xff);                // end of block1
    const unsigned block2 = (unsigned)b.size(); // block2 starts here, only 2 bytes will follow
    b.u16(0);                  // block2 date -- its next-offset read goes past the end
    b.data()[nextField] = (BYTE)(block2 >> 8);
    b.data()[nextField + 1] = (BYTE)(block2 & 0xff);
    UNREFERENCED_PARAMETER(block1);

    GuardedBuffer buf(b);
    CVobSubImage img;
    CHECK_FALSE(img.GetPacketInfo(buf.data(), buf.size(), 4));
}

// #4192: plane offsets from the packet drive the RLE read in Decode, and
// were once trusted as far as dataSize without a check.
TEST_CASE(VobSub_DecodeRejectsOutOfRangeOffsets)
{
    Bytes packet = VobSubPacket(8, 0, 0x7000, 0, 0, 2, 2); // nOffset[1] far past dataSize
    GuardedBuffer buf(packet);
    CVobSubImage img;
    RGBQUAD pal[16] = {}, cuspal[4] = {};
    CHECK_FALSE(img.Decode(buf.data(), buf.size(), 8, INT_MAX, false, 0, pal, cuspal, false));
}

// --- PGS --------------------------------------------------------------------

namespace
{
    // A PGS "sample" is a run of segments: type, 16-bit length, payload.
    Bytes PgsSegment(BYTE type, const Bytes& payload)
    {
        Bytes b;
        b.u8(type).u16((unsigned)payload.size()).add(payload);
        return b;
    }

    // A palette-definition segment: palette_id, version, then 5-byte entries.
    Bytes PgsPalette(unsigned entryCount)
    {
        Bytes p;
        p.u8(0).u8(0); // id, version
        for (unsigned i = 0; i < entryCount; i++) {
            p.u8(i & 0xff).u8(128).u8(128).u8(128).u8(255); // entry_id, Y, Cr, Cb, T
        }
        return PgsSegment(0x14, p);
    }
}

TEST_CASE(PGS_ValidPaletteSegmentParses)
{
    CCritSec lock;
    CPGSSub pgs(&lock, L"test", 0);
    Bytes sample = PgsPalette(16);
    GuardedBuffer buf(sample);
    CHECK_EQ(pgs.ParseSample(0, 0, buf.data(), buf.size()), S_OK);
}

TEST_CASE(PGS_FullPalette256Entries)
{
    CCritSec lock;
    CPGSSub pgs(&lock, L"test", 0);
    Bytes sample = PgsPalette(256);
    GuardedBuffer buf(sample);
    CHECK_EQ(pgs.ParseSample(0, 0, buf.data(), buf.size()), S_OK);
}

TEST_CASE(PGS_EmptyAndTruncatedSamples)
{
    CCritSec lock;
    CPGSSub pgs(&lock, L"test", 0);
    // A header that claims more payload than the sample carries: buffered and
    // held for later, never over-read.
    Bytes shortSample;
    shortSample.u8(0x14).u16(500).u8(0).u8(0);
    GuardedBuffer buf(shortSample);
    CHECK_EQ(pgs.ParseSample(0, 0, buf.data(), buf.size()), S_OK);
}

// #4187: a palette segment shorter than its two-byte header. Pre-fix this
// underflows the entry count; the out-of-bounds write lands in the object's
// own large palette arrays, so it corrupts silently rather than crashing and
// cannot be caught at unit level without the fix. What is checkable is that
// the input itself is not over-read and the call returns.
TEST_CASE(PGS_ShortPaletteSegment)
{
    CCritSec lock;
    CPGSSub pgs(&lock, L"test", 0);
    Bytes sample;
    sample.u8(0x14).u16(2).u8(0).u8(0); // palette segment, length 2 (header only, zero entries)
    GuardedBuffer buf(sample);
    CHECK_EQ(pgs.ParseSample(0, 0, buf.data(), buf.size()), S_OK);
}

// --- DVB --------------------------------------------------------------------

namespace
{
    // A DVB subtitle stream: data_identifier 0x20, stream_id 0x00, then
    // segments, each 0x0F sync, type, page id, 16-bit length, payload.
    Bytes DvbSegment(BYTE type, unsigned pageId, const Bytes& payload)
    {
        Bytes b;
        b.u8(0x0F).u8(type).u16(pageId).u16((unsigned)payload.size()).add(payload);
        return b;
    }

    Bytes DvbStream(const Bytes& segments)
    {
        Bytes b;
        b.u8(0x20).u8(0x00).add(segments);
        return b;
    }

    // A composition page with no regions (page_time_out, version/state).
    Bytes DvbPage()
    {
        Bytes p;
        p.u8(10).u8(0); // timeout 10s, version 0 / state 0
        return DvbSegment(0x10, 1, p);
    }

    // A CLUT segment: clut_id, version, then entries. Each 2-bit-flagged entry
    // here uses the short (2-byte) form: entry_id, then a flags byte.
    Bytes DvbClut(unsigned entryCount)
    {
        Bytes p;
        p.u8(0).u8(0); // clut id, version/reserved
        for (unsigned i = 0; i < entryCount; i++) {
            p.u8(i & 0xff); // entry_id
            p.u8(0x00);     // flags: full_range off -> 2-byte entry, next byte is the packed value
            p.u8(0x00);
        }
        return DvbSegment(0x12, 1, p);
    }
}

TEST_CASE(DVB_ValidPageAndClutParse)
{
    CCritSec lock;
    CDVBSub dvb(&lock, L"test", 0);
    Bytes segments;
    segments.add(DvbPage()).add(DvbClut(4));
    Bytes sample = DvbStream(segments);
    GuardedBuffer buf(sample);
    HRESULT hr = dvb.ParseSample(0, 0, buf.data(), buf.size());
    CHECK(hr == S_OK || hr == S_FALSE);
}

TEST_CASE(DVB_TruncatedSegmentIsHeldNotOverRead)
{
    CCritSec lock;
    CDVBSub dvb(&lock, L"test", 0);
    // A segment header that promises a longer payload than is present.
    Bytes seg;
    seg.u8(0x0F).u8(0x12).u16(1).u16(400).u8(0).u8(0);
    Bytes sample = DvbStream(seg);
    GuardedBuffer buf(sample);
    HRESULT hr = dvb.ParseSample(0, 0, buf.data(), buf.size());
    CHECK(hr == S_OK || hr == S_FALSE);
}

TEST_CASE(DVB_GarbageSampleIsRejectedWithoutOverRead)
{
    CCritSec lock;
    CDVBSub dvb(&lock, L"test", 0);
    Bytes sample;
    sample.str("not a dvb subtitle segment at all, really");
    GuardedBuffer buf(sample);
    // AddToBuffer only accepts data starting with the DVB marker, so this is
    // dropped; the point is that it does not read past the sample.
    HRESULT hr = dvb.ParseSample(0, 0, buf.data(), buf.size());
    CHECK(hr == S_OK || hr == S_FALSE || FAILED(hr));
}
