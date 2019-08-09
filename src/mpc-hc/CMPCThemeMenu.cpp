﻿#include "stdafx.h"
#include "CMPCThemeMenu.h"
#include "CMPCTheme.h"
#include "CMPCThemeUtil.h"
#include <strsafe.h>
#include "AppSettings.h"
#include "PPageAccelTbl.h"
#include "mplayerc.h"

std::map<UINT, CMPCThemeMenu*> CMPCThemeMenu::subMenuIDs;


IMPLEMENT_DYNAMIC(CMPCThemeMenu, CMenu);

bool CMPCThemeMenu::hasDimensions = false;
int CMPCThemeMenu::subMenuPadding;
int CMPCThemeMenu::iconSpacing;
int CMPCThemeMenu::iconPadding;
int CMPCThemeMenu::rowPadding;
int CMPCThemeMenu::separatorPadding;
int CMPCThemeMenu::separatorHeight;
int CMPCThemeMenu::postTextSpacing;
int CMPCThemeMenu::accelSpacing;

CMPCThemeMenu::CMPCThemeMenu(){
}


CMPCThemeMenu::~CMPCThemeMenu() {
    std::map<UINT, CMPCThemeMenu*>::iterator itr = subMenuIDs.begin();
    while (itr != subMenuIDs.end()) {
        if (itr->second == this) {
            itr = subMenuIDs.erase(itr);
        } else {
            ++itr;
        }
    }

    for (u_int i = 0; i < allocatedItems.size(); i++) {
        delete allocatedItems[i];
    }
    for (u_int i = 0; i < allocatedMenus.size(); i++) {
        delete allocatedMenus[i];
    }
}

void CMPCThemeMenu::initDimensions() {
    if (!hasDimensions) {
        DpiHelper dpi = DpiHelper();

        subMenuPadding = dpi.ScaleX(20);
        iconSpacing = dpi.ScaleX(22);
        iconPadding = dpi.ScaleX(10);
        rowPadding = dpi.ScaleY(4) + 3; //windows 10 explorer has paddings of 7,8,9,9,11--this yields 7,8,9,10,11
        separatorPadding = dpi.ScaleX(8);
        separatorHeight = dpi.ScaleX(7);
        postTextSpacing = dpi.ScaleX(20);
        accelSpacing = dpi.ScaleX(30);
        hasDimensions = true;
    }
}


void CMPCThemeMenu::fulfillThemeReqs(bool isMenubar) {
    if (AfxGetAppSettings().bMPCThemeLoaded) {
        MENUINFO MenuInfo = { 0 };
        MenuInfo.cbSize = sizeof(MENUINFO);
        MenuInfo.fMask = MIM_BACKGROUND | MIM_STYLE | MIM_APPLYTOSUBMENUS;
        MenuInfo.dwStyle = MNS_AUTODISMISS;
        MenuInfo.hbrBack = ::CreateSolidBrush(CMPCTheme::MenuBGColor);
        SetMenuInfo(&MenuInfo);

        int iMaxItems = GetMenuItemCount();
        for (int i = 0; i < iMaxItems; i++) {
            CString nameHolder;
            MenuObject* pObject = new MenuObject;
            allocatedItems.push_back(pObject);
            pObject->m_hIcon = NULL;
            pObject->isMenubar = isMenubar;
            if (i == 0) pObject->isFirstMenuInMenuBar = true;

            GetMenuString(i, pObject->m_strCaption, MF_BYPOSITION);

            UINT nID = GetMenuItemID(i);
            pObject->m_strAccel = CPPageAccelTbl::MakeAccelShortcutLabel(nID);

            subMenuIDs[nID] = this;

            MENUITEMINFO tInfo;
            ZeroMemory(&tInfo, sizeof(MENUITEMINFO));
            tInfo.fMask = MIIM_FTYPE;
            tInfo.cbSize = sizeof(MENUITEMINFO);
            GetMenuItemInfo(i, &tInfo, true);

            if (tInfo.fType & MFT_SEPARATOR) {
                pObject->isSeparator = true;
            }

            MENUITEMINFO mInfo;
            ZeroMemory(&mInfo, sizeof(MENUITEMINFO));

            mInfo.fMask = MIIM_FTYPE | MIIM_DATA;
            mInfo.fType = MFT_OWNERDRAW | tInfo.fType;
            mInfo.cbSize = sizeof(MENUITEMINFO);
            mInfo.dwItemData = (ULONG_PTR)pObject;
            SetMenuItemInfo(i, &mInfo, true);

            CMenu* t = GetSubMenu(i);
            if (nullptr != t) {
                CMPCThemeMenu* pSubMenu = new CMPCThemeMenu;
                allocatedMenus.push_back(pSubMenu);
                pSubMenu->Attach(t->GetSafeHmenu());
                pSubMenu->fulfillThemeReqs();
            }
        }
    }
}

