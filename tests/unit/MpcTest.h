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

// A minimal unit-test harness: one header, no dependencies beyond the CRT,
// the STL, Win32 and ATL strings.
//
//   TEST_CASE(Group_Name) { CHECK(x); CHECK_EQ(a, b); REQUIRE(p != nullptr); }
//
//   TEST_CASE_EXPECTED_FAILURE(Group_Name, "issue, what is wrong") { ... }
//       documents a bug that exists today. It is reported as an expected
//       failure and does not fail the run; when it starts passing it is
//       reported as an unexpected pass, which does fail the run, so the
//       marker gets removed together with the fix.
//
//   TEST_CASE_ISOLATED(Group_Name) { ... }
//   TEST_CASE_ISOLATED_EXPECTED_FAILURE(Group_Name, "...") { ... }
//       run the test in a child process with a timeout. For input that may
//       hang the code under test or corrupt the heap: neither can be survived
//       in-process, and either would otherwise take every other result with it.
//
// CHECK* record a failure and continue; REQUIRE* abandon the test case.
// Inside a test, an access violation (or any structured exception), a C++
// exception, a CRT invalid-parameter report and an AfxMessageBox are each
// recorded as a failure of that test rather than ending or blocking the run.
//
// Exactly one translation unit defines MPCTEST_MAIN before including this
// header and calls mpctest::Main(argc, argv).

#ifndef MPCTEST_H
#define MPCTEST_H

#include <atlstr.h>
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <string>
#include <type_traits>
#include <vector>

namespace mpctest
{
    struct Failure {
        std::string file;
        int line;
        std::string message;
    };

    struct TestCase {
        const char* name;
        void (*fn)();
        const char* file;
        int line;
        const char* expectedFailure; // nullptr unless the test documents a current bug
        bool isolated;
    };

    struct AbortTest {};

    std::vector<TestCase>& Registry();
    void ReportFailure(const char* file, int line, const std::string& message);
    // Directory holding the fixture files, with a trailing backslash.
    const CStringW& FixtureDir();
    // A scratch directory private to this run, with a trailing backslash.
    const CStringW& TempDir();
    // Number of message boxes the code under test tried to show in this test.
    // They are swallowed and recorded as failures; a test that provokes one on
    // purpose calls ExpectMessageBox() first.
    int MessageBoxCount();
    void ExpectMessageBox();
    // For the application object: returns the button to "press".
    int OnMessageBox(const wchar_t* prompt);
    int Main(int argc, wchar_t** argv);

    struct Registrar {
        Registrar(const char* name, void (*fn)(), const char* file, int line, const char* expectedFailure, bool isolated) {
            Registry().push_back({ name, fn, file, line, expectedFailure, isolated });
        }
    };

    // --- value printing -----------------------------------------------------

    inline std::string ToUtf8(const wchar_t* s, int len = -1)
    {
        if (!s) {
            return "(null)";
        }
        if (len < 0) {
            len = (int)wcslen(s);
        }
        std::string out;
        if (len > 0) {
            int n = WideCharToMultiByte(CP_UTF8, 0, s, len, nullptr, 0, nullptr, nullptr);
            out.resize(n);
            WideCharToMultiByte(CP_UTF8, 0, s, len, &out[0], n, nullptr, nullptr);
        }
        return out;
    }

    // Control characters and non-ASCII are escaped so that a failure message
    // shows exactly which code units differ, whatever the console code page.
    inline std::string Quote(const wchar_t* s, int len)
    {
        std::string out = "L\"";
        char buf[16];
        for (int i = 0; i < len; i++) {
            wchar_t c = s[i];
            if (c == L'\n') {
                out += "\\n";
            } else if (c == L'\r') {
                out += "\\r";
            } else if (c == L'\t') {
                out += "\\t";
            } else if (c == L'"' || c == L'\\') {
                out += '\\';
                out += (char)c;
            } else if (c >= 0x20 && c < 0x7f) {
                out += (char)c;
            } else {
                sprintf_s(buf, "\\x%04x", (unsigned)c);
                out += buf;
            }
        }
        return out + "\"";
    }

