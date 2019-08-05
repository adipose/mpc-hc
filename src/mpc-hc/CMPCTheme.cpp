#include "stdafx.h"
#include "CMPCTheme.h"
#include "mplayerc.h"

const COLORREF CMPCTheme::MenuBGColor = COLORREF(RGB(43, 43, 43));
const COLORREF CMPCTheme::WindowBGColor = COLORREF(RGB(25, 25, 25));
const COLORREF CMPCTheme::ControlAreaBGColor = COLORREF(RGB(56, 56, 56));

const COLORREF CMPCTheme::ContentBGColor = COLORREF(RGB(32, 32, 32));
const COLORREF CMPCTheme::ContentSelectedColor = COLORREF(RGB(119, 119, 119));
const COLORREF CMPCTheme::PlayerBGColor = COLORREF(RGB(32, 32, 32));

const COLORREF CMPCTheme::HighLightColor = GetSysColor(COLOR_HIGHLIGHT);

const COLORREF CMPCTheme::MenuSelectedColor = COLORREF(RGB(65, 65, 65));
const COLORREF CMPCTheme::MenuSeparatorColor = COLORREF(RGB(128, 128, 128));
const COLORREF CMPCTheme::MenuItemDisabledColor = COLORREF(RGB(109, 109, 109));

const COLORREF CMPCTheme::ShadowColor = COLORREF(RGB(25, 25, 25));
const COLORREF CMPCTheme::TextFGColor = COLORREF(RGB(255, 255, 255));
const COLORREF CMPCTheme::TextFGColorFade = COLORREF(RGB(200, 200, 200));
const COLORREF CMPCTheme::ContentTextDisabledFGColorFade = COLORREF(RGB(109, 109, 109));
const COLORREF CMPCTheme::ContentTextDisabledFGColorFade2 = COLORREF(RGB(60, 60, 60)); //even more faded, used for NA text on CListCtrl/audio switcher

const COLORREF CMPCTheme::SubmenuColor = COLORREF(RGB(191, 191, 191));
const COLORREF CMPCTheme::LightColor = COLORREF(RGB(100, 100, 100));
const COLORREF CMPCTheme::CloseHoverColor = COLORREF(RGB(232, 17, 35));
const COLORREF CMPCTheme::ClosePushColor = COLORREF(RGB(139, 10, 20));

const COLORREF CMPCTheme::WindowBorderColorLight = COLORREF(RGB(99, 99, 99));
const COLORREF CMPCTheme::WindowBorderColorDim = COLORREF(RGB(43, 43, 43));
const COLORREF CMPCTheme::NoBorderColor = COLORREF(RGB(0, 0, 0));
const COLORREF CMPCTheme::GripperPatternColor = COLORREF(RGB(70, 70, 74)); //visual studio dark, since explorer has no grippers

const COLORREF CMPCTheme::ScrollBGColor = COLORREF(RGB(23, 23, 23));
const COLORREF CMPCTheme::ScrollChapterColor = COLORREF(RGB(60, 60, 60));
const COLORREF CMPCTheme::ScrollThumbColor = COLORREF(RGB(77, 77, 77));
const COLORREF CMPCTheme::ScrollThumbHoverColor = COLORREF(RGB(144, 144, 144));
const COLORREF CMPCTheme::ScrollThumbDragColor = COLORREF(RGB(183, 183, 183));
const COLORREF CMPCTheme::ScrollButtonArrowColor = COLORREF(RGB(103, 103, 103));
const COLORREF CMPCTheme::ScrollButtonHoverColor = COLORREF(RGB(55, 55, 55));
const COLORREF CMPCTheme::ScrollButtonClickColor = COLORREF(RGB(166, 166, 166));

const COLORREF CMPCTheme::InlineEditBorderColor = COLORREF(RGB(255, 255, 255));
const COLORREF CMPCTheme::TooltipBorderColor = COLORREF(RGB(118, 118, 118));

const COLORREF CMPCTheme::GroupBoxBorderColor = COLORREF(RGB(118, 118, 118));
const int CMPCTheme::GroupBoxTextIndent = 8;

const COLORREF CMPCTheme::PlayerButtonHotColor = COLORREF(RGB(43, 43, 43));
const COLORREF CMPCTheme::PlayerButtonCheckedColor = COLORREF(RGB(66, 66, 66)); 
const COLORREF CMPCTheme::PlayerButtonClickedColor = COLORREF(RGB(55, 55, 55));
const COLORREF CMPCTheme::PlayerButtonBorderColor = COLORREF(RGB(0, 0, 0));

