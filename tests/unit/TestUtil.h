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

#include "MpcTest.h"
#include <initializer_list>
#include <vector>

// Helpers shared by the test files: fixture paths, scratch files, byte builders.

namespace testutil
{
    inline CStringW Fixture(LPCWSTR name)
    {
        return mpctest::FixtureDir() + name;
    }

    // Writes raw bytes to a file in the run's scratch directory and returns its path.
    inline CStringW WriteTemp(LPCWSTR name, const void* data, size_t size)
    {
        CStringW path = mpctest::TempDir() + name;
        HANDLE h = CreateFileW(path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (h == INVALID_HANDLE_VALUE) {
            FAIL("cannot create scratch file " + mpctest::ToUtf8(path));
            throw mpctest::AbortTest();
        }
        DWORD written = 0;
        WriteFile(h, data, (DWORD)size, &written, nullptr);
        CloseHandle(h);
        return path;
    }

    inline CStringW WriteTemp(LPCWSTR name, const std::vector<BYTE>& bytes)
    {
        return WriteTemp(name, bytes.data(), bytes.size());
    }

    inline CStringW WriteTemp(LPCWSTR name, const std::string& bytes)
    {
        return WriteTemp(name, bytes.data(), bytes.size());
    }

    inline std::vector<BYTE> ReadAll(LPCWSTR path)
    {
        std::vector<BYTE> out;
        HANDLE h = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
        if (h == INVALID_HANDLE_VALUE) {
            FAIL("cannot open " + mpctest::ToUtf8(path));
            throw mpctest::AbortTest();
        }
        out.resize(GetFileSize(h, nullptr));
        DWORD read = 0;
        if (!out.empty()) {
            ReadFile(h, out.data(), (DWORD)out.size(), &read, nullptr);
        }
        CloseHandle(h);
        return out;
    }

    // A copy of the input placed so that its last byte is the last byte of a
    // page, with an inaccessible page after it. A parser that reads even one
    // byte past its input takes an access violation here, every time, instead
    // of reading whatever the heap had next to it: an out-of-bounds read
    // becomes a test failure without needing ASan.
    class GuardedBuffer
    {
        BYTE* m_base = nullptr;
        BYTE* m_data = nullptr;
        size_t m_size = 0;

    public:
        explicit GuardedBuffer(const std::vector<BYTE>& bytes) : m_size(bytes.size()) {
            SYSTEM_INFO si;
            GetSystemInfo(&si);
            const size_t page = si.dwPageSize;
            const size_t pages = (m_size + page - 1) / page + 1; // at least one, plus the guard
            m_base = (BYTE*)VirtualAlloc(nullptr, pages * page, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
            if (!m_base) {
                FAIL("VirtualAlloc failed");
                throw mpctest::AbortTest();
            }
            DWORD old;
            VirtualProtect(m_base + (pages - 1) * page, page, PAGE_NOACCESS, &old);
            m_data = m_base + (pages - 1) * page - m_size;
            if (m_size) {
                memcpy(m_data, bytes.data(), m_size);
            }
        }
        ~GuardedBuffer() {
            if (m_base) {
                VirtualFree(m_base, 0, MEM_RELEASE);
            }
        }
        GuardedBuffer(const GuardedBuffer&) = delete;
        GuardedBuffer& operator=(const GuardedBuffer&) = delete;

        BYTE* data() const { return m_data; }
        size_t size() const { return m_size; }
    };

    // Big-endian byte builder for hand-made binary fixtures.
    class Bytes : public std::vector<BYTE>
    {
    public:
        Bytes() = default;
        Bytes(std::initializer_list<BYTE> l) : std::vector<BYTE>(l) {}
        Bytes& u8(unsigned v) { push_back((BYTE)v); return *this; }
        Bytes& u16(unsigned v) { u8(v >> 8); return u8(v); }
        Bytes& u24(unsigned v) { u8(v >> 16); return u16(v); }
        Bytes& u32(unsigned v) { u16(v >> 16); return u16(v); }
        Bytes& fill(size_t n, BYTE v = 0) { insert(end(), n, v); return *this; }
        Bytes& str(const char* s) { insert(end(), s, s + strlen(s)); return *this; }
        Bytes& add(const std::vector<BYTE>& o) { insert(end(), o.begin(), o.end()); return *this; }
    };
}