    inline std::string Quote(const char* s, int len)
    {
        std::string out = "\"";
        char buf[16];
        for (int i = 0; i < len; i++) {
            unsigned char c = (unsigned char)s[i];
            if (c == '"' || c == '\\') {
                out += '\\';
                out += (char)c;
            } else if (c >= 0x20 && c < 0x7f) {
                out += (char)c;
            } else {
                sprintf_s(buf, "\\x%02x", (unsigned)c);
                out += buf;
            }
        }
        return out + "\"";
    }

    inline std::string Show(const CStringW& v) { return Quote((LPCWSTR)v, v.GetLength()); }
    inline std::string Show(const CStringA& v) { return Quote((LPCSTR)v, v.GetLength()); }
    inline std::string Show(const std::wstring& v) { return Quote(v.c_str(), (int)v.size()); }
    inline std::string Show(const std::string& v) { return Quote(v.c_str(), (int)v.size()); }
    inline std::string Show(const wchar_t* v) { return v ? Quote(v, (int)wcslen(v)) : "(null)"; }
    inline std::string Show(const char* v) { return v ? Quote(v, (int)strlen(v)) : "(null)"; }
    inline std::string Show(bool v) { return v ? "true" : "false"; }
    inline std::string Show(std::nullptr_t) { return "nullptr"; }

    template<typename T>
    inline std::string Show(const T& v)
    {
        char buf[64];
        if constexpr(std::is_enum_v<T>) {
            sprintf_s(buf, "%lld", (long long)v);
        } else if constexpr(std::is_floating_point_v<T>) {
            sprintf_s(buf, "%g", (double)v);
        } else if constexpr(std::is_pointer_v<T>) {
            sprintf_s(buf, "%p", (const void*)v);
        } else if constexpr(std::is_integral_v<T> && std::is_signed_v<T>) {
            sprintf_s(buf, "%lld", (long long)v);
        } else if constexpr(std::is_integral_v<T>) {
            sprintf_s(buf, "%llu (0x%llx)", (unsigned long long)v, (unsigned long long)v);
        } else {
            sprintf_s(buf, "(unprintable)");
        }
        return buf;
    }

    // Comparisons go through these so that a CString compared with a literal
    // uses CString's operator== rather than a pointer comparison.
    template<typename A, typename B>
    inline bool Equal(const A& a, const B& b) { return a == b; }
    inline bool Equal(const wchar_t* a, const wchar_t* b) { return a && b ? wcscmp(a, b) == 0 : a == b; }
    inline bool Equal(const char* a, const char* b) { return a && b ? strcmp(a, b) == 0 : a == b; }

    template<typename A, typename B>
    inline void CheckEq(const A& a, const B& b, bool wantEqual, const char* exprA, const char* exprB, const char* file, int line, bool require)
    {
#pragma warning(push)
#pragma warning(disable: 4389 4018) // signed/unsigned: the test author compares what the API returns
        const bool eq = Equal(a, b);
#pragma warning(pop)
        if (eq != wantEqual) {
            std::string msg = std::string(wantEqual ? "CHECK_EQ(" : "CHECK_NE(") + exprA + ", " + exprB + ")\n"
                              "      left:  " + Show(a) + "\n"
                              "      right: " + Show(b);
            ReportFailure(file, line, msg);
            if (require) {
                throw AbortTest();
            }
        }
    }

    inline void CheckTrue(bool ok, const char* macro, const char* expr, const char* file, int line, bool require)
    {
        if (!ok) {
            ReportFailure(file, line, std::string(macro) + "(" + expr + ")");
            if (require) {
                throw AbortTest();
            }
        }
    }
}

#define MPCTEST_CAT2(a, b) a##b
#define MPCTEST_CAT(a, b) MPCTEST_CAT2(a, b)

#define MPCTEST_DEFINE(name, xfail, isolated)                                                \
    static void MPCTEST_CAT(mpctest_fn_, name)();                                            \
    static mpctest::Registrar MPCTEST_CAT(mpctest_reg_, name)(#name,                         \
            &MPCTEST_CAT(mpctest_fn_, name), __FILE__, __LINE__, xfail, isolated);           \
    static void MPCTEST_CAT(mpctest_fn_, name)()

#define TEST_CASE(name)                                   MPCTEST_DEFINE(name, nullptr, false)
#define TEST_CASE_EXPECTED_FAILURE(name, reason)          MPCTEST_DEFINE(name, reason, false)
#define TEST_CASE_ISOLATED(name)                          MPCTEST_DEFINE(name, nullptr, true)
#define TEST_CASE_ISOLATED_EXPECTED_FAILURE(name, reason) MPCTEST_DEFINE(name, reason, true)

