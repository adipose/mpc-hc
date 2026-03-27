#pragma once

// Header-only mouse/scroll trace for debug builds.
//
// Drop this file into the project and add two lines to MainFrm.cpp:
//   #include "DebugScrollTrace.h"
//   OnCreate:  DBG_InstallScrollTrace();
//   OnDestroy: DBG_UninstallScrollTrace();
//
// Add #include "DebugScrollTrace.h" to any other file that calls DBG_LogTrace.
// No .cpp file or project file changes required.
//
// Output: %TEMP%\MPC-HC_ScrollTrace.log

#include "version.h"
#include <stdio.h>
#include <time.h>
#include <stdarg.h>

// ---- shared state via function-local statics (C++11, no ODR issues) --------

static inline FILE*& DBG_LogFile()   { static FILE* p = nullptr; return p; }
static inline HHOOK& DBG_GetMsgHook()  { static HHOOK h = nullptr; return h; }
static inline HHOOK& DBG_CallWndHook() { static HHOOK h = nullptr; return h; }

// ---- internal helpers -------------------------------------------------------

static inline void DBG_OpenLog()
{
    if (DBG_LogFile()) return;

    TCHAR szTemp[MAX_PATH];
    GetTempPath(MAX_PATH, szTemp);

    char szPath[MAX_PATH];
    WideCharToMultiByte(CP_ACP, 0, szTemp, -1, szPath, MAX_PATH, nullptr, nullptr);
    strncat_s(szPath, "MPC-HC_ScrollTrace.log", _TRUNCATE);

    fopen_s(&DBG_LogFile(), szPath, "a");
    if (DBG_LogFile()) {
        time_t t = time(nullptr);
        char tbuf[64];
        ctime_s(tbuf, sizeof(tbuf), &t);
        tbuf[strcspn(tbuf, "\n")] = '\0';
        fprintf(DBG_LogFile(), "\n=== MPC-HC mouse trace started: %s ===\n", tbuf);
        fprintf(DBG_LogFile(), "    version: %ls\n", MPC_VERSION_STR_FULL);
        fflush(DBG_LogFile());
    }
}

static inline void DBG_CloseLog()
{
    if (DBG_LogFile()) {
        fprintf(DBG_LogFile(), "=== trace stopped ===\n");
        fclose(DBG_LogFile());
        DBG_LogFile() = nullptr;
    }
}

static inline void DBG_GetClassName8(HWND hwnd, char* buf, int len)
{
    TCHAR wbuf[128] = {};
    if (hwnd) GetClassName(hwnd, wbuf, 128);
    WideCharToMultiByte(CP_ACP, 0, wbuf, -1, buf, len, nullptr, nullptr);
}

static inline const char* DBG_MsgName(UINT msg)
{
    switch (msg) {
        case WM_MOUSEWHEEL:     return "WM_MOUSEWHEEL";
        case WM_MOUSEHWHEEL:    return "WM_MOUSEHWHEEL";
        case WM_VSCROLL:        return "WM_VSCROLL";
        case WM_HSCROLL:        return "WM_HSCROLL";
        case WM_LBUTTONDOWN:    return "WM_LBUTTONDOWN";
        case WM_LBUTTONUP:      return "WM_LBUTTONUP";
        case WM_LBUTTONDBLCLK:  return "WM_LBUTTONDBLCLK";
        case WM_RBUTTONDOWN:    return "WM_RBUTTONDOWN";
        case WM_RBUTTONUP:      return "WM_RBUTTONUP";
        case WM_RBUTTONDBLCLK:  return "WM_RBUTTONDBLCLK";
        case WM_MBUTTONDOWN:    return "WM_MBUTTONDOWN";
        case WM_MBUTTONUP:      return "WM_MBUTTONUP";
        case WM_MBUTTONDBLCLK:  return "WM_MBUTTONDBLCLK";
        case WM_XBUTTONDOWN:    return "WM_XBUTTONDOWN";
        case WM_XBUTTONUP:      return "WM_XBUTTONUP";
        case WM_XBUTTONDBLCLK:  return "WM_XBUTTONDBLCLK";
        default:                return "?";
    }
}

static inline bool DBG_IsMouseMsg(UINT msg)
{
    switch (msg) {
        case WM_MOUSEWHEEL:   case WM_MOUSEHWHEEL:
        case WM_VSCROLL:      case WM_HSCROLL:
        case WM_LBUTTONDOWN:  case WM_LBUTTONUP:   case WM_LBUTTONDBLCLK:
        case WM_RBUTTONDOWN:  case WM_RBUTTONUP:   case WM_RBUTTONDBLCLK:
        case WM_MBUTTONDOWN:  case WM_MBUTTONUP:   case WM_MBUTTONDBLCLK:
        case WM_XBUTTONDOWN:  case WM_XBUTTONUP:   case WM_XBUTTONDBLCLK:
            return true;
        default:
            return false;
    }
}

