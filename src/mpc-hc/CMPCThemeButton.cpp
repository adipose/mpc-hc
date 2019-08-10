#include "stdafx.h"
#include "CMPCThemeButton.h"
#include "CMPCTheme.h"
#include "CMPCThemeUtil.h"
#include "mplayerc.h"

CMPCThemeButton::CMPCThemeButton() {
    if (AfxGetAppSettings().bMPCThemeLoaded) {
        m_nFlatStyle = CMFCButton::BUTTONSTYLE_FLAT; //just setting this to get hovering working
    }
    drawShield = false;
}

CMPCThemeButton::~CMPCThemeButton() {
}

void CMPCThemeButton::PreSubclassWindow() { //bypass CMFCButton impl since it will enable ownerdraw
    InitStyle(GetStyle());
    CButton::PreSubclassWindow();
}

BOOL CMPCThemeButton::PreCreateWindow(CREATESTRUCT& cs) {//bypass CMFCButton impl since it will enable ownerdraw
    InitStyle(cs.style);
    return CButton::PreCreateWindow(cs);
}


IMPLEMENT_DYNAMIC(CMPCThemeButton, CMFCButton)
BEGIN_MESSAGE_MAP(CMPCThemeButton, CMFCButton)
    ON_WM_SETFONT()
    ON_WM_GETFONT()
    ON_NOTIFY_REFLECT(NM_CUSTOMDRAW, &CMPCThemeButton::OnNMCustomdraw)
    ON_MESSAGE(BCM_SETSHIELD, &setShieldIcon)
    ON_WM_LBUTTONUP()
END_MESSAGE_MAP()

LRESULT CMPCThemeButton::setShieldIcon(WPARAM wParam, LPARAM lParam) {
    drawShield = (BOOL)lParam;
    return 1; //pass it along
}

void CMPCThemeButton::drawButtonBase(CDC* pDC, CRect rect, CString strText, bool selected, bool highLighted, bool focused, bool disabled, bool thin, bool shield) {
    CBrush fb, fb2;
    fb.CreateSolidBrush(CMPCTheme::ButtonBorderOuterColor);

    if (!thin) { //some small buttons look very ugly with the full border.  make up our own solution
        pDC->FrameRect(rect, &fb);
        rect.DeflateRect(1, 1);
    }
    COLORREF bg = CMPCTheme::ButtonFillColor, dottedClr = CMPCTheme::ButtonBorderKBFocusColor;

    if (selected) {//mouse down
        fb2.CreateSolidBrush(CMPCTheme::ButtonBorderInnerColor);
        bg = CMPCTheme::ButtonFillSelectedColor;
        dottedClr = CMPCTheme::ButtonBorderSelectedKBFocusColor;
    } else if (highLighted) {
        fb2.CreateSolidBrush(CMPCTheme::ButtonBorderInnerColor);
        bg = CMPCTheme::ButtonFillHoverColor;
        dottedClr = CMPCTheme::ButtonBorderHoverKBFocusColor;
    } else if (focused) {
        fb2.CreateSolidBrush(CMPCTheme::ButtonBorderInnerFocusedColor);
    } else {
        fb2.CreateSolidBrush(CMPCTheme::ButtonBorderInnerColor);
    }

    pDC->FrameRect(rect, &fb2);
    rect.DeflateRect(1, 1);
    pDC->FillSolidRect(rect, bg);


    if (focused) {
        rect.DeflateRect(1, 1);
        COLORREF oldTextFGColor = pDC->SetTextColor(dottedClr);
        COLORREF oldBGColor = pDC->SetBkColor(bg);
        CBrush *dotted = pDC->GetHalftoneBrush();
        pDC->FrameRect(rect, dotted);
        DeleteObject(dotted);
        pDC->SetTextColor(oldTextFGColor);
        pDC->SetBkColor(oldBGColor);
    }

    if (!strText.IsEmpty()) {
        int nMode = pDC->SetBkMode(TRANSPARENT);

        COLORREF oldTextFGColor;
        if (disabled)
            oldTextFGColor = pDC->SetTextColor(CMPCTheme::ButtonDisabledFGColor);
        else
            oldTextFGColor = pDC->SetTextColor(CMPCTheme::TextFGColor);

        UINT format = DT_CENTER | DT_VCENTER | DT_SINGLELINE;
        if (shield) {
            int iconsize = MulDiv(pDC->GetDeviceCaps(LOGPIXELSX), 1, 6);
            int shieldY = rect.top + (rect.Height() - iconsize) / 2 + 1;
            CRect centerRect = rect;
            pDC->DrawText(strText, rect, format | DT_CALCRECT);
            rect.top = centerRect.top;
            rect.bottom = centerRect.bottom;
            rect.OffsetRect((centerRect.Width() - rect.Width() + iconsize) / 2, 0);
            int shieldX = rect.left - iconsize - 1;
            HICON hShieldIcon = (HICON)LoadImage(0, IDI_SHIELD, IMAGE_ICON, iconsize, iconsize, LR_SHARED);
            if (hShieldIcon) {
                DrawIconEx(pDC->GetSafeHdc(), shieldX, shieldY, hShieldIcon, iconsize, iconsize, 0, NULL, DI_NORMAL);
            }
        }

        pDC->DrawText(strText, rect, format);

        pDC->SetTextColor(oldTextFGColor);
        pDC->SetBkMode(nMode);
    }
}

