#include "stdafx.h"
#include "CDarkStatic.h"
#include "CDarkTheme.h"
#include "CDarkChildHelper.h"

CDarkStatic::CDarkStatic() {
    isFileDialogChild = false;
}


CDarkStatic::~CDarkStatic() {
}
IMPLEMENT_DYNAMIC(CDarkStatic, CStatic)
BEGIN_MESSAGE_MAP(CDarkStatic, CStatic)
    ON_WM_PAINT()
    ON_WM_NCPAINT()
    ON_WM_ENABLE()
    ON_WM_ERASEBKGND()
END_MESSAGE_MAP()


void CDarkStatic::OnPaint() {
    if (AfxGetAppSettings().bDarkThemeLoaded) {
        CPaintDC dc(this);

        CString sTitle;
        GetWindowText(sTitle);
        CRect rectItem;
        GetClientRect(rectItem);
        dc.SetBkMode(TRANSPARENT);

        COLORREF oldBkColor = dc.GetBkColor();
        COLORREF oldTextColor = dc.GetTextColor();

        bool isDisabled = !IsWindowEnabled();
        UINT style = GetStyle();

        if (!sTitle.IsEmpty()) {
            CRect centerRect = rectItem;
            CFont font;
            CDarkTheme::getFontByType(font, &dc, CDarkTheme::CDDialogFont);
            CFont* pOldFont = dc.SelectObject(&font);

            UINT uFormat = 0;
            if (style & SS_LEFTNOWORDWRAP) {
                uFormat |= DT_SINGLELINE;
            } else {
                uFormat |= DT_WORDBREAK;
            }

            if (style & BS_VCENTER) {
                uFormat |= DT_VCENTER;
            }

            if ((style & SS_CENTER) == SS_CENTER) {
                uFormat |= DT_CENTER;
                dc.DrawText(sTitle, -1, &rectItem, uFormat | DT_CALCRECT);
                rectItem.OffsetRect((centerRect.Width() - rectItem.Width()) / 2,
                    (centerRect.Height() - rectItem.Height()) / 2);
            } else if ((style & SS_RIGHT) == SS_RIGHT) {
                uFormat |= DT_RIGHT;
                dc.DrawText(sTitle, -1, &rectItem, uFormat | DT_CALCRECT);
                rectItem.OffsetRect(centerRect.Width() - rectItem.Width(),
                    (centerRect.Height() - rectItem.Height()) / 2);
            } else { // if ((style & SS_LEFT) == SS_LEFT || (style & SS_LEFTNOWORDWRAP) == SS_LEFTNOWORDWRAP) {
                uFormat |= DT_LEFT;
                dc.DrawText(sTitle, -1, &rectItem, uFormat | DT_CALCRECT);
                rectItem.OffsetRect(0, (centerRect.Height() - rectItem.Height()) / 2);
            }

            dc.SetBkColor(CDarkTheme::WindowBGColor);
            if (isDisabled) {
                dc.SetTextColor(CDarkTheme::ButtonDisabledFGColor);
                dc.DrawText(sTitle, -1, &rectItem, uFormat);
            } else {
                dc.SetTextColor(CDarkTheme::TextFGColor);
                dc.DrawText(sTitle, -1, &rectItem, uFormat);
            }
            dc.SelectObject(pOldFont);
            dc.SetBkColor(oldBkColor);
            dc.SetTextColor(oldTextColor);
        }
    } else {
        __super::OnPaint();
    }
}


void CDarkStatic::OnNcPaint() {
    if (AfxGetAppSettings().bDarkThemeLoaded) {
        CDC* pDC = GetWindowDC();

        CRect rect;
        GetWindowRect(&rect);
        rect.OffsetRect(-rect.left, -rect.top);
        DWORD type = GetStyle() & SS_TYPEMASK;


        if (SS_ETCHEDHORZ == type || SS_ETCHEDVERT == type) { //etched lines assumed
            rect.DeflateRect(0, 0, 1, 1); //make it thinner
            CBrush brush(CDarkTheme::StaticEtchedColor);
            pDC->FillSolidRect(rect, CDarkTheme::StaticEtchedColor);
        } else if (SS_ETCHEDFRAME == type) { //etched border
            CBrush brush(CDarkTheme::StaticEtchedColor);
            pDC->FrameRect(rect, &brush);
        } else { //not supported yet
        }
    } else {
        CStatic::OnNcPaint();
    }
}

void CDarkStatic::OnEnable(BOOL bEnable) {
    if (AfxGetAppSettings().bDarkThemeLoaded) {
        SetRedraw(FALSE);
        __super::OnEnable(bEnable);
        SetRedraw(TRUE);
        CWnd *parent = GetParent();
        if (nullptr != parent) {
            CRect wr;
            GetWindowRect(wr);
            parent->ScreenToClient(wr);
            parent->InvalidateRect(wr, TRUE);
        } else {
            Invalidate();
        }
    } else {
        __super::OnEnable(bEnable);
    }
}

BOOL CDarkStatic::OnEraseBkgnd(CDC* pDC) {
    if (AfxGetAppSettings().bDarkThemeLoaded) {
        if (isFileDialogChild) {
            CRect r;
            GetClientRect(r);
            HBRUSH hBrush=CDarkChildHelper::DarkCtlColorFileDialog(pDC->GetSafeHdc(), CTLCOLOR_STATIC);
            ::FillRect(pDC->GetSafeHdc(), r, hBrush);
        }
        return TRUE;
    } else {
        return CStatic::OnEraseBkgnd(pDC);
    }
}
