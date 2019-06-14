#include "stdafx.h"
#include "CDarkSpinButtonCtrl.h"
#include "CDarkTheme.h"
#include "CDarkEdit.h"

CDarkSpinButtonCtrl::CDarkSpinButtonCtrl() {
}


CDarkSpinButtonCtrl::~CDarkSpinButtonCtrl() {
}

IMPLEMENT_DYNAMIC(CDarkSpinButtonCtrl, CSpinButtonCtrl)
BEGIN_MESSAGE_MAP(CDarkSpinButtonCtrl, CSpinButtonCtrl)
    ON_WM_PAINT()
    ON_WM_MOUSEMOVE()
    ON_WM_LBUTTONDOWN()
    ON_WM_LBUTTONUP()
    ON_WM_ERASEBKGND()
END_MESSAGE_MAP()


void CDarkSpinButtonCtrl::OnPaint() {
    CWnd *buddy = GetBuddy();
    CDarkEdit* buddyEdit;
    if (nullptr != buddy && nullptr != (buddyEdit = DYNAMIC_DOWNCAST(CDarkEdit, buddy))) {
        buddyEdit->setBuddy(this); //we need to know about the buddy spin ctrl to clip it in ncpaint :-/
    }

    CPaintDC dc(this);
    CRect   rectItem;
    GetClientRect(rectItem);

    COLORREF oldBkColor = dc.GetBkColor();
    COLORREF oldTextColor = dc.GetTextColor();

    COLORREF bgClr = CDarkTheme::ContentBGColor;

    CBrush borderBrush(CDarkTheme::EditBorderColor);
    CBrush butBorderBrush(CDarkTheme::ButtonBorderOuterColor);
    dc.FillSolidRect(rectItem, bgClr);
    dc.ExcludeClipRect(0, 1, 1, rectItem.Height()-1); //don't get left edge of rect
    dc.FrameRect(rectItem, &borderBrush);

    CBitmap arrowBMP;
    CDC dcArrowBMP;
    dcArrowBMP.CreateCompatibleDC(&dc);
    int arrowLeft, arrowTop, arrowWidth, arrowHeight;

    arrowWidth = CDarkTheme::SpinnerArrowWidth;
    arrowHeight = CDarkTheme::SpinnerArrowHeight;
    arrowLeft = rectItem.left + (rectItem.Width() - arrowWidth) / 2;
    arrowBMP.CreateBitmap(arrowWidth, arrowHeight, 1, 1, CDarkTheme::SpinnerArrowBits);

    for (int a = 0; a < 2; a++) {
        CRect butRect = rectItem;
        butRect.DeflateRect(1, 1, 2, 1);
        if (0 == a) {
            butRect.bottom -= butRect.Height() / 2;
        } else {
            butRect.top += butRect.Height() / 2;
        }
        butRect.DeflateRect(0, 1);

        if (butRect.PtInRect(downPos)) {
            bgClr = CDarkTheme::ButtonFillSelectedColor;
        } else {
            bgClr = CDarkTheme::ButtonFillColor;
        }

        arrowTop = butRect.top + (butRect.Height() - arrowHeight) / 2;
        dcArrowBMP.SelectObject(&arrowBMP);

        dc.FillSolidRect(butRect, bgClr);
        dc.FrameRect(butRect, &borderBrush);
        dc.SetBkColor(CDarkTheme::ScrollButtonArrowColor);
        dc.SetTextColor(bgClr);

        if (0 == a) { //top
            dc.BitBlt(arrowLeft, arrowTop, arrowWidth, arrowHeight, &dcArrowBMP, 0, 0, SRCCOPY);
        } else {
            dc.StretchBlt(arrowLeft, arrowTop, arrowWidth, arrowHeight, &dcArrowBMP, 0, arrowHeight - 1, arrowWidth, -arrowHeight, SRCCOPY);
        }
    }
       

    dc.SetBkColor(oldBkColor);
    dc.SetTextColor(oldTextColor);

}


void CDarkSpinButtonCtrl::OnMouseMove(UINT nFlags, CPoint point) {
    CSpinButtonCtrl::OnMouseMove(nFlags, point);
    if (MK_LBUTTON & nFlags) {
        downPos = point;
    } else {
        downPos = CPoint(-1, -1);
    }
}


void CDarkSpinButtonCtrl::OnLButtonDown(UINT nFlags, CPoint point) {
    CSpinButtonCtrl::OnLButtonDown(nFlags, point);
    downPos = point;
}


void CDarkSpinButtonCtrl::OnLButtonUp(UINT nFlags, CPoint point) {
    CSpinButtonCtrl::OnLButtonUp(nFlags, point);
    downPos = CPoint(-1, -1);
}


BOOL CDarkSpinButtonCtrl::OnEraseBkgnd(CDC* pDC) {
    return TRUE;
}
