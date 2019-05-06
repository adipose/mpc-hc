#pragma once
#include "stdafx.h"

class CDarkTheme {
public:
    static const COLORREF MenuBGColor;
    static const COLORREF WindowBGColor;  //used in explorer for left nav
    static const COLORREF ContentBGColor; //used in explorer for bg of file list
    static const COLORREF ContentSelectedColor; //used in explorer for bg of file list
    static const COLORREF PlayerBGColor;

    static const COLORREF MenuSelectedColor;
    static const COLORREF MenuItemDisabledColor;
    static const COLORREF MenuSeparatorColor;

    static const COLORREF ShadowColor;
    static const COLORREF TextFGColor;
    static const COLORREF ContentTextDisabledFGColor;
    static const COLORREF SubmenuColor;
    static const COLORREF LightColor;
    static const COLORREF CloseHoverColor;
    static const COLORREF ClosePushColor;
    static const COLORREF WindowBorderColorLight;
    static const COLORREF WindowBorderColorDim;
    static const COLORREF NoBorderColor;
    static const COLORREF GripperPatternColor;

    static const COLORREF ScrollBGColor;
    static const COLORREF ScrollThumbColor;
    static const COLORREF ScrollThumbHoverColor;
    static const COLORREF ScrollThumbDragColor;
    static const COLORREF ScrollButtonArrowColor;
    static const COLORREF ScrollButtonHoverColor;
    static const COLORREF ScrollButtonClickColor;

    static const COLORREF EditBorderColor;
    static const COLORREF TooltipBorderColor;
    
    static const COLORREF DebugColorRed;
    static const COLORREF DebugColorYellow;
    static const COLORREF DebugColorGreen;

    static const COLORREF PlayerButtonHotColor;
    static const COLORREF PlayerButtonCheckedColor;
    static const COLORREF PlayerButtonClickedColor;
    static const COLORREF PlayerButtonBorderColor;


    static const BYTE GripperBitsH[10];
    static const BYTE GripperBitsV[8];
    static const int gripPatternShort;
    static const int gripPatternLong;

    static wchar_t* const uiTextFont;
    static wchar_t* const uiSymbolFont;

    static const BYTE ScrollArrowBitsV[12];
    static const int scrollArrowShort;
    static const int scrollArrowLong;

    static CFont* getUIFont(HDC hDC, wchar_t *fontName, int size, LONG weight = FW_REGULAR);

    static CSize GetTextSize(CString str, HDC hDC);

};
