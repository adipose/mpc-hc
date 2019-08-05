#include "stdafx.h"
#include "CMPCThemeToolTipCtrl.h"
#include "CMPCTheme.h"
#include "CMPCThemeUtil.h"
#include <afxglobals.h>


CMPCThemeToolTipCtrl::CMPCThemeToolTipCtrl() {
    this->useFlickerHelper = false;
    this->helper = nullptr;
}


CMPCThemeToolTipCtrl::~CMPCThemeToolTipCtrl() {
    if (nullptr != helper) {
        helper->DestroyWindow();
        delete helper;
    }
}

void CMPCThemeToolTipCtrl::enableFlickerHelper() {
    this->useFlickerHelper = true;
}

IMPLEMENT_DYNAMIC(CMPCThemeToolTipCtrl, CToolTipCtrl)
BEGIN_MESSAGE_MAP(CMPCThemeToolTipCtrl, CToolTipCtrl)
    ON_WM_PAINT()
    ON_WM_ERASEBKGND()
    ON_WM_CREATE()
    ON_WM_MOVE()
    ON_WM_SHOWWINDOW()
    ON_WM_SIZE()
END_MESSAGE_MAP()

void CMPCThemeToolTipCtrl::drawText(CDC& dc, CMPCThemeToolTipCtrl* tt, CRect& rect, bool calcRect) {
    CFont font;
    CMPCThemeUtil::getFontByType(font, &dc, CMPCThemeUtil::MessageFont);
    CFont* pOldFont = dc.SelectObject(&font);

    CString text;
    tt->GetWindowText(text);
    int maxWidth = tt->GetMaxTipWidth();
    int calcStyle = 0;
    if (calcRect) {
        calcStyle = DT_CALCRECT;
    }
    rect.DeflateRect(6, 2);
    if (maxWidth == -1)
        dc.DrawText(text, rect, DT_VCENTER | DT_CENTER | DT_SINGLELINE | calcStyle);
    else
        dc.DrawText(text, rect, DT_LEFT | DT_WORDBREAK | calcStyle);
    rect.InflateRect(6, 2); //when calculating, put it back


    dc.SelectObject(pOldFont);
}

void CMPCThemeToolTipCtrl::paintTT(CDC& dc, CMPCThemeToolTipCtrl* tt) {
    CRect r;
    tt->GetClientRect(r);

    dc.FillSolidRect(r, CMPCTheme::MenuBGColor);

    CBrush fb;
    fb.CreateSolidBrush(CMPCTheme::TooltipBorderColor);
    dc.FrameRect(r, &fb);
    COLORREF oldClr = dc.SetTextColor(CMPCTheme::TextFGColor);
    drawText(dc, tt, r, false);
    dc.SetTextColor(oldClr);
}

void CMPCThemeToolTipCtrl::OnPaint() {
    CPaintDC dc(this);
    if (useFlickerHelper) { //helper will paint
        return;
    }
    paintTT(dc, this);
}


BOOL CMPCThemeToolTipCtrl::OnEraseBkgnd(CDC* pDC) {
    return TRUE;
}


int CMPCThemeToolTipCtrl::OnCreate(LPCREATESTRUCT lpCreateStruct) {
    if (CToolTipCtrl::OnCreate(lpCreateStruct) == -1)
        return -1;

    makeHelper();
    return 0;
}

void CMPCThemeToolTipCtrl::makeHelper() {
    if (!useFlickerHelper) return;

    if (nullptr != helper) {
        delete helper;
    }
    CRect r;
    GetClientRect(r);
    ClientToScreen(r);

    helper = new CMPCThemeToolTipCtrlHelper(this);
    //do it the long way since no menu for parent
    helper->CreateEx(NULL, AfxRegisterWndClass(0), NULL, WS_POPUP | WS_DISABLED,
        r.left, r.top, r.right - r.left, r.bottom - r.top,
        GetParent()->GetSafeHwnd(), NULL, NULL);
    helper->Invalidate();
    helper->ShowWindow(SW_SHOWNOACTIVATE);
}


BEGIN_MESSAGE_MAP(CMPCThemeToolTipCtrl::CMPCThemeToolTipCtrlHelper, CWnd)
    ON_WM_PAINT()
    ON_WM_ERASEBKGND()
    ON_WM_NCCALCSIZE()
END_MESSAGE_MAP()


CMPCThemeToolTipCtrl::CMPCThemeToolTipCtrlHelper::CMPCThemeToolTipCtrlHelper(CMPCThemeToolTipCtrl * tt) {
    this->tt = tt;
}

CMPCThemeToolTipCtrl::CMPCThemeToolTipCtrlHelper::~CMPCThemeToolTipCtrlHelper() {
    DestroyWindow();
}

void CMPCThemeToolTipCtrl::CMPCThemeToolTipCtrlHelper::OnPaint() {
    CPaintDC dc(this);
    CMPCThemeToolTipCtrl::paintTT(dc, tt);
}


BOOL CMPCThemeToolTipCtrl::CMPCThemeToolTipCtrlHelper::OnEraseBkgnd(CDC* pDC) {
    return TRUE;
}

void CMPCThemeToolTipCtrl::OnMove(int x, int y) {
    CToolTipCtrl::OnMove(x, y);
    makeHelper();
}


void CMPCThemeToolTipCtrl::OnShowWindow(BOOL bShow, UINT nStatus) {

    CToolTipCtrl::OnShowWindow(bShow, nStatus);
    if (!bShow) {
        if (helper != nullptr) {
            delete helper;
            helper = nullptr;
        }
    }
}

void CMPCThemeToolTipCtrl::OnSize(UINT nType, int cx, int cy) {
    //hack to make it fit if fonts differ from parent. can be manually avoided
    //if the parent widget is set to same font (see CMPCThemePlayerListCtrl using MessageFont now)
    if (GetMaxTipWidth() == -1) {
        CWindowDC dc(this);

        CRect cr, origCr, wr, origWr;
        GetWindowRect(wr);
        GetClientRect(cr);
        origCr = cr;
        origWr = wr;
        drawText(dc, this, cr, true);//calculate crect required to fit the text
        wr.right += cr.Width() - origCr.Width();//add the difference to the window
        if (origWr.right != wr.right) {
            MoveWindow(wr, FALSE);
        }
    }

    CToolTipCtrl::OnSize(nType, cx, cy);
    makeHelper();
}
