#pragma once
#include "..\CmdUI\CmdUI.h"
#include "CDarkButton.h"
#include "CDarkGroupBox.h"
#include "CDarkLinkCtrl.h"
#include "CDarkChildHelper.h"
class CMPCThemeCmdUIDialog : public CCmdUIDialog, public CDarkChildHelper
{
public:
	CMPCThemeCmdUIDialog();
    CMPCThemeCmdUIDialog(UINT nIDTemplate, CWnd* pParent = nullptr);
    CMPCThemeCmdUIDialog(LPCTSTR lpszTemplateName, CWnd* pParent = nullptr);
    virtual ~CMPCThemeCmdUIDialog();
    void enableDarkThemeIfActive() { CDarkChildHelper::enableDarkThemeIfActive((CWnd*)this); };
    DECLARE_MESSAGE_MAP()
public:
    afx_msg HBRUSH OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor);
};