#define CHECK(expr)          mpctest::CheckTrue(!!(expr), "CHECK", #expr, __FILE__, __LINE__, false)
#define CHECK_FALSE(expr)    mpctest::CheckTrue(!(expr), "CHECK_FALSE", #expr, __FILE__, __LINE__, false)
#define CHECK_EQ(a, b)       mpctest::CheckEq((a), (b), true, #a, #b, __FILE__, __LINE__, false)
#define CHECK_NE(a, b)       mpctest::CheckEq((a), (b), false, #a, #b, __FILE__, __LINE__, false)
#define REQUIRE(expr)        mpctest::CheckTrue(!!(expr), "REQUIRE", #expr, __FILE__, __LINE__, true)
#define REQUIRE_EQ(a, b)     mpctest::CheckEq((a), (b), true, #a, #b, __FILE__, __LINE__, true)
#define FAIL(message)        mpctest::ReportFailure(__FILE__, __LINE__, (message))

#endif // MPCTEST_H

// Outside the include guard on purpose: the precompiled header has already
// included the declarations by the time main.cpp asks for the implementation.
#if defined(MPCTEST_MAIN) && !defined(MPCTEST_MAIN_INCLUDED)
#define MPCTEST_MAIN_INCLUDED

#include <crtdbg.h>
#include <DbgHelp.h>
#pragma comment(lib, "dbghelp.lib")

namespace mpctest
{
    static std::vector<Failure>* g_currentFailures = nullptr;
    static CStringW g_fixtureDir;
    static CStringW g_tempDir;
    static int g_messageBoxes = 0;
    static bool g_messageBoxExpected = false;

    std::vector<TestCase>& Registry()
    {
        static std::vector<TestCase> registry;
        return registry;
    }

    void ReportFailure(const char* file, int line, const std::string& message)
    {
        if (g_currentFailures) {
            g_currentFailures->push_back({ file, line, message });
        }
    }

    const CStringW& FixtureDir() { return g_fixtureDir; }
    const CStringW& TempDir() { return g_tempDir; }
    int MessageBoxCount() { return g_messageBoxes; }
    void ExpectMessageBox() { g_messageBoxExpected = true; }

    int OnMessageBox(const wchar_t* prompt)
    {
        g_messageBoxes++;
        if (!g_messageBoxExpected) {
            ReportFailure("", 0, "the code under test showed a message box: " + ToUtf8(prompt));
        }
        return IDOK;
    }

    static void InvalidParameterHandler(const wchar_t*, const wchar_t*, const wchar_t*, unsigned int, uintptr_t)
    {
        // The release CRT passes no detail. Without a handler this is a
        // fast-fail that ends the process; with one the CRT function returns
        // its error code and the test carries on, one failure richer.
        ReportFailure("", 0, "CRT invalid parameter (a secure CRT function was given a buffer too small for its input)");
    }

    static void RunCpp(void (*fn)())
    {
        try {
            fn();
        } catch (const AbortTest&) {
            // already recorded by the REQUIRE that threw
        } catch (const std::exception& e) {
            ReportFailure("", 0, std::string("unhandled C++ exception: ") + e.what());
        } catch (...) {
            // MFC throws CException*; leaking one in a failing test is harmless
            ReportFailure("", 0, "unhandled exception of unknown type (CException* or other)");
        }
    }

    // Where a structured exception came from, resolved through the PDB when
    // there is one: "parser crashed" is a result, "crashed in
    // CVobSubImage::GetNibble" is a lead.
    static std::string g_crashSite;

    static int DescribeCrash(EXCEPTION_POINTERS* ep)
    {
        if (ep->ExceptionRecord->ExceptionCode == EXCEPTION_STACK_OVERFLOW) {
            return EXCEPTION_EXECUTE_HANDLER; // no stack left to walk with
        }
        static bool symbolsReady = false;
        HANDLE process = GetCurrentProcess();
        if (!symbolsReady) {
            SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS);
            symbolsReady = !!SymInitialize(process, nullptr, TRUE);
        }

