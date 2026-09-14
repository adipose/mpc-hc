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
#include "DragRectWnd.h"
#include "mplayerc.h"
#include "CMPCTheme.h"

CDragRectWnd::~CDragRectWnd()
{
    if (m_hWnd) {
        DestroyWindow();
    }
}

BEGIN_MESSAGE_MAP(CDragRectWnd, CWnd)
    ON_WM_ERASEBKGND()
END_MESSAGE_MAP()

bool CDragRectWnd::Create()
{
    // no owner or parent: a parent would clip the frame to its client area
    return !!CreateEx(WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE,
                      AfxRegisterWndClass(0), nullptr, WS_POPUP, CRect(0, 0, 0, 0), nullptr, 0);
}

void CDragRectWnd::Show(const CRect& rcScreen, const CSize& border, bool bDocked)
{
    if (!m_hWnd && !Create()) {
        return;
    }

    // only the frame is visible: window rect minus the rect deflated by the border
    CRect rcOuter(0, 0, rcScreen.Width(), rcScreen.Height());
    CRect rcInner(rcOuter);
    rcInner.DeflateRect(border.cx, border.cy);

    CRgn rgnOuter, rgnInner;
    rgnOuter.CreateRectRgnIndirect(rcOuter);
    rgnInner.CreateRectRgnIndirect(rcInner);
    rgnOuter.CombineRgn(&rgnOuter, &rgnInner, RGN_DIFF);
    // the system owns the region after SetWindowRgn
    SetWindowRgn((HRGN)rgnOuter.Detach(), FALSE);

    // the docked preview is a one pixel line, keep it opaque
    SetLayeredWindowAttributes(0, bDocked ? 255 : 160, LWA_ALPHA);
    SetWindowPos(nullptr, rcScreen.left, rcScreen.top, rcScreen.Width(), rcScreen.Height(),
                 SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_SHOWWINDOW);
    RedrawWindow(nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_UPDATENOW);
}

void CDragRectWnd::Hide()
{
    if (m_hWnd) {
        ShowWindow(SW_HIDE);
    }
}

BOOL CDragRectWnd::OnEraseBkgnd(CDC* pDC)
{
    CRect rc;
    GetClientRect(rc);
    pDC->FillSolidRect(rc, AppIsThemeLoaded() ? CMPCTheme::ContentSelectedColor : GetSysColor(COLOR_HIGHLIGHT));
    return TRUE;
}