void CMPCThemeMenu::fullfillThemeReqsItem(UINT i, bool byCommand) {

    int iMaxItems = GetMenuItemCount();

    CString nameHolder;
    MenuObject* pObject = new MenuObject;
    allocatedItems.push_back(pObject);
    pObject->m_hIcon = NULL;

    UINT posOrCmd = byCommand ? MF_BYCOMMAND : MF_BYPOSITION;

    GetMenuString(i, pObject->m_strCaption, posOrCmd);


    UINT nID;
    if (byCommand) {
        nID = i;
        bool found = false;
        for (int j = 0; j < iMaxItems; j++) {
            if (nID == GetMenuItemID(j)) {
                i = j;
                found = true;
                break;
            }
        }
        if (!found) return;
    } else {
        nID = GetMenuItemID(i);
    }

    pObject->m_strAccel = CPPageAccelTbl::MakeAccelShortcutLabel(nID);

    subMenuIDs[nID] = this;

    MENUITEMINFO tInfo;
    ZeroMemory(&tInfo, sizeof(MENUITEMINFO));
    tInfo.fMask = MIIM_FTYPE;
    tInfo.cbSize = sizeof(MENUITEMINFO);
    GetMenuItemInfo(i, &tInfo, true);

    if (tInfo.fType & MFT_SEPARATOR) {
        pObject->isSeparator = true;
    }

    MENUITEMINFO mInfo;
    ZeroMemory(&mInfo, sizeof(MENUITEMINFO));

    mInfo.fMask = MIIM_FTYPE | MIIM_DATA;
    mInfo.fType = MFT_OWNERDRAW | tInfo.fType;
    mInfo.cbSize = sizeof(MENUITEMINFO);
    mInfo.dwItemData = (ULONG_PTR)pObject;
    SetMenuItemInfo(i, &mInfo, true);

    CMenu *t = GetSubMenu(i);
    if (nullptr != t) {
        CMPCThemeMenu* pSubMenu = new CMPCThemeMenu;
        allocatedMenus.push_back(pSubMenu);
        pSubMenu->Attach(t->GetSafeHmenu());
        pSubMenu->fulfillThemeReqs();
    }
}

void CMPCThemeMenu::fullfillThemeReqsItem(CMenu* parent, UINT i, bool byCommand) {
    CMPCThemeMenu* t;
    if ((t = DYNAMIC_DOWNCAST(CMPCThemeMenu, parent)) != nullptr) {
        t->fullfillThemeReqsItem(i, byCommand);
    }
}

UINT CMPCThemeMenu::getPosFromID(CMenu * parent, UINT nID) {
    int iMaxItems = parent->GetMenuItemCount();
    for (int j = 0; j < iMaxItems; j++) {
        if (nID == parent->GetMenuItemID(j)) {
            return j;
        }
    }
    return (UINT)-1;
}

CMPCThemeMenu* CMPCThemeMenu::getParentMenu(UINT itemID) {
    if (subMenuIDs.count(itemID) == 1) {
        CMPCThemeMenu *m = subMenuIDs.at(itemID);
        /* // checks if submenu for overriding of onmeasureitem (win32 limitation).
           // but mpc-hc doesn't set up some submenus until later
           // which is too late for measureitem to take place
           // so we return all items for measuring
        MENUITEMINFO mInfo;
        ZeroMemory(&mInfo, sizeof(MENUITEMINFO));
        mInfo.fMask = MIIM_SUBMENU;
        mInfo.cbSize = sizeof(MENUITEMINFO);
        m->GetMenuItemInfo(itemID, &mInfo);
        if (mInfo.hSubMenu)      //  */
            return m;
    }

    return nullptr;
}

