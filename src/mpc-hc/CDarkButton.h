#pragma once

class CDarkButton :	public CMFCButton {
protected:
    void drawButton(HDC hdc, CRect rect, UINT state);
public:
	CDarkButton();
	virtual ~CDarkButton();
    void PreSubclassWindow();
    BOOL PreCreateWindow(CREATESTRUCT & cs);
    static void drawButtonBase(CDC* pDC, CRect rect, CString strText, bool selected, bool highLighted, bool focused, bool disabled, bool thin);
    DECLARE_DYNAMIC(CDarkButton)
	DECLARE_MESSAGE_MAP()
    afx_msg void OnNMCustomdraw(NMHDR *pNMHDR, LRESULT *pResult);
};