const COLORREF CMPCTheme::DebugColorRed = COLORREF(RGB(255, 0, 0));
const COLORREF CMPCTheme::DebugColorYellow = COLORREF(RGB(255, 255, 0));
const COLORREF CMPCTheme::DebugColorGreen = COLORREF(RGB(0, 255, 0));

const COLORREF CMPCTheme::ButtonBorderOuterColor = COLORREF(RGB(240, 240, 240));
const COLORREF CMPCTheme::ButtonBorderInnerFocusedColor = COLORREF(RGB(255, 255, 255));
const COLORREF CMPCTheme::ButtonBorderInnerColor = COLORREF(RGB(155, 155, 155));
const COLORREF CMPCTheme::ButtonBorderSelectedKBFocusColor = COLORREF(RGB(150, 150, 150));
const COLORREF CMPCTheme::ButtonBorderHoverKBFocusColor = COLORREF(RGB(181, 181, 181));
const COLORREF CMPCTheme::ButtonBorderKBFocusColor = COLORREF(RGB(195, 195, 195));
const COLORREF CMPCTheme::ButtonFillColor = COLORREF(RGB(51, 51, 51));
const COLORREF CMPCTheme::ButtonFillHoverColor = COLORREF(RGB(69, 69, 69));
const COLORREF CMPCTheme::ButtonFillSelectedColor = COLORREF(RGB(102, 102, 102));
const COLORREF CMPCTheme::ButtonDisabledFGColor = COLORREF(RGB(109, 109, 109));

const COLORREF CMPCTheme::CheckboxBorderColor = COLORREF(RGB(137, 137, 137));
const COLORREF CMPCTheme::CheckboxBGColor = COLORREF(RGB(0, 0, 0));
const COLORREF CMPCTheme::CheckboxBorderHoverColor = COLORREF(RGB(121, 121, 121));
const COLORREF CMPCTheme::CheckboxBGHoverColor = COLORREF(RGB(8, 8, 8));

const COLORREF CMPCTheme::ImageDisabledColor = COLORREF(RGB(109, 109, 109));

const COLORREF CMPCTheme::SliderChannelColor = COLORREF(RGB(109, 109, 109));

const COLORREF CMPCTheme::EditBorderColor = COLORREF(RGB(106, 106, 106));

const COLORREF CMPCTheme::TreeCtrlLineColor = COLORREF(RGB(106, 106, 106));
const COLORREF CMPCTheme::TreeCtrlHoverColor = COLORREF(RGB(77, 77, 77));;
const COLORREF CMPCTheme::TreeCtrlFocusColor = COLORREF(RGB(98, 98, 98));;

const COLORREF CMPCTheme::CheckColor = COLORREF(RGB(222, 222, 222));;

const COLORREF CMPCTheme::ColumnHeaderHotColor = COLORREF(RGB(67, 67, 67));;

const COLORREF CMPCTheme::StaticEtchedColor = COLORREF(RGB(65, 65, 65));

const COLORREF CMPCTheme::ListCtrlDisabledBGColor = COLORREF(RGB(40, 40, 40));
const COLORREF CMPCTheme::ListCtrlGridColor = COLORREF(RGB(43, 43, 43));
const COLORREF CMPCTheme::HeaderCtrlGridColor = COLORREF(RGB(99, 99, 99));
const COLORREF CMPCTheme::AudioSwitcherGridColor = COLORREF(RGB(99, 99, 99));

const COLORREF CMPCTheme::TabCtrlBorderColor = COLORREF(RGB(99, 99, 99));
const COLORREF CMPCTheme::TabCtrlInactiveColor = COLORREF(RGB(40, 40, 40));


const COLORREF CMPCTheme::StatusBarBGColor = COLORREF(RGB(51, 51, 51));
const COLORREF CMPCTheme::StatusBarSeparatorColor = COLORREF(RGB(247, 247, 247));

const COLORREF CMPCTheme::W10DarkThemeFileDialogInjectedTextColor = COLORREF(RGB(255, 255, 255));
const COLORREF CMPCTheme::W10DarkThemeFileDialogInjectedBGColor = COLORREF(RGB(56, 56, 56));
const COLORREF CMPCTheme::W10DarkThemeFileDialogInjectedEditBorderColor = COLORREF(RGB(155, 155, 155));


const COLORREF CMPCTheme::ProgressBarBGColor = COLORREF(RGB(0, 0, 0));
const COLORREF CMPCTheme::ProgressBarColor = COLORREF(RGB(75, 75, 75));