void CMPCThemeMenu::GetRects(RECT rcItem, CRect& rectFull, CRect& rectM, CRect &rectIcon, CRect &rectText, CRect &rectArrow) {
    rectFull.CopyRect(&rcItem);
    rectM = rectFull;
    rectIcon.SetRect(rectM.left, rectM.top, rectM.left + iconSpacing, rectM.bottom);
    rectText.SetRect(rectM.left + iconSpacing + iconPadding, rectM.top, rectM.right - subMenuPadding, rectM.bottom);
    rectArrow.SetRect(rectM.right - subMenuPadding, rectM.top, rectM.right, rectM.bottom);
}

void CMPCThemeMenu::DrawItem(LPDRAWITEMSTRUCT lpDrawItemStruct) {

    MenuObject* menuObject = (MenuObject*)lpDrawItemStruct->itemData;

    MENUITEMINFO mInfo;
    ZeroMemory(&mInfo, sizeof(MENUITEMINFO));

    mInfo.fMask = MIIM_FTYPE | MIIM_STATE | MIIM_SUBMENU;
    mInfo.cbSize = sizeof(MENUITEMINFO);
    GetMenuItemInfo(lpDrawItemStruct->itemID, &mInfo);

    CRect rectFull;
    CRect rectM;
    CRect rectIcon;
    CRect rectText;
    CRect rectArrow;

    GetRects(lpDrawItemStruct->rcItem, rectFull, rectM, rectIcon, rectText, rectArrow);
    
    UINT captionAlign = DT_LEFT;


    COLORREF ArrowColor = CMPCTheme::SubmenuColor;
    COLORREF TextFGColor;
    COLORREF TextBGColor = CMPCTheme::MenuBGColor;
    //TextBGColor = R255; //test
    COLORREF TextSelectColor = CMPCTheme::MenuSelectedColor;

    CDC* pDC = CDC::FromHandle(lpDrawItemStruct->hDC);

    if ((lpDrawItemStruct->itemState & ODS_DISABLED)) {
        TextFGColor = CMPCTheme::MenuItemDisabledColor;
        ArrowColor = CMPCTheme::MenuItemDisabledColor;
    } else {
        TextFGColor = CMPCTheme::TextFGColor;
    }

    int oldBKMode = pDC->SetBkMode(TRANSPARENT);
    pDC->FillSolidRect(&rectM, TextBGColor);

    if (menuObject->isMenubar) {
        if (menuObject->isFirstMenuInMenuBar) { //clean up white borders
            CRect wndSize;
            GetWindowRect(AfxGetMainWnd()->m_hWnd, &wndSize);

            CRect rectBorder(rectM.left, rectM.bottom, rectM.left + wndSize.right - wndSize.left, rectM.bottom+1);
            pDC->FillSolidRect(&rectBorder, CMPCTheme::MenuItemDisabledColor);
            ExcludeClipRect(lpDrawItemStruct->hDC, rectBorder.left, rectBorder.top, rectBorder.right, rectBorder.bottom);
        }
        rectM = rectFull;
        rectText = rectFull;
        captionAlign = DT_CENTER;
    } 

    if (mInfo.fType & MFT_SEPARATOR) {
        int centerOffset = (separatorHeight - 1) / 2;
        CRect rectSeparator(rectM.left + separatorPadding, rectM.top + centerOffset, rectM.right - separatorPadding, rectM.top + centerOffset + 1);
        pDC->FillSolidRect(&rectSeparator, CMPCTheme::MenuSeparatorColor);
    } else {



        COLORREF oldTextFGColor = pDC->SetTextColor(TextFGColor);
        CFont font;
        CMPCThemeUtil::getFontByType(font, pDC, CMPCThemeUtil::MenuFont);
        CFont* pOldFont = pDC->SelectObject(&font);


        if ((lpDrawItemStruct->itemState & ODS_SELECTED) && (lpDrawItemStruct->itemAction & (ODA_SELECT | ODA_DRAWENTIRE))) {
            pDC->FillSolidRect(&rectM, TextSelectColor);
        }

        if (lpDrawItemStruct->itemState & ODS_NOACCEL) { //removing single &s before drawtext
            CString t = menuObject->m_strCaption;
            t.Replace(TEXT("&&"), TEXT("{{amp}}"));
            t.Remove(TEXT('&'));
            t.Replace(TEXT("{{amp}}"), TEXT("&&"));

            pDC->DrawText(t, rectText, DT_VCENTER | captionAlign | DT_SINGLELINE);
        }
        else {
            pDC->DrawText(menuObject->m_strCaption, rectText, DT_VCENTER | captionAlign | DT_SINGLELINE);
        }

        if (!menuObject->isMenubar) {

            if (menuObject->m_strAccel.GetLength() > 0) {
                pDC->DrawText(menuObject->m_strAccel, rectText, DT_VCENTER | DT_RIGHT | DT_SINGLELINE);
            }


            if (mInfo.hSubMenu) {
                CFont sfont;
                CMPCThemeUtil::getFontByFace(sfont, pDC, CMPCTheme::uiSymbolFont, 14, FW_BOLD); //this seems right but explorer has subpixel hints and we don't. why (directdraw)?

                pDC->SelectObject(&sfont);
                pDC->SetTextColor(ArrowColor);
                pDC->DrawText(TEXT(">"), rectArrow, DT_VCENTER | DT_CENTER | DT_SINGLELINE);
            }

            if (mInfo.fState & MFS_CHECKED) {
                CString check;
                int size;
                if (mInfo.fType & MFT_RADIOCHECK) {
                    check = TEXT("\u25CF"); //bullet
                    size = 6;
                }
                else {
                    check = TEXT("\u2714"); //checkmark
                    size = 10;
                }
                CFont bFont;
                CMPCThemeUtil::getFontByFace(bFont, pDC, CMPCTheme::uiSymbolFont, size, FW_REGULAR); //this seems right but explorer has subpixel hints and we don't. why (directdraw)?
                pDC->SelectObject(&bFont);
                pDC->SetTextColor(TextFGColor);
                pDC->DrawText(check, rectIcon, DT_VCENTER | DT_CENTER | DT_SINGLELINE);
            }
        }

        pDC->SetBkMode(oldBKMode);
        pDC->SetTextColor(oldTextFGColor);
        pDC->SelectObject(pOldFont);
    }
    ExcludeClipRect(lpDrawItemStruct->hDC, rectFull.left, rectFull.top, rectFull.right, rectFull.bottom);
}

