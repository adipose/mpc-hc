#pragma once
#include "StaticLink.h"
#include "CDarkChildHelper.h"
class CDarkStaticLink :
	public CStaticLink, public CDarkChildHelper
{
protected:
    CBrush bgBrush;
public:
    DECLARE_DYNAMIC(CDarkStaticLink)
    CDarkStaticLink(LPCTSTR lpText = nullptr, bool bDeleteOnDestroy = false);
	virtual ~CDarkStaticLink();
    DECLARE_MESSAGE_MAP()
    afx_msg void OnPaint();
    afx_msg HBRUSH CtlColor(CDC* pDC, UINT nCtlColor);
    afx_msg void OnEnable(BOOL bEnable);
};

