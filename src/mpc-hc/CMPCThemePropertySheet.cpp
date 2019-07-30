#include "stdafx.h"
#include "CMPCThemePropertySheet.h"
#include "CDarkTheme.h"

CMPCThemePropertySheet::CMPCThemePropertySheet(UINT nIDCaption, CWnd* pParentWnd, UINT iSelectPage)
    : CPropertySheet(nIDCaption, pParentWnd, iSelectPage) {
}

CMPCThemePropertySheet::CMPCThemePropertySheet(LPCTSTR pszCaption, CWnd* pParentWnd, UINT iSelectPage)
    : CPropertySheet(pszCaption, pParentWnd, iSelectPage) {
}

CMPCThemePropertySheet::~CMPCThemePropertySheet() {
}

IMPLEMENT_DYNAMIC(CMPCThemePropertySheet, CPropertySheet)
BEGIN_MESSAGE_MAP(CMPCThemePropertySheet, CPropertySheet)
    ON_WM_CTLCOLOR()
END_MESSAGE_MAP()

BOOL CMPCThemePropertySheet::OnInitDialog() {
    BOOL bResult = __super::OnInitDialog();
    fulfillThemeReqs();
    return bResult;
}

void CMPCThemePropertySheet::fulfillThemeReqs() {
    if (AfxGetAppSettings().bDarkThemeLoaded) {
        CDarkChildHelper::fulfillThemeReqs((CWnd*)this);
    }
}

HBRUSH CMPCThemePropertySheet::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor) {
    if (AfxGetAppSettings().bDarkThemeLoaded) {
        LRESULT lResult;
        if (pWnd->SendChildNotifyLastMsg(&lResult)) {
            return (HBRUSH)lResult;
        }
        pDC->SetTextColor(CDarkTheme::TextFGColor);
        pDC->SetBkColor(CDarkTheme::ControlAreaBGColor);
        return darkControlAreaBrush;
    } else {
        HBRUSH hbr = __super::OnCtlColor(pDC, pWnd, nCtlColor);
        return hbr;
    }
}