void CMPCThemeMenu::MeasureItem(LPMEASUREITEMSTRUCT lpMeasureItemStruct) {
    initDimensions();

    HDC hDC = ::GetDC(NULL);
    MenuObject* mo = (MenuObject*)lpMeasureItemStruct->itemData;

    if (mo->isSeparator) {
        lpMeasureItemStruct->itemWidth = 0;
        lpMeasureItemStruct->itemHeight = separatorHeight;
    } else {
        if (mo->isMenubar) {
            CSize cs = CMPCThemeUtil::GetTextSize(mo->m_strCaption, hDC, CMPCThemeUtil::MenuFont);
            lpMeasureItemStruct->itemWidth = cs.cx;
            lpMeasureItemStruct->itemHeight = cs.cy + rowPadding;
        } else {
            CSize cs = CMPCThemeUtil::GetTextSize(mo->m_strCaption, hDC, CMPCThemeUtil::MenuFont);
            lpMeasureItemStruct->itemHeight = cs.cy + rowPadding;
            lpMeasureItemStruct->itemWidth = iconSpacing + postTextSpacing + subMenuPadding + cs.cx;
            if (mo->m_strAccel.GetLength() > 0) {
                CSize csAccel = CMPCThemeUtil::GetTextSize(mo->m_strAccel, hDC, CMPCThemeUtil::MenuFont);
                lpMeasureItemStruct->itemWidth += accelSpacing + csAccel.cx;
            }
        }
    }
}

CMPCThemeMenu* CMPCThemeMenu::GetSubMenu(int nPos) {
    return (CMPCThemeMenu*) CMenu::GetSubMenu(nPos);
}

void CMPCThemeMenu::updateItem(CCmdUI* pCmdUI) {
    CMenu* cm = pCmdUI->m_pMenu;

    if (DYNAMIC_DOWNCAST(CMPCThemeMenu, cm)) {
        MENUITEMINFO mInfo;
        ZeroMemory(&mInfo, sizeof(MENUITEMINFO));

        mInfo.fMask = MIIM_DATA;
        mInfo.cbSize = sizeof(MENUITEMINFO);
        VERIFY(cm->GetMenuItemInfo(pCmdUI->m_nID, &mInfo));
        
        MenuObject* menuObject = (MenuObject*)mInfo.dwItemData;
        cm->GetMenuString(pCmdUI->m_nID, menuObject->m_strCaption, MF_BYCOMMAND);
    }
}