        CONTEXT ctx = *ep->ContextRecord;
        STACKFRAME64 frame = {};
        frame.AddrPC.Offset = ctx.Rip;
        frame.AddrFrame.Offset = ctx.Rbp;
        frame.AddrStack.Offset = ctx.Rsp;
        frame.AddrPC.Mode = frame.AddrFrame.Mode = frame.AddrStack.Mode = AddrModeFlat;

        g_crashSite.clear();
        for (int i = 0; i < 8; i++) {
            if (!StackWalk64(IMAGE_FILE_MACHINE_AMD64, process, GetCurrentThread(), &frame, &ctx, nullptr,
                             SymFunctionTableAccess64, SymGetModuleBase64, nullptr) || !frame.AddrPC.Offset) {
                break;
            }
            char symbolBuffer[sizeof(SYMBOL_INFO) + 512] = {};
            SYMBOL_INFO* symbol = (SYMBOL_INFO*)symbolBuffer;
            symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
            symbol->MaxNameLen = 511;
            DWORD64 displacement = 0;
            char line[768];
            if (symbolsReady && SymFromAddr(process, frame.AddrPC.Offset, &displacement, symbol)) {
                if (strstr(symbol->Name, "mpctest::Run")) {
                    break; // the harness itself: nothing of interest below
                }
                IMAGEHLP_LINE64 lineInfo = { sizeof(lineInfo) };
                DWORD lineDisplacement = 0;
                if (SymGetLineFromAddr64(process, frame.AddrPC.Offset, &lineDisplacement, &lineInfo)) {
                    sprintf_s(line, "\n      at %s (%s:%lu)", symbol->Name, lineInfo.FileName, lineInfo.LineNumber);
                } else {
                    sprintf_s(line, "\n      at %s", symbol->Name);
                }
            } else {
                sprintf_s(line, "\n      at 0x%llx", (unsigned long long)frame.AddrPC.Offset);
            }
            g_crashSite += line;
        }
        return EXCEPTION_EXECUTE_HANDLER;
    }

    static DWORD RunGuarded(void (*fn)())
    {
        __try {
            RunCpp(fn);
        } __except (DescribeCrash(GetExceptionInformation())) {
            return GetExceptionCode();
        }
        return 0;
    }

    static std::string SehText(DWORD code)
    {
        char buf[96];
        sprintf_s(buf, "crashed: structured exception 0x%08lx%s", code,
                  code == EXCEPTION_ACCESS_VIOLATION ? " (access violation)" :
                  code == EXCEPTION_STACK_OVERFLOW ? " (stack overflow)" : "");
        return buf + g_crashSite;
    }

    static void RunInProcess(const TestCase& t, std::vector<Failure>& failures)
    {
        g_currentFailures = &failures;
        g_messageBoxes = 0;
        g_messageBoxExpected = false;
        DWORD seh = RunGuarded(t.fn);
        if (seh) {
            failures.push_back({ "", 0, SehText(seh) });
        }
        g_currentFailures = nullptr;
    }

    // One failure per line, tab-separated, newlines and tabs escaped: the
    // only thing a child has to hand back to its parent.
    static std::string EscapeLine(const std::string& s)
    {
        std::string out;
        for (char c : s) {
            if (c == '\n') {
                out += "\\n";
            } else if (c == '\t') {
                out += "\\t";
            } else if (c == '\r') {
                // dropped
            } else if (c == '\\') {
                out += "\\\\";
            } else {
                out += c;
            }
        }
        return out;
    }

    static std::string UnescapeLine(const std::string& s)
    {
        std::string out;
        for (size_t i = 0; i < s.size(); i++) {
            if (s[i] == '\\' && i + 1 < s.size()) {
                i++;
                out += s[i] == 'n' ? '\n' : s[i] == 't' ? '\t' : s[i];
            } else {
                out += s[i];
            }
        }
        return out;
    }

    static void RunInChild(const TestCase& t, std::vector<Failure>& failures, DWORD timeoutMs)
    {
        WCHAR exe[MAX_PATH * 4] = {};
        GetModuleFileNameW(nullptr, exe, _countof(exe));

        CStringW report;
        report.Format(L"%schild-%S.txt", (LPCWSTR)g_tempDir, t.name);
        DeleteFileW(report);

        CStringW cmd;
        cmd.Format(L"\"%s\" --child %S --child-report \"%s\" --fixtures \"%s\\.\" --temp \"%s\\.\"",
                   exe, t.name, (LPCWSTR)report, (LPCWSTR)g_fixtureDir, (LPCWSTR)g_tempDir);

        STARTUPINFOW si = { sizeof(si) };
        PROCESS_INFORMATION pi = {};
        if (!CreateProcessW(exe, cmd.GetBuffer(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) {
            failures.push_back({ "", 0, "cannot start the child process" });
            return;
        }

        DWORD exitCode = 0;
        if (WaitForSingleObject(pi.hProcess, timeoutMs) == WAIT_TIMEOUT) {
            TerminateProcess(pi.hProcess, 3);
            WaitForSingleObject(pi.hProcess, 5000);
            char buf[96];
            sprintf_s(buf, "hung: no result after %lu s, child process killed", timeoutMs / 1000);
            failures.push_back({ "", 0, buf });
        } else {
            GetExitCodeProcess(pi.hProcess, &exitCode);
        }
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);

        FILE* f = nullptr;
        if (_wfopen_s(&f, report, L"rb") == 0 && f) {
            char line[8192];
            while (fgets(line, sizeof(line), f)) {
                std::string s(line);
                while (!s.empty() && (s.back() == '\n' || s.back() == '\r')) {
                    s.pop_back();
                }
                size_t t1 = s.find('\t');
                size_t t2 = t1 == std::string::npos ? t1 : s.find('\t', t1 + 1);
                if (t2 != std::string::npos) {
                    failures.push_back({ s.substr(0, t1), atoi(s.substr(t1 + 1, t2 - t1 - 1).c_str()), UnescapeLine(s.substr(t2 + 1)) });
                }
            }
            fclose(f);
        }
        if (exitCode > 1) {
            char buf[128];
            sprintf_s(buf, "crashed: child process ended with exit code 0x%08lx", exitCode);
            failures.push_back({ "", 0, buf });
        } else if (exitCode == 1 && failures.empty()) {
            failures.push_back({ "", 0, "child process reported a failure but left no detail" });
        }
    }

    static int ChildMain(const std::string& name, const CStringW& report)
    {
        // A crash must end the process quietly, not raise a dialog.
        SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
        _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);

        for (const auto& t : Registry()) {
            if (name == t.name) {
                std::vector<Failure> failures;
                RunInProcess(t, failures);
                FILE* f = nullptr;
                if (_wfopen_s(&f, report, L"wb") == 0 && f) {
                    for (const auto& fl : failures) {
                        fprintf(f, "%s\t%d\t%s\n", fl.file.c_str(), fl.line, EscapeLine(fl.message).c_str());
                    }
                    fclose(f);
                }
                return failures.empty() ? 0 : 1;
            }
        }
        return 2;
    }

    static std::string JsonEscape(const std::string& s)
    {
        std::string out;
        char buf[8];
        for (unsigned char c : s) {
            if (c == '"' || c == '\\') {
                out += '\\';
                out += (char)c;
            } else if (c == '\n') {
                out += "\\n";
            } else if (c < 0x20) {
                sprintf_s(buf, "\\u%04x", (unsigned)c);
                out += buf;
            } else {
                out += (char)c;
            }
        }
        return out;
    }

    static bool Matches(const char* name, const std::vector<std::string>& filters)
    {
        if (filters.empty()) {
            return true;
        }
        std::string n(name);
        for (auto& c : n) {
            c = (char)tolower((unsigned char)c);
        }
        for (const auto& f : filters) {
            if (n.find(f) != std::string::npos) {
                return true;
            }
        }
        return false;
    }

    static CStringW WithBackslash(CStringW dir)
    {
        // "dir\." is how a path ending in a backslash survives command-line quoting
        if (dir.Right(2) == L"\\.") {
            dir.Truncate(dir.GetLength() - 1);
        }
        if (dir.Right(1) != L"\\") {
            dir += L"\\";
        }
        return dir;
    }

    static CStringW FindFixtureDir()
    {
        // The exe lives under <repo>\bin\...; walk up until tests\unit\fixtures appears.
        WCHAR path[MAX_PATH * 4] = {};
        GetModuleFileNameW(nullptr, path, _countof(path));
        CStringW dir(path);
        for (int i = 0; i < 8; i++) {
            int slash = dir.ReverseFind(L'\\');
            if (slash <= 0) {
                break;
            }
            dir.Truncate(slash);
            CStringW candidate = dir + L"\\tests\\unit\\fixtures\\";
            if (GetFileAttributesW(candidate) != INVALID_FILE_ATTRIBUTES) {
                return candidate;
            }
        }
        return L"";
    }

    static void RemoveTree(const CStringW& dir)
    {
        WIN32_FIND_DATAW fd;
        HANDLE h = FindFirstFileW(dir + L"*", &fd);
        if (h != INVALID_HANDLE_VALUE) {
            do {
                if (!wcscmp(fd.cFileName, L".") || !wcscmp(fd.cFileName, L"..")) {
                    continue;
                }
                if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                    RemoveTree(dir + fd.cFileName + L"\\");
                } else {
                    SetFileAttributesW(dir + fd.cFileName, FILE_ATTRIBUTE_NORMAL);
                    DeleteFileW(dir + fd.cFileName);
                }
            } while (FindNextFileW(h, &fd));
            FindClose(h);
        }
        RemoveDirectoryW(dir);
    }

    int Main(int argc, wchar_t** argv)
    {
        bool list = false;
        bool keepTemp = false;
        DWORD timeoutMs = 30000;
        std::string jsonPath, childName;
        CStringW childReport;
        std::vector<std::string> filters;

        for (int i = 1; i < argc; i++) {
            std::wstring a = argv[i];
            if (a == L"--list") {
                list = true;
            } else if (a == L"--keep-temp") {
                keepTemp = true;
            } else if (a == L"--json" && i + 1 < argc) {
                jsonPath = ToUtf8(argv[++i]);
            } else if (a == L"--fixtures" && i + 1 < argc) {
                g_fixtureDir = WithBackslash(argv[++i]);
            } else if (a == L"--timeout" && i + 1 < argc) {
                timeoutMs = (DWORD)_wtoi(argv[++i]) * 1000;
            } else if (a == L"--temp" && i + 1 < argc) { // internal
                g_tempDir = WithBackslash(argv[++i]);
            } else if (a == L"--child" && i + 1 < argc) { // internal
                childName = ToUtf8(argv[++i]);
            } else if (a == L"--child-report" && i + 1 < argc) { // internal
                childReport = argv[++i];
            } else if (a == L"--help" || a == L"-h" || a == L"/?") {
                printf("usage: MpcUnitTests [--list] [--json <file>] [--fixtures <dir>] [--timeout <seconds>] [--keep-temp] [filter ...]\n"
                       "  filter     run only tests whose name contains the text (case-insensitive); several are OR-ed\n"
                       "  --timeout  limit for a test that runs in a child process (default 30)\n"
                       "exit code: 0 all passed, 1 failures or unexpected passes, 2 usage or setup error\n");
                return 0;
            } else if (a.size() > 1 && a[0] == L'-' && a[1] == L'-') {
                fprintf(stderr, "unknown option %s\n", ToUtf8(a.c_str()).c_str());
                return 2;
            } else {
                std::string f = ToUtf8(a.c_str());
                for (auto& c : f) {
                    c = (char)tolower((unsigned char)c);
                }
                filters.push_back(f);
            }
        }

        auto& tests = Registry();
        std::sort(tests.begin(), tests.end(), [](const TestCase & a, const TestCase & b) { return strcmp(a.name, b.name) < 0; });

        if (list) {
            for (const auto& t : tests) {
                if (Matches(t.name, filters)) {
                    printf("%s%s%s\n", t.name, t.isolated ? "  [isolated]" : "", t.expectedFailure ? "  [expected failure]" : "");
                }
            }
            return 0;
        }

        if (g_fixtureDir.IsEmpty()) {
            g_fixtureDir = FindFixtureDir();
        }
        if (g_fixtureDir.IsEmpty() || GetFileAttributesW(g_fixtureDir) == INVALID_FILE_ATTRIBUTES) {
            fprintf(stderr, "fixture directory not found; pass --fixtures <dir>\n");
            return 2;
        }

        _set_invalid_parameter_handler(InvalidParameterHandler);
        _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
        _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);

        if (!childName.empty()) {
            return ChildMain(childName, childReport);
        }

        WCHAR tmp[MAX_PATH] = {};
        GetTempPathW(_countof(tmp), tmp);
        g_tempDir.Format(L"%sMpcUnitTests-%lu\\", tmp, GetCurrentProcessId());
        CreateDirectoryW(g_tempDir, nullptr);

        struct Result {
            const TestCase* test;
            std::string status; // passed | failed | expected-failure | unexpected-pass
            double ms;
            std::vector<Failure> failures;
        };
        std::vector<Result> results;
        int passed = 0, failed = 0, expectedFailures = 0, unexpectedPasses = 0;

        const auto runStart = std::chrono::steady_clock::now();
        for (const auto& t : tests) {
            if (!Matches(t.name, filters)) {
                continue;
            }
            Result r{ &t };
            const auto start = std::chrono::steady_clock::now();
            if (t.isolated) {
                RunInChild(t, r.failures, timeoutMs);
            } else {
                RunInProcess(t, r.failures);
            }
            r.ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();

            const bool ok = r.failures.empty();
            if (t.expectedFailure) {
                r.status = ok ? "unexpected-pass" : "expected-failure";
                ok ? unexpectedPasses++ : expectedFailures++;
            } else {
                r.status = ok ? "passed" : "failed";
                ok ? passed++ : failed++;
            }

            const char* label = r.status == "passed" ? "PASS " : r.status == "failed" ? "FAIL " : r.status == "expected-failure" ? "XFAIL" : "XPASS";
            printf("[%s] %s (%.1f ms)\n", label, t.name, r.ms);
            if (t.expectedFailure && !ok) {
                printf("    expected: %s\n", t.expectedFailure);
            }
            if (r.status == "unexpected-pass") {
                printf("    marked as an expected failure but passed -- remove the marker:\n    %s\n", t.expectedFailure);
            }
            for (const auto& f : r.failures) {
                if (f.line) {
                    printf("    %s(%d): %s\n", f.file.c_str(), f.line, f.message.c_str());
                } else {
                    printf("    %s\n", f.message.c_str());
                }
            }
            fflush(stdout);
            results.push_back(std::move(r));
        }
        const double totalMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - runStart).count();

        printf("\n%d run: %d passed, %d failed, %d expected failure(s), %d unexpected pass(es) in %.0f ms\n",
               (int)results.size(), passed, failed, expectedFailures, unexpectedPasses, totalMs);

        if (!jsonPath.empty()) {
            FILE* f = nullptr;
            CStringW wpath(CA2W(jsonPath.c_str(), CP_UTF8));
            if (_wfopen_s(&f, wpath, L"wb") == 0 && f) {
                fprintf(f, "{\n  \"total\": %d,\n  \"passed\": %d,\n  \"failed\": %d,\n  \"expectedFailures\": %d,\n  \"unexpectedPasses\": %d,\n  \"durationMs\": %.1f,\n  \"tests\": [\n",
                        (int)results.size(), passed, failed, expectedFailures, unexpectedPasses, totalMs);
                for (size_t i = 0; i < results.size(); i++) {
                    const auto& r = results[i];
                    fprintf(f, "    { \"name\": \"%s\", \"status\": \"%s\", \"isolated\": %s, \"durationMs\": %.2f, \"file\": \"%s\", \"line\": %d",
                            r.test->name, r.status.c_str(), r.test->isolated ? "true" : "false", r.ms, JsonEscape(r.test->file).c_str(), r.test->line);
                    if (r.test->expectedFailure) {
                        fprintf(f, ", \"expectedFailure\": \"%s\"", JsonEscape(r.test->expectedFailure).c_str());
                    }
                    fprintf(f, ", \"failures\": [");
                    for (size_t j = 0; j < r.failures.size(); j++) {
                        const auto& fl = r.failures[j];
                        fprintf(f, "%s{ \"file\": \"%s\", \"line\": %d, \"message\": \"%s\" }", j ? ", " : "",
                                JsonEscape(fl.file).c_str(), fl.line, JsonEscape(fl.message).c_str());
                    }
                    fprintf(f, "] }%s\n", i + 1 < results.size() ? "," : "");
                }
                fprintf(f, "  ]\n}\n");
                fclose(f);
            } else {
                fprintf(stderr, "cannot write %s\n", jsonPath.c_str());
                return 2;
            }
        }

        if (!keepTemp) {
            RemoveTree(g_tempDir);
        }
        if (results.empty()) {
            fprintf(stderr, "no test matched\n");
            return 2;
        }
        return (failed || unexpectedPasses) ? 1 : 0;
    }
}

#endif // MPCTEST_MAIN