#define max(a,b)            (((a) > (b)) ? (a) : (b))
void CMPCThemeButton::drawButton(HDC hdc, CRect rect, UINT state) {
    CDC* pDC = CDC::FromHandle(hdc);

    CString strText;
    GetWindowText(strText);
    bool selected = ODS_SELECTED == (state & ODS_SELECTED);
    bool focused = ODS_FOCUS == (state & ODS_FOCUS);
    bool disabled = ODS_DISABLED == (state & ODS_DISABLED);

    BUTTON_IMAGELIST imgList;
    GetImageList(&imgList);
    CImageList *images = CImageList::FromHandlePermanent(imgList.himl);
    bool thin = (images != nullptr); //thin borders for image buttons


    drawButtonBase(pDC, rect, strText, selected, IsHighlighted(), focused, disabled, thin, drawShield);

    int imageIndex = 0; //Normal
    if (disabled) {
        imageIndex = 1;
    }

    if (images != nullptr) { //assume centered
        IMAGEINFO ii;
        if (images->GetImageCount() <= imageIndex) imageIndex = 0;
        images->GetImageInfo(imageIndex, &ii);
        int width = ii.rcImage.right - ii.rcImage.left;
        int height = ii.rcImage.bottom - ii.rcImage.top;
        rect.DeflateRect((rect.Width() - width) / 2, max(0, (rect.Height() - height) / 2));
        images->Draw(pDC, imageIndex, rect.TopLeft(), ILD_NORMAL);
    }
}

void CMPCThemeButton::OnNMCustomdraw(NMHDR *pNMHDR, LRESULT *pResult) {
    LPNMCUSTOMDRAW pNMCD = reinterpret_cast<LPNMCUSTOMDRAW>(pNMHDR);
    *pResult = CDRF_DODEFAULT;
    if (AfxGetAppSettings().bMPCThemeLoaded) {
        if (pNMCD->dwDrawStage == CDDS_PREERASE) {
            drawButton(pNMCD->hdc, pNMCD->rc, pNMCD->uItemState);
            *pResult = CDRF_SKIPDEFAULT;
        }
    }
}
void CMPCThemeButton::OnSetFont(CFont* pFont, BOOL bRedraw) {
    Default(); //bypass the MFCButton font impl since we don't always draw this button ourselves (classic mode)
}

HFONT CMPCThemeButton::OnGetFont() {
    return (HFONT)Default();
}


void CMPCThemeButton::OnLButtonUp(UINT nFlags, CPoint point) {
    BOOL bClicked = m_bPushed && m_bClickiedInside && m_bHighlighted;

    m_bPushed = FALSE;
    m_bClickiedInside = FALSE;
    m_bHighlighted = FALSE;

    if (bClicked && m_bAutoCheck) {
        if (m_bCheckButton) {
            m_bChecked = !m_bChecked;
        } else if (m_bRadioButton && !m_bChecked) {
            m_bChecked = TRUE;
            UncheckRadioButtonsInGroup();
        }
    }

    HWND hWnd = GetSafeHwnd();

    if (m_bWasDblClk) { //we don't send a second single click after a double click!
        m_bWasDblClk = FALSE;
    }

    if (!::IsWindow(hWnd)) {
        return;
    }

    RedrawWindow();

    CButton::OnLButtonUp(nFlags, point);

    if (!::IsWindow(hWnd)) {
        return;
    }

    if (m_bCaptured) {
        ReleaseCapture();
        m_bCaptured = FALSE;
    }

    if (m_nAutoRepeatTimeDelay > 0) {
        KillTimer(AFX_TIMER_ID_AUTOCOMMAND);
    }

    if (m_pToolTip->GetSafeHwnd() != NULL) {
        m_pToolTip->Pop();
    }
}