const COLORREF CMPCTheme::SubresyncFadeText1 = COLORREF(RGB(190, 190, 190));
const COLORREF CMPCTheme::SubresyncFadeText2 = COLORREF(RGB(160, 160, 160));
const COLORREF CMPCTheme::SubresyncActiveFadeText = COLORREF(RGB(215, 215, 215));
const COLORREF CMPCTheme::SubresyncHLColor1 = COLORREF(RGB(100, 100, 100));
const COLORREF CMPCTheme::SubresyncHLColor2 = COLORREF(RGB(80, 80, 80));
const COLORREF CMPCTheme::SubresyncGridSepColor = COLORREF(RGB(220, 220, 220));

wchar_t* const CMPCTheme::uiTextFont = L"Segoe UI";
wchar_t* const CMPCTheme::uiStaticTextFont = L"Segoe UI Semilight";
wchar_t* const CMPCTheme::uiSymbolFont = L"MS UI Gothic";


const int CMPCTheme::gripPatternLong = 5;
const int CMPCTheme::gripPatternShort = 4;

const BYTE CMPCTheme::GripperBitsH[10] = {
    0x80, 0x00,
    0x00, 0x00,
    0x20, 0x00,
    0x00, 0x00,
    0x80, 0x00,
};

const BYTE CMPCTheme::GripperBitsV[8] = {
    0x88, 0x00,
    0x00, 0x00,
    0x20, 0x00,
    0x00, 0x00,
};

const int CMPCTheme::scrollArrowLong = 7;
const int CMPCTheme::scrollArrowShort = 6;

const BYTE CMPCTheme::ScrollArrowBitsV[12] = {
    0x10, 0x00,
    0x38, 0x00,
    0x7C, 0x00,
    0xEE, 0x00,
    0xC6, 0x00,
    0x82, 0x00,
};
const BYTE CMPCTheme::ScrollArrowBitsH[14] = {
    0x1C, 0x00,
    0x38, 0x00,
    0x70, 0x00,
    0xE0, 0x00,
    0x70, 0x00,
    0x38, 0x00,
    0x1C, 0x00,
};

const BYTE CMPCTheme::ComboArrowBits[10] = {
    0x80, 0x40,
    0x40, 0x80,
    0x21, 0x00,
    0x12, 0x00,
    0x0C, 0x00,
};
const int CMPCTheme::ComboArrowWidth = 10;
const int CMPCTheme::ComboArrowHeight = 5;

const COLORREF CMPCTheme::ComboboxArrowColor[3] = {
    COLORREF(RGB(113, 113, 113)),
    COLORREF(RGB(173, 173, 173)),
    COLORREF(RGB(62, 62, 62)),
};

const COLORREF CMPCTheme::ComboboxArrowColorHover[3] = {
    COLORREF(RGB(123, 123, 123)),
    COLORREF(RGB(177, 177, 177)),
    COLORREF(RGB(78, 78, 78)),
};

const COLORREF CMPCTheme::ComboboxArrowColorClick[3] = {
    COLORREF(RGB(143, 143, 143)),
    COLORREF(RGB(183, 183, 183)),
    COLORREF(RGB(109, 109, 109)),
};

const BYTE CMPCTheme::SpinnerArrowBitsV[6] = {
    0x20, 0x00,
    0x70, 0x00,
    0xF8, 0x00,
};
const BYTE CMPCTheme::SpinnerArrowBitsH[10] = {
    0x20, 0x00,
    0x60, 0x00,
    0xE0, 0x00,
    0x60, 0x00,
    0x20, 0x00,
};

const int CMPCTheme::SpinnerArrowWidth = 5;
const int CMPCTheme::SpinnerArrowHeight = 3;

const BYTE CMPCTheme::CheckBits[14] = {
    0x02, 0x00,
    0x06, 0x00,
    0x8E, 0x00,
    0xDC, 0x00,
    0xF8, 0x00,
    0x70, 0x00,
    0x20, 0x00,
};

const int CMPCTheme::CheckWidth = 7;
const int CMPCTheme::CheckHeight = 7;


const UINT CMPCTheme::ThemeCheckBoxes[5] = {
    IDB_DT_CB_96,
    IDB_DT_CB_120,
    IDB_DT_CB_144,
    IDB_DT_CB_144,
    IDB_DT_CB_192,
};

const UINT CMPCTheme::ThemeRadios[5] = {
    IDB_DT_RADIO_96,
    IDB_DT_RADIO_120,
    IDB_DT_RADIO_144,
    IDB_DT_RADIO_144,
    IDB_DT_RADIO_192,
};