static inline POINT DBG_ExtractPoint(HWND hwnd, UINT msg, LPARAM lParam)
{
    POINT pt;
    switch (msg) {
        case WM_MOUSEWHEEL:
        case WM_MOUSEHWHEEL:
            pt.x = GET_X_LPARAM(lParam);
            pt.y = GET_Y_LPARAM(lParam);
            break;
        case WM_VSCROLL:
        case WM_HSCROLL:
            GetCursorPos(&pt);
            break;
        default:
            pt.x = GET_X_LPARAM(lParam);
            pt.y = GET_Y_LPARAM(lParam);
            if (hwnd) ClientToScreen(hwnd, &pt);
            break;
    }
    return pt;
}

inline void DBG_LogTrace(const char* fmt, ...)
{
    if (!DBG_LogFile()) return;
    va_list args;
    va_start(args, fmt);
    vfprintf(DBG_LogFile(), fmt, args);
    va_end(args);
    fputc('\n', DBG_LogFile());
    fflush(DBG_LogFile());
}

static inline void DBG_LogMouseEvent(const char* hookType, HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (!DBG_LogFile()) return;

    char destClass[128] = {};
    DBG_GetClassName8(hwnd, destClass, sizeof(destClass));

    POINT pt = DBG_ExtractPoint(hwnd, msg, lParam);

    HWND hwndUnder = WindowFromPoint(pt);
    char underClass[128] = {};
    DBG_GetClassName8(hwndUnder, underClass, sizeof(underClass));

    if (msg == WM_MOUSEWHEEL || msg == WM_MOUSEHWHEEL) {
        short delta = GET_WHEEL_DELTA_WPARAM(wParam);
        WORD keys   = GET_KEYSTATE_WPARAM(wParam);
        fprintf(DBG_LogFile(), "[%s] %s  hwnd=0x%p (%s)  delta=%+d  keys=0x%04X  screen=(%d,%d)  under=(%s)\n",
                hookType, DBG_MsgName(msg), (void*)hwnd, destClass,
                (int)delta, (unsigned)keys, pt.x, pt.y, underClass);
    } else if (msg == WM_VSCROLL || msg == WM_HSCROLL) {
        WORD code = LOWORD(wParam);
        fprintf(DBG_LogFile(), "[%s] %s  hwnd=0x%p (%s)  code=%u  cursor=(%d,%d)  under=(%s)\n",
                hookType, DBG_MsgName(msg), (void*)hwnd, destClass,
                (unsigned)code, pt.x, pt.y, underClass);
    } else {
        char extra[32] = {};
        if (msg == WM_XBUTTONDOWN || msg == WM_XBUTTONUP || msg == WM_XBUTTONDBLCLK) {
            sprintf_s(extra, "  xbtn=%u", (unsigned)GET_XBUTTON_WPARAM(wParam));
        }
        fprintf(DBG_LogFile(), "[%s] %s  hwnd=0x%p (%s)  screen=(%d,%d)  under=(%s)%s\n",
                hookType, DBG_MsgName(msg), (void*)hwnd, destClass,
                pt.x, pt.y, underClass, extra);
    }
    fflush(DBG_LogFile());
}

static LRESULT CALLBACK DBG_GetMsgProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode == HC_ACTION) {
        const MSG* pMsg = reinterpret_cast<const MSG*>(lParam);
        if (DBG_IsMouseMsg(pMsg->message))
            DBG_LogMouseEvent("GetMsg", pMsg->hwnd, pMsg->message, pMsg->wParam, pMsg->lParam);
    }
    return CallNextHookEx(DBG_GetMsgHook(), nCode, wParam, lParam);
}

static LRESULT CALLBACK DBG_CallWndProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode == HC_ACTION) {
        const CWPSTRUCT* pCwp = reinterpret_cast<const CWPSTRUCT*>(lParam);
        if (DBG_IsMouseMsg(pCwp->message))
            DBG_LogMouseEvent("CallWnd", pCwp->hwnd, pCwp->message, pCwp->wParam, pCwp->lParam);
    }
    return CallNextHookEx(DBG_CallWndHook(), nCode, wParam, lParam);
}

// ---- public API -------------------------------------------------------------

inline void DBG_InstallScrollTrace()
{
    DBG_OpenLog();
    HINSTANCE hInst = AfxGetInstanceHandle();
    DWORD tid = GetCurrentThreadId();
    DBG_GetMsgHook()  = SetWindowsHookEx(WH_GETMESSAGE,  DBG_GetMsgProc,  hInst, tid);
    DBG_CallWndHook() = SetWindowsHookEx(WH_CALLWNDPROC, DBG_CallWndProc, hInst, tid);
    if (DBG_LogFile()) {
        fprintf(DBG_LogFile(), "Hooks installed: GetMsg=%p  CallWnd=%p  tid=%lu\n",
                (void*)DBG_GetMsgHook(), (void*)DBG_CallWndHook(), (unsigned long)tid);
        fflush(DBG_LogFile());
    }
}

inline void DBG_UninstallScrollTrace()
{
    if (DBG_GetMsgHook())  { UnhookWindowsHookEx(DBG_GetMsgHook());  DBG_GetMsgHook()  = nullptr; }
    if (DBG_CallWndHook()) { UnhookWindowsHookEx(DBG_CallWndHook()); DBG_CallWndHook() = nullptr; }
    DBG_CloseLog();
}
