// message.cpp
// 对话/窗口消息处理函数实现文件

#include "MindMapData.h"
#include "framework.h"
#include "MINDTREE.h"
#include "controls.h"
#include "message.h"
#include "resolve.h"
#include "MindMapManager.h"
#include "canvas_impl.h" // Changed from canvas_stub.h to canvas_impl.h
#include "history_system.h"
#include "borderless_button.h"
#include "commands.h" // Ensure commands.h is included
#include <functional> // Added for std::function
#include <vector>
#include <string>
#include <set>
#include <algorithm>
#include <map>
#include <windowsx.h> // For GET_X_LPARAM

#pragma comment(lib, "comctl32.lib") // Link against comctl32.lib for subclassing functions

// Global font list
std::vector<std::wstring> g_sysFonts;
std::vector<LPCWSTR> g_fontList;

int CALLBACK EnumFontFamExProc(const LOGFONT* lpelfe, const TEXTMETRIC* lpntme, DWORD FontType, LPARAM lParam) {
    std::set<std::wstring>* fonts = (std::set<std::wstring>*)lParam;
    if (lpelfe->lfFaceName[0] != L'@') {
        // Check if name contains non-ASCII characters (exclude English names)
        bool hasNonAscii = false;
        for (const wchar_t* p = lpelfe->lfFaceName; *p; ++p) {
            if (*p > 127) {
                hasNonAscii = true;
                break;
            }
        }
        if (hasNonAscii) {
            fonts->insert(lpelfe->lfFaceName);
        }
    }
    return 1;
}

void ReloadFontList() {
    std::set<std::wstring> fonts;
    HDC hdc = GetDC(NULL);
    LOGFONT lf = { 0 };
    lf.lfCharSet = DEFAULT_CHARSET;
    EnumFontFamiliesEx(hdc, &lf, (FONTENUMPROC)EnumFontFamExProc, (LPARAM)&fonts, 0);
    ReleaseDC(NULL, hdc);

    g_sysFonts.assign(fonts.begin(), fonts.end());
    
    g_fontList.clear();
    g_fontList.push_back(L"默认字体");
    
    if (!g_sysFonts.empty()) {
        for (const auto& f : g_sysFonts) {
            g_fontList.push_back(f.c_str());
        }
    }
    
    g_fontList.push_back(L"其他字体……");
    g_fontList.push_back(NULL);
}

// Subclass procedure for panels (W_STYLE, W_PAINT) to forward messages to parent
LRESULT CALLBACK PanelSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
    switch (uMsg) {
        case WM_COMMAND:
        case WM_NOTIFY:
        case WM_VSCROLL: 
            // Forward these messages to the parent (StyleWndproc)
            return SendMessage(GetParent(hWnd), uMsg, wParam, lParam);
        case WM_DESTROY:
            RemoveWindowSubclass(hWnd, PanelSubclassProc, uIdSubclass);
            break;
    }
    return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

// Files Tab Subclass
std::map<int, HWND> g_tabCloseButtons;
std::map<int, HWND> g_clothCloseButtons; // Added for CLOTH
HWND g_tabEdit = NULL;
WNDPROC g_oldTabEditProc = NULL;
int g_editingTabIndex = -1;
HWND g_editingTabCtrl = NULL; // Track which tab control is being edited

void UpdateTabButtons(HWND hTab, std::map<int, HWND>& btnMap) {
    int count = TabCtrl_GetItemCount(hTab);
    
    // Remove extra buttons
    auto it = btnMap.begin();
    while (it != btnMap.end()) {
        if (it->first >= count) {
            DestroyWindow(it->second);
            it = btnMap.erase(it);
        } else {
            ++it;
        }
    }

    for (int i = 0; i < count; ++i) {
        RECT rect;
        TabCtrl_GetItemRect(hTab, i, &rect);
        
        int btnSize = 16;
        int x = rect.right - btnSize - 4; 
        int y = rect.top + (rect.bottom - rect.top - btnSize) / 2;
        
        HWND hBtn = NULL;
        if (btnMap.find(i) == btnMap.end()) {
            HICON hIconClose = (HICON)LoadImage(global::hInst, MAKEINTRESOURCE(IDI_close1616), IMAGE_ICON, 12, 12, LR_DEFAULTCOLOR);
            if (!hIconClose) hIconClose = (HICON)LoadImage(global::hInst, L"icon/close.ico", IMAGE_ICON, 12, 12, LR_LOADFROMFILE);
            
            BORDERLESS_BUTTON_CONFIG config = {0};
            config.iconSize = 10;
            config.bgColorNormal = GetSysColor(COLOR_BTNFACE); 
            config.cornerRadius = 0; 
            
            hBtn = CreateBorderlessButton(hTab, global::hInst, x, y, btnSize, btnSize, NULL, hIconClose, IDM_TAB_CLOSE, config);
            btnMap[i] = hBtn;
        } else {
            hBtn = btnMap[i];
            SetWindowPos(hBtn, HWND_TOP, x, y, btnSize, btnSize, SWP_NOACTIVATE | SWP_SHOWWINDOW);
        }
    }
}

LRESULT CALLBACK TabEditProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (uMsg == WM_KEYDOWN && wParam == VK_RETURN) {
        SetFocus(GetParent(hWnd)); 
        return 0;
    }
    if (uMsg == WM_KILLFOCUS) {
        WCHAR buf[256];
        GetWindowText(hWnd, buf, 256);
        if (g_editingTabIndex >= 0 && g_editingTabCtrl) {
            std::wstring newTitle = buf;
            
            if (g_editingTabCtrl == global::FILES) {
                MindMapManager::Instance().RenamePage(g_editingTabIndex, newTitle);
                // Get updated title (RenamePage might have added extension)
                newTitle = MindMapManager::Instance().GetPageTitle(g_editingTabIndex);
            } else if (g_editingTabCtrl == global::CLOTH) {
                MindMapManager::Instance().SetPaintTitle(g_editingTabIndex, newTitle);
            }
            
            TCITEM tie;
            tie.mask = TCIF_TEXT;
            std::wstring displayTitle = newTitle + L"      "; 
            tie.pszText = (LPWSTR)displayTitle.c_str();
            TabCtrl_SetItem(g_editingTabCtrl, g_editingTabIndex, &tie);
            
            if (g_editingTabCtrl == global::FILES) UpdateTabButtons(g_editingTabCtrl, g_tabCloseButtons);
            else UpdateTabButtons(g_editingTabCtrl, g_clothCloseButtons);
        }
        DestroyWindow(hWnd);
        g_tabEdit = NULL;
        g_editingTabIndex = -1;
        g_editingTabCtrl = NULL;
        return 0;
    }
    return CallWindowProc(g_oldTabEditProc, hWnd, uMsg, wParam, lParam);
}

LRESULT CALLBACK FilesTabSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
    switch (uMsg) {
        case WM_PAINT: {
            LRESULT res = DefSubclassProc(hWnd, uMsg, wParam, lParam);
            UpdateTabButtons(hWnd, g_tabCloseButtons);
            return res;
        }
        case WM_SIZE:
        case WM_WINDOWPOSCHANGED:
            UpdateTabButtons(hWnd, g_tabCloseButtons);
            break;
            
        case WM_COMMAND: {
            if (LOWORD(wParam) == IDM_TAB_CLOSE) {
                HWND hBtn = (HWND)lParam;
                int tabIndex = -1;
                for (auto& pair : g_tabCloseButtons) {
                    if (pair.second == hBtn) {
                        tabIndex = pair.first;
                        break;
                    }
                }
                
                if (tabIndex != -1) {
                    if (MessageBox(global::HOME, L"Are you sure you want to close this page?", L"Confirm Close", MB_YESNO | MB_ICONQUESTION) == IDYES) {
                        MindMapManager::Instance().DeletePage(tabIndex);
                        TabCtrl_DeleteItem(hWnd, tabIndex);
                        
                        // Rebuild buttons map to avoid index mismatch
                        for (auto& pair : g_tabCloseButtons) DestroyWindow(pair.second);
                        g_tabCloseButtons.clear();
                        UpdateTabButtons(hWnd, g_tabCloseButtons);
                        
                        int cur = MindMapManager::Instance().GetCurrentPageIndex();
                        TabCtrl_SetCurSel(hWnd, cur);
                    }
                }
                return 0;
            }
            break;
        }
        
        case WM_LBUTTONDBLCLK: {
            TCHITTESTINFO hti;
            hti.pt.x = GET_X_LPARAM(lParam);
            hti.pt.y = GET_Y_LPARAM(lParam);
            int index = TabCtrl_HitTest(hWnd, &hti);
            if (index != -1) {
                RECT rect;
                TabCtrl_GetItemRect(hWnd, index, &rect);
                rect.right -= 20;
                
                g_editingTabIndex = index;
                g_editingTabCtrl = hWnd;
                std::wstring currentTitle = MindMapManager::Instance().GetPageTitle(index);
                
                g_tabEdit = CreateWindow(L"EDIT", currentTitle.c_str(), WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, 
                    rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, hWnd, NULL, global::hInst, NULL);
                SendMessage(g_tabEdit, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);
                SetFocus(g_tabEdit);
                SendMessage(g_tabEdit, EM_SETSEL, 0, -1);
                
                g_oldTabEditProc = (WNDPROC)SetWindowLongPtr(g_tabEdit, GWLP_WNDPROC, (LONG_PTR)TabEditProc);
            }
            return 0;
        }
        
        case WM_DESTROY:
            RemoveWindowSubclass(hWnd, FilesTabSubclassProc, uIdSubclass);
            break;
    }
    return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

LRESULT CALLBACK ClothTabSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
    switch (uMsg) {
        case WM_PAINT: {
            LRESULT res = DefSubclassProc(hWnd, uMsg, wParam, lParam);
            UpdateTabButtons(hWnd, g_clothCloseButtons);
            return res;
        }
        case WM_SIZE:
        case WM_WINDOWPOSCHANGED:
            UpdateTabButtons(hWnd, g_clothCloseButtons);
            break;
            
        case WM_COMMAND: {
            if (LOWORD(wParam) == IDM_TAB_CLOSE) {
                HWND hBtn = (HWND)lParam;
                int tabIndex = -1;
                for (auto& pair : g_clothCloseButtons) {
                    if (pair.second == hBtn) {
                        tabIndex = pair.first;
                        break;
                    }
                }
                
                if (tabIndex != -1) {
                    if (MessageBox(global::HOME, L"Are you sure you want to close this paint?", L"Confirm Close", MB_YESNO | MB_ICONQUESTION) == IDYES) {
                        MindMapManager::Instance().DeletePaint(tabIndex);
                        TabCtrl_DeleteItem(hWnd, tabIndex);
                        
                        // Rebuild buttons map
                        for (auto& pair : g_clothCloseButtons) DestroyWindow(pair.second);
                        g_clothCloseButtons.clear();
                        UpdateTabButtons(hWnd, g_clothCloseButtons);
                        
                        // Current selection is handled by DeletePaint calling SwitchToPaint, but we need to update UI selection
                        // Actually DeletePaint calls SwitchToPaint which updates UI, but TabCtrl_DeleteItem might shift indices.
                        // We should re-sync selection.
                        // MindMapManager::DeletePaint updates activePaintIndex.
                        // But we need to get it.
                        // Let's just assume the manager handles the logic and we just need to reflect it.
                        // Wait, DeletePaint calls SwitchToPaint which calls TabCtrl_SetCurSel.
                        // So we might be fine.
                    }
                }
                return 0;
            }
            break;
        }
        
        case WM_LBUTTONDBLCLK: {
            TCHITTESTINFO hti;
            hti.pt.x = GET_X_LPARAM(lParam);
            hti.pt.y = GET_Y_LPARAM(lParam);
            int index = TabCtrl_HitTest(hWnd, &hti);
            if (index != -1) {
                RECT rect;
                TabCtrl_GetItemRect(hWnd, index, &rect);
                rect.right -= 20;
                
                g_editingTabIndex = index;
                g_editingTabCtrl = hWnd;
                std::wstring currentTitle = MindMapManager::Instance().GetPaintTitle(index);
                
                g_tabEdit = CreateWindow(L"EDIT", currentTitle.c_str(), WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, 
                    rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, hWnd, NULL, global::hInst, NULL);
                SendMessage(g_tabEdit, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);
                SetFocus(g_tabEdit);
                SendMessage(g_tabEdit, EM_SETSEL, 0, -1);
                
                g_oldTabEditProc = (WNDPROC)SetWindowLongPtr(g_tabEdit, GWLP_WNDPROC, (LONG_PTR)TabEditProc);
            }
            return 0;
        }
        
        case WM_DESTROY:
            RemoveWindowSubclass(hWnd, ClothTabSubclassProc, uIdSubclass);
            break;
    }
    return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

// Helper to apply style to selected nodes
static void ApplyStyleToSelectedNodes(std::function<void(MindNode*)> applyFunc) {
    auto& manager = MindMapManager::Instance();
    const auto& selected = manager.GetSelectedNodes();
    bool applied = false;
    MindNode* lastNode = nullptr; // Track last modified node for UI update

    if (!selected.empty()) {
        for (MindNode* node : selected) {
            applyFunc(node);
            Canvas_CalculateNodeSize(node);
            lastNode = node;
        }
        applied = true;
    } else if (global::G_TREEVIEW) {
        HTREEITEM hSel = TreeView_GetSelection(global::G_TREEVIEW);
        if (hSel) {
            MindNode* node = manager.GetNodeFromHandle(hSel);
            if (node) {
                applyFunc(node);
                Canvas_CalculateNodeSize(node);
                lastNode = node;
                applied = true;
            }
        }
    }
    if (applied) {
        Canvas_Invalidate();
        if (lastNode) {
            UpdateStylePanelUI(lastNode); // Update UI to reflect changes (e.g. button colors)
        }
    }
}

// Helper to pick color
static std::wstring PickColor(HWND owner) {
    CHOOSECOLOR cc = {0};
    static COLORREF customColors[16] = {0};
    cc.lStructSize = sizeof(cc);
    cc.hwndOwner = owner;
    cc.lpCustColors = customColors;
    cc.Flags = CC_FULLOPEN | CC_RGBINIT;
    if (ChooseColor(&cc)) {
        wchar_t buf[16];
        swprintf_s(buf, L"#%02X%02X%02X", GetRValue(cc.rgbResult), GetGValue(cc.rgbResult), GetBValue(cc.rgbResult));
        return buf;
    }
    return L"";
}

// Helper to get color from index
static std::wstring GetColorFromIndex(int idx, HWND owner) {
    switch(idx) {
        case 0: return L"#000000";
        case 1: return L"#FFFFFF";
        case 2: return L"#D3D3D3";
        case 3: return L"#FF0000";
        case 4: return L"#FFFF00";
        case 5: return PickColor(owner);
        default: return L"";
    }
}

// Reverse helpers for UI update
static int GetShapeIndex(const std::wstring& shape) {
    if (shape == L"rounded_rectangle") return 0;
    if (shape == L"rectangle") return 1;
    if (shape == L"ellipse") return 2;
    if (shape == L"circle") return 3;
    return -1;
}

static int GetColorIndex(const std::wstring& color) {
    if (color == L"#000000") return 0;
    if (color == L"#FFFFFF") return 1;
    if (color == L"#D3D3D3") return 2;
    if (color == L"#FF0000") return 3;
    if (color == L"#FFFF00") return 4;
    return 5; // Custom
}

static int GetFrameStyleIndex(const std::wstring& style) {
    if (style == L"solid") return 0;
    if (style == L"dashed") return 1;
    if (style == L"dotted") return 2;
    return -1;
}

static int GetAlignIndex(const std::wstring& align) {
    if (align == L"left") return 0;
    if (align == L"center") return 1;
    if (align == L"right") return 2;
    return -1;
}

static int GetStructureIndex(const std::wstring& struc) {
    if (struc == L"mindmap") return 0;
    if (struc == L"tree") return 1;
    if (struc == L"bracket") return 2;
    if (struc == L"fishbone") return 3;
    return -1;
}

void UpdateStylePanelUI(MindNode* node) {
    if (!node || !global::W_STYLE) return;

    // Shape
    int shapeIdx = GetShapeIndex(node->style.shape);
    if (shapeIdx != -1) SendDlgItemMessage(global::W_STYLE, IDM_SHAPE, CB_SETCURSEL, shapeIdx, 0);

    // Fill Color
    int fillIdx = GetColorIndex(node->style.fillColor);
    SendDlgItemMessage(global::W_STYLE, IDM_FILL, CB_SETCURSEL, fillIdx, 0);

    // Frame Style
    int frameIdx = GetFrameStyleIndex(node->style.borderStyle);
    if (frameIdx != -1) SendDlgItemMessage(global::W_STYLE, IDM_FRAME, CB_SETCURSEL, frameIdx, 0);

    // Frame Width
    if (GetDlgItemInt(global::W_STYLE, IDM_FRAMETHICK, NULL, FALSE) != node->style.borderWidth) {
        SetDlgItemInt(global::W_STYLE, IDM_FRAMETHICK, node->style.borderWidth, FALSE);
    }

    // Frame Color
    int frameColorIdx = GetColorIndex(node->style.borderColor);
    SendDlgItemMessage(global::W_STYLE, IDM_FRAMECOLOR, CB_SETCURSEL, frameColorIdx, 0);

    // Font Family
    int fontIdx = CB_ERR;
    if (node->style.fontFamily == L"Arial" || node->style.fontFamily.empty()) {
        fontIdx = 0;
    } else {
        fontIdx = SendDlgItemMessage(global::W_STYLE, IDM_FONT, CB_FINDSTRINGEXACT, -1, (LPARAM)node->style.fontFamily.c_str());
    }

    if (fontIdx != CB_ERR) {
        SendDlgItemMessage(global::W_STYLE, IDM_FONT, CB_SETCURSEL, fontIdx, 0);
    } else {
        SendDlgItemMessage(global::W_STYLE, IDM_FONT, CB_SETCURSEL, 0, 0);
    }

    // Font Size
    if (GetDlgItemInt(global::W_STYLE, IDM_TEXTSIZE, NULL, FALSE) != node->style.fontSize) {
        SetDlgItemInt(global::W_STYLE, IDM_TEXTSIZE, node->style.fontSize, FALSE);
    }

    // Text Color
    int textColorIdx = GetColorIndex(node->style.textColor);
    SendDlgItemMessage(global::W_STYLE, IDM_TEXTCOLOR, CB_SETCURSEL, textColorIdx, 0);

    // Update Bold/Italic/Underline buttons state (visual style)
    HWND hBold = GetDlgItem(global::W_STYLE, IDM_BOLD);
    HWND hItalic = GetDlgItem(global::W_STYLE, IDM_ITALIC);
    HWND hUnderline = GetDlgItem(global::W_STYLE, IDM_UNDERLINE);

    COLORREF activeBg = RGB(180, 180, 180); // Darker for active
    COLORREF normalBg = RGB(240, 240, 240); // Default
    COLORREF borderCol = RGB(200, 200, 200);

    if (hBold) {
        BorderlessButtonSetColors(hBold, node->style.bold ? activeBg : normalBg, borderCol, BBS_NORMAL);
    }
    if (hItalic) {
        BorderlessButtonSetColors(hItalic, node->style.italic ? activeBg : normalBg, borderCol, BBS_NORMAL);
    }
    if (hUnderline) {
        BorderlessButtonSetColors(hUnderline, node->style.underline ? activeBg : normalBg, borderCol, BBS_NORMAL);
    }

    // Text Align
    int alignIdx = GetAlignIndex(node->style.textAlignment);
    if (alignIdx != -1) SendDlgItemMessage(global::W_STYLE, IDM_TEXTALIGN, CB_SETCURSEL, alignIdx, 0);

    // Branch Width
    if (GetDlgItemInt(global::W_STYLE, IDM_BRANCH_THICK, NULL, FALSE) != node->style.branchWidth) {
        SetDlgItemInt(global::W_STYLE, IDM_BRANCH_THICK, node->style.branchWidth, FALSE);
    }

    // Branch Color
    int branchColorIdx = GetColorIndex(node->style.branchColor);
    SendDlgItemMessage(global::W_STYLE, IDM_BRANCH_COLOR, CB_SETCURSEL, branchColorIdx, 0);

    // Structure
    int strucIdx = GetStructureIndex(node->style.nodeStructure);
    if (strucIdx != -1) SendDlgItemMessage(global::W_STYLE, IDM_STRUCTURE, CB_SETCURSEL, strucIdx, 0);
}

// 各控件选项文本
LPCWSTR shapestr[] = { L"圆角矩形",L"矩形",L"椭圆",L"圆形", NULL };
LPCWSTR framestr[] = { L"实线",L"长虚线",L"短虚线",NULL };
LPCWSTR colorstr[] = { L"黑色",L"白色",L"浅灰",L"红色",L"黄色",L"自定义" ,NULL };
LPCWSTR alignstr[] = { L"左对齐",L"居中对齐",L"右对齐" ,NULL };
LPCWSTR structureStr[] = { L"思维导图", L"树状图", L"括号图", L"鱼骨图", NULL };
LPCWSTR presetStr[] = { L"思维导图", L"树状图", L"括号图", L"鱼骨图",NULL };

// 计算字符串数组长度
int lengthof(LPCWSTR* str) {
    int counter = 0;
    while (str[counter] != NULL) {
        counter++;
    }
    return counter;
}

// “关于”对话框消息处理函数
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}

// TOOLS:普通子窗口过程
LRESULT CALLBACK ToolsWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    RECT home;
    GetClientRect(global::HOME, &home);

    switch (message)
    {
    case WM_CREATE:
    {
        // 在创建时获取控件区域并创建子控件（等同于原 WM_INITDIALOG）
        GetClientRect(hWnd, &global::tools);
        if (global::SUB == NULL) {
            int width = global::tools.right - global::tools.left;
            int height = global::tools.bottom - global::tools.top;
            // 创建工具按钮：子节点、父节点、兄弟节点、返回、关闭
            HICON hIconSub = (HICON)LoadImage(global::hInst, L"icon/sub.ico", IMAGE_ICON, 0, 0, LR_LOADFROMFILE | LR_DEFAULTSIZE);
            global::SUB = CreateNoborderButton(hWnd, global::hInst, width / 9 * 3 - 16, (height - 20) / 2 - 16, 32, 32, hIconSub, IDM_SUB);
            HICON hIconParent = (HICON)LoadImage(global::hInst, L"icon/parent.ico", IMAGE_ICON, 0, 0, LR_LOADFROMFILE | LR_DEFAULTSIZE);
            global::PARENT = CreateNoborderButton(hWnd, global::hInst, width / 9 * 4 - 16, (height - 20) / 2 - 16, 32, 32, hIconParent, IDM_PARENT);
            HICON hIconMate = (HICON)LoadImage(global::hInst, L"icon/mate.ico", IMAGE_ICON, 0, 0, LR_LOADFROMFILE | LR_DEFAULTSIZE);
            global::MATE = CreateNoborderButton(hWnd, global::hInst, width / 9 * 5 - 16, (height - 20) / 2 - 16, 32, 32, hIconMate, IDM_WORKMATE);
            HICON hIconReturn = (HICON)LoadImage(global::hInst, L"icon/returntoroot.ico", IMAGE_ICON, 0, 0, LR_LOADFROMFILE | LR_DEFAULTSIZE);
            global::RETURN = CreateNoborderButton(hWnd, global::hInst, width / 9 * 6 - 16, (height - 20) / 2 - 16, 32, 32, hIconReturn, IDM_RETURN);
            HICON Iup = (HICON)LoadImage(global::hInst, L"icon/parent.ico", IMAGE_ICON, 0, 0, LR_LOADFROMFILE | LR_DEFAULTSIZE);
            global::CLOSETOOLS = CreateNoborderButton(hWnd, global::hInst, 50, 0, width, 20, Iup, IDM_CLOSETOOLS);
            HICON Idown = (HICON)LoadImage(global::hInst, L"icon/sub.ico", IMAGE_ICON, 0, 0, LR_LOADFROMFILE | LR_DEFAULTSIZE);
            global::OPENTOOLS = CreateNoborderButton(hWnd, global::hInst, 0, 0, width, 20, Idown, IDM_OPENTOOLS);
            ShowWindow(global::OPENTOOLS, SW_HIDE);
            HICON hIconRight = (HICON)LoadImage(global::hInst, L"icon/right.ico", IMAGE_ICON, 0, 0, LR_LOADFROMFILE | LR_DEFAULTSIZE);
            global::RIGHT = CreateNoborderButton(hWnd, global::hInst, width - 36, (height - 20) / 2 - 16, 32, 32, hIconRight, IDM_EDITSTYLE);
        }
    }
    return 0;

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDM_CLOSETOOLS:
            SetWindowPos(hWnd, HWND_TOP, 0, 0, home.right, 20, SWP_NOMOVE);
            ShowWindow(global::OPENTOOLS, SW_SHOW);
            ShowWindow(global::CLOSETOOLS, SW_HIDE);
            ShowWindow(global::SUB, SW_HIDE);
            ShowWindow(global::PARENT, SW_HIDE);
            ShowWindow(global::MATE, SW_HIDE);
            ShowWindow(global::RETURN, SW_HIDE);
            ShowWindow(global::RIGHT, SW_HIDE);
            Setguide(home);
            Setstyle(home);
            SetFILES(home);
            SetCanvas(home); // Added SetCanvas call
            return 0;
        case IDM_OPENTOOLS:
            Ftools(home);
            ShowWindow(global::OPENTOOLS, SW_HIDE);
            ShowWindow(global::CLOSETOOLS, SW_SHOW);
            ShowWindow(global::SUB, SW_SHOW);
            ShowWindow(global::PARENT, SW_SHOW);
            ShowWindow(global::MATE, SW_SHOW);
            ShowWindow(global::RETURN, SW_SHOW);
            ShowWindow(global::RIGHT, SW_SHOW);
            Setguide(home);
            Setstyle(home);
            SetFILES(home);
            SetCanvas(home); // Added SetCanvas call
            return 0;
        case IDM_SUB:
        {
            HistorySystem::RecordScope rec([] { return MindMapManager::Instance().CreateSnapshot(); });
            MindMapManager::Instance().AddSubNodeToSelected();
            Canvas_Invalidate();
        }
        return 0;
        case IDM_PARENT:
        {
            HistorySystem::RecordScope rec([] { return MindMapManager::Instance().CreateSnapshot(); });
            MindMapManager::Instance().AddParentNodeToSelected();
            Canvas_Invalidate();
        }
        return 0;
        case IDM_WORKMATE:
        {
            HistorySystem::RecordScope rec([] { return MindMapManager::Instance().CreateSnapshot(); });
            MindMapManager::Instance().AddSiblingNodeToSelected();
            Canvas_Invalidate();
        }
        return 0;
        case IDM_RETURN:
            if (global::G_TREEVIEW) {
                HTREEITEM hRoot = TreeView_GetRoot(global::G_TREEVIEW);
                if (hRoot) {
                    TreeView_SelectItem(global::G_TREEVIEW, hRoot);
                    SetFocus(global::G_TREEVIEW);
                }
            }
            return 0;
        case IDM_EDITSTYLE:
            if (!IsWindowVisible(global::STYLE)) {
                Setstyle(home);
                ShowWindow(global::STYLE, SW_NORMAL);
                UpdateWindow(global::STYLE);
            }
            else {
                ShowWindow(global::STYLE, SW_HIDE);
            }
            SetFILES(home);
            SetCLOTH(home);
            return 0;
        }
        break;
    }

    return DefWindowProc(hWnd, message, wParam, lParam);
}

// “说明”对话框消息处理函数
INT_PTR CALLBACK Explain(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDC_expclose)
        {
            ShowWindow(hDlg, SW_HIDE);
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}

// 辅助：在导航树上双击时展示节点完整信息
static void ShowNodeInfo(HWND parent, MindNode* node) {
    if (!node) return;
    std::wstringstream ss;
    ss << L"ID: " << node->id << L"\n";
    ss << L"标题: " << node->text << L"\n";
    ss << L"标签: " << node->label << L"\n";
    ss << L"注解: " << node->annotation << L"\n";
    ss << L"笔记: " << node->note << L"\n";
    ss << L"概要: " << node->summary << L"\n";
    ss << L"外链(URL): " << node->outlinkURL << L"\n";
    ss << L"外链文件: " << node->outlinkFile << L"\n";
    ss << L"外链页面: " << node->outlinkPage << L"\n";
    ss << L"框架: " << node->frameStyle << L"\n";
    ss << L"画布: " << node->paintPath << L"\n";

    // Display linked nodes
    if (!node->linkedNodeIds.empty()) {
        ss << L"关联节点: "; for (size_t i = 0; i < node->linkedNodeIds.size(); ++i) { ss << node->linkedNodeIds[i]; if (i < node->linkedNodeIds.size() - 1) ss << L", "; }ss << L"\\n";
    }MessageBox(parent, ss.str().c_str(), L"节点信息", MB_OK);
}

// “指南”对话框消息处理函数
LRESULT CALLBACK GuideWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_CREATE:
    {
        GetClientRect(hWnd, &global::guide);
        // 创建指南页面切换按钮：主题、笔记、标签
        global::G_TAB = CreateWindowEx(0, WC_TABCONTROL, L"", WS_VISIBLE | WS_CHILD | TCS_TABS, 0, 0, global::guide.right - 20, 20, hWnd, NULL, global::hInst, NULL);
        TCITEM tieT = {};
        tieT.mask = TCIF_TEXT;
        tieT.pszText = (LPWSTR)L"主题";
        TabCtrl_InsertItem(global::G_TAB, 0, &tieT);
        TCITEM tieN = {};
        tieN.mask = TCIF_TEXT;
        tieN.pszText = (LPWSTR)L"笔记";
        TabCtrl_InsertItem(global::G_TAB, 1, &tieN);
        TCITEM tieL = {};
        tieL.mask = TCIF_TEXT;
        tieL.pszText = (LPWSTR)L"标签";
        TabCtrl_InsertItem(global::G_TAB, 2, &tieL);
        HICON hIconToLeft = (HICON)LoadImage(global::hInst, L"icon/toleft.ico", IMAGE_ICON, 0, 0, LR_LOADFROMFILE | LR_DEFAULTSIZE);
        global::G_CLOSE = CreateNoborderButton(hWnd, global::hInst, global::guide.right - 20, 0, 20, global::guide.bottom, hIconToLeft, IDM_CLOSEGUIDE);
        // 创建树形视图用于显示内容结构
        global::G_TREEVIEW = CreateContentTree(hWnd, global::hInst, IDC_treeview, 0, 20, global::guide.right - global::guide.left - 20, global::guide.bottom - global::guide.top - 20);

        // 初始化根节点
        MindMapManager::Instance().CreateRootUI();
    }

    return 0;

    case WM_NOTIFY: {
        LPNMHDR pnmh = (LPNMHDR)lParam;
        if (pnmh->idFrom == IDC_treeview) {
            if (pnmh->code == NM_DBLCLK) {
                // 命中测试获取双击的项
                POINT pt; GetCursorPos(&pt);
                ScreenToClient(pnmh->hwndFrom, &pt);
                TVHITTESTINFO ht{};
                ht.pt = pt;
                HTREEITEM hItem = TreeView_HitTest(pnmh->hwndFrom, &ht);
                if (hItem) {
                    TreeView_SelectItem(pnmh->hwndFrom, hItem);
                    MindNode* node = MindMapManager::Instance().GetNodeFromHandle(hItem);
                    if (node) {
                        ShowNodeInfo(hWnd, node);
                    }
                }
                return 0;
            }
            else if (pnmh->code == TVN_SELCHANGEDW || pnmh->code == TVN_SELCHANGEDA) {
                // 选中项改变时，刷新画布以同步高亮
                Canvas_Invalidate();
                
                // Update Style Panel
                HTREEITEM hSel = TreeView_GetSelection(global::G_TREEVIEW);
                if (hSel) {
                    MindNode* node = MindMapManager::Instance().GetNodeFromHandle(hSel);
                    if (node) {
                        UpdateStylePanelUI(node);
                    }
                }
            }
        }
        NMHDR* nmhdr = (NMHDR*)lParam;

        if (nmhdr->hwndFrom == global::G_TAB &&
            nmhdr->code == TCN_SELCHANGE)
        {
            int selected = TabCtrl_GetCurSel(global::G_TAB);

            switch (selected) {
                case 0:
                    SendMessageW(hWnd, WM_COMMAND, IDM_GUIDETOPIC, 0);
                    break;
                case 1:
                    SendMessageW(hWnd, WM_COMMAND, IDM_GUIDENOTE, 0);
                    break;
                case 2:
                    SendMessageW(hWnd, WM_COMMAND, IDM_GUIDELABEL, 0);
                    break;
            }
        }
        break;
    }

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDM_GUIDETOPIC:
            Fguidetopic();
            return 0;
        case IDM_GUIDENOTE:
            Fguidenote();
            return 0;
        case IDM_GUIDELABEL:
            Fguidelabel();
            return 0;
        case IDM_CLOSEGUIDE:
            RECT home;
            GetClientRect(global::HOME, &home);
            ShowWindow(hWnd, SW_HIDE);
            SetFILES(home);
            SetCLOTH(home);
            SetCanvas(home); // Added SetCanvas call
            return 0;
        }
        break;
    }
    return DefWindowProc(hWnd, message, wParam, lParam);
}

// “样式”对话框消息处理函数
LRESULT CALLBACK StyleWndproc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    int yPos = 0;

    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_CREATE:
    {
        GetClientRect(hWnd, &global::style);

        // 创建样式对话框的顶部按钮：关闭、样式编辑、绘图设置
        HICON hIconToRight = (HICON)LoadImage(global::hInst, L"icon/toright.ico", IMAGE_ICON, 0, 0, LR_LOADFROMFILE | LR_DEFAULTSIZE);
        global::S_CLOSE = CreateNoborderButton(hWnd, global::hInst, 0, 20, 20, global::style.bottom - 20, hIconToRight, IDM_CLOSESTYLE);
        global::S_TAB = CreateWindowEx(0, WC_TABCONTROL, L"", WS_VISIBLE | WS_CHILD | TCS_TABS, 20, 0, global::style.right, 20, hWnd, NULL, global::hInst, NULL);
        TCITEM tieS = {};
        tieS.mask = TCIF_TEXT;
        tieS.pszText = (LPWSTR)L"样式";
        TabCtrl_InsertItem(global::S_TAB, 0, &tieS);
        TCITEM tieP = {};
        tieP.mask = TCIF_TEXT;
        tieP.pszText = (LPWSTR)L"绘图";
        TabCtrl_InsertItem(global::S_TAB, 1, &tieP);

        // 创建样式编辑面板（带滚动条）
        global::W_STYLE = CreateWindowEx(
            WS_EX_CLIENTEDGE,
            L"STATIC",
            L"",
            WS_VISIBLE | WS_CHILD | WS_VSCROLL,
            20, 20, 200, global::style.bottom,
            hWnd, NULL, global::hInst, NULL);
        
        // Subclass W_STYLE to forward messages
        SetWindowSubclass(global::W_STYLE, PanelSubclassProc, 0, 0);

        // 样式设置部分 - 按顺序创建各控件
        yPos = 10;

        // 形状设置
        CreateText(global::W_STYLE, global::hInst, -1, 10, yPos, 60, 20, L"形状:");
        CreateDropList(global::W_STYLE, global::hInst, IDM_SHAPE, 80, yPos, 80, 200, shapestr, lengthof(shapestr));
        yPos += 30;

        CreateText(global::W_STYLE, global::hInst, -1, 10, yPos, 80, 20, L"填充颜色:");
        CreateDropList(global::W_STYLE, global::hInst, IDM_FILL, 100, yPos, 80, 200, colorstr, lengthof(colorstr));
        yPos += 40;

        // 边框设置
        CreateText(global::W_STYLE, global::hInst, -1, 10, yPos, 60, 20, L"边框:");
        CreateDropList(global::W_STYLE, global::hInst, IDM_FRAME, 80, yPos, 100, 200, framestr, lengthof(framestr));
        yPos += 30;

        CreateText(global::W_STYLE, global::hInst, -1, 10, yPos, 80, 20, L"边框粗细:");
        CreateEditBox(global::W_STYLE, global::hInst, IDM_FRAMETHICK, 100, yPos, 60, 20, L"1");
        yPos += 30;

        CreateText(global::W_STYLE, global::hInst, -1, 10, yPos, 80, 20, L"边框颜色:");
        CreateDropList(global::W_STYLE, global::hInst, IDM_FRAMECOLOR, 100, yPos, 80, 200, colorstr, lengthof(colorstr));
        yPos += 40;

        // 分隔线
        CreateWindow(WC_STATIC, L"", WS_VISIBLE | WS_CHILD | SS_ETCHEDHORZ,
            10, yPos, global::style.right - 20, 2, global::W_STYLE, NULL, global::hInst, NULL);
        yPos += 15;

        // 文字设置
        CreateText(global::W_STYLE, global::hInst, -1, 10, yPos, 60, 20, L"字体:");
        
        // Initialize font list if empty
        if (g_fontList.empty()) ReloadFontList();
        
        CreateDropList(global::W_STYLE, global::hInst, IDM_FONT, 80, yPos, 100, 200, g_fontList.data(), (int)g_fontList.size() - 1);
        yPos += 30;

        CreateText(global::W_STYLE, global::hInst, -1, 10, yPos, 80, 20, L"字体大小:");
        CreateEditBox(global::W_STYLE, global::hInst, IDM_TEXTSIZE, 100, yPos, 50, 20, L"12");
        yPos += 30;

        CreateText(global::W_STYLE, global::hInst, -1, 10, yPos, 80, 20, L"文字颜色:");
        CreateDropList(global::W_STYLE, global::hInst, IDM_TEXTCOLOR, 100, yPos, 80, 200, colorstr, lengthof(colorstr));
        yPos += 30;

        // 文字样式按钮
        // CreateButton(global::W_STYLE, global::hInst, IDM_BOLD, 20, yPos, 40, 25, L"粗体");
        // CreateButton(global::W_STYLE, global::hInst, IDM_ITALIC, 60, yPos, 40, 25, L"斜体");
        // CreateButton(global::W_STYLE, global::hInst, IDM_UNDERLINE, 100, yPos, 60, 25, L"下划线");
        
        BORDERLESS_BUTTON_CONFIG btnConfig = {0};
        btnConfig.borderWidth = 1;
        btnConfig.cornerRadius = 0;
        btnConfig.style = BB_STYLE_NORMAL;
        btnConfig.showText = TRUE;
        btnConfig.bgColorNormal = RGB(240, 240, 240);
        btnConfig.borderColorNormal = RGB(200, 200, 200);
        btnConfig.bgColorHover = RGB(229, 241, 251);
        btnConfig.borderColorHover = RGB(0, 120, 215);
        btnConfig.bgColorPressed = RGB(204, 228, 247);
        btnConfig.borderColorPressed = RGB(0, 84, 153);
        
        CreateBorderlessButton(global::W_STYLE, global::hInst, 20, yPos, 40, 25, L"粗体", NULL, IDM_BOLD, btnConfig);
        CreateBorderlessButton(global::W_STYLE, global::hInst, 60, yPos, 40, 25, L"斜体", NULL, IDM_ITALIC, btnConfig);
        CreateBorderlessButton(global::W_STYLE, global::hInst, 100, yPos, 60, 25, L"下划线", NULL, IDM_UNDERLINE, btnConfig);
        
        yPos += 30;

        // 文字对齐方式
        CreateText(global::W_STYLE, global::hInst, -1, 10, yPos, 80, 20, L"文字对齐:");
        CreateDropList(global::W_STYLE, global::hInst, IDM_TEXTALIGN, 100, yPos, 80, 200, alignstr, lengthof(alignstr));
        yPos += 30;

        // 分隔线
        CreateWindow(WC_STATIC, L"", WS_VISIBLE | WS_CHILD | SS_ETCHEDHORZ,
            10, yPos, global::style.right - 20, 2, global::W_STYLE, NULL, global::hInst, NULL);
        yPos += 15;

        // 分支线设置
        CreateText(global::W_STYLE, global::hInst, -1, 10, yPos, 80, 20, L"分支粗细:");
        CreateEditBox(global::W_STYLE, global::hInst, IDM_BRANCH_THICK, 100, yPos, 50, 20, L"2");
        yPos += 30;

        CreateText(global::W_STYLE, global::hInst, -1, 10, yPos, 80, 20, L"分支颜色:");
        CreateDropList(global::W_STYLE, global::hInst, IDM_BRANCH_COLOR, 100, yPos, 80, 200, colorstr, lengthof(colorstr));
        yPos += 40;

        // 分隔线
        CreateWindow(WC_STATIC, L"", WS_VISIBLE | WS_CHILD | SS_ETCHEDHORZ,
            10, yPos, global::style.right - 20, 2, global::W_STYLE, NULL, global::hInst, NULL);
        yPos += 15;

        // 结构设置
        CreateText(global::W_STYLE, global::hInst, -1, 10, yPos, 60, 20, L"结构:");
        CreateDropList(global::W_STYLE, global::hInst, IDM_STRUCTURE, 80, yPos, 100, 200, structureStr, lengthof(structureStr));

        // 设置滚动条范围
        SetScrollRange(global::W_STYLE, SB_VERT, 0, (global::style.bottom > yPos + 30) ? 0 : global::style.bottom - yPos - 30, FALSE);
        SetScrollPos(global::W_STYLE, SB_VERT, 0, TRUE);

        // 创建绘图设置面板
        global::W_PAINT = CreateWindowEx(
            WS_EX_CLIENTEDGE,
            L"STATIC",
            L"",
            WS_VISIBLE | WS_CHILD,
            20, 20, 200, global::style.bottom,
            hWnd, NULL, global::hInst, NULL);
        ShowWindow(global::W_PAINT, SW_HIDE);
        
        // Subclass W_PAINT to forward messages
        SetWindowSubclass(global::W_PAINT, PanelSubclassProc, 1, 0);

        // 绘图设置部分
        yPos = 10;

        // 预设样式
        CreateText(global::W_PAINT, global::hInst, -1, 10, yPos, 80, 20, L"预设样式:");
        CreateDropList(global::W_PAINT, global::hInst, IDM_PRESET, 100, yPos, 80, 200, presetStr, lengthof(presetStr));
        yPos += 40;

        // 分隔线
        CreateWindow(WC_STATIC, L"", WS_VISIBLE | WS_CHILD | SS_ETCHEDHORZ,
            10, yPos, global::style.right - 20, 2, global::W_PAINT, NULL, global::hInst, NULL);
        yPos += 15;

        // 布局选项
        CreateText(global::W_PAINT, global::hInst, -1, 10, yPos, 80, 20, L"布局选项:");
        yPos += 30;

        CreateCheckBox(global::W_PAINT, global::hInst, IDM_AUTO_BALANCE, 20, yPos, 140, 20, L"自动平衡布局");
        yPos += 30;

        CreateCheckBox(global::W_PAINT, global::hInst, IDM_COMPACT_LAYOUT, 20, yPos, 140, 20, L"紧凑布局");
        yPos += 30;

        CreateCheckBox(global::W_PAINT, global::hInst, IDM_SIBLING_ALIGN, 20, yPos, 140, 20, L"同级节点对齐");

        // 设置当前活动面板为样式面板
        global::S_CURRENT = global::W_STYLE;
    }

    return 0;

    case WM_VSCROLL:
        // 处理垂直滚动条消息
    {
        HWND hScroll = global::S_CURRENT;
        int nScrollCode = LOWORD(wParam);
        SCROLLINFO si;

        si.cbSize = sizeof(SCROLLINFO);
        si.fMask = SIF_ALL;
        GetScrollInfo(hScroll, SB_VERT, &si);

        int nCurPos = si.nPos;

        switch (nScrollCode)
        {
        case SB_LINEUP:        // 向上滚动一行
            si.nPos -= 10;
            break;
        case SB_LINEDOWN:      // 向下滚动一行
            si.nPos += 10;
            break;
        case SB_THUMBTRACK:    // 拖动滑块
            si.nPos = si.nTrackPos;
            break;
        default:
            break;
        }

        // 确保位置在有效范围内
        si.nPos = max(si.nMin, min(si.nPos, si.nMax));

        if (si.nPos != nCurPos)
        {
            SetScrollPos(hScroll, SB_VERT, si.nPos, TRUE);
            ScrollWindow(hScroll, 0, nCurPos - si.nPos, NULL, NULL);
            UpdateWindow(hScroll);
        }
    }
    break;
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDM_CLOSESTYLE:
            ShowWindow(hWnd, SW_HIDE);
            RECT home;
            GetClientRect(global::HOME, &home);
            SetFILES(home);
            SetCLOTH(home);
            SetCanvas(home); // Added SetCanvas call
            return 0;

        case IDM_EDITSTYLE:
            // 切换到样式编辑面板
            ShowWindow(global::W_STYLE, SW_SHOW);
            ShowWindow(global::W_PAINT, SW_HIDE);
            global::S_CURRENT = global::W_STYLE;
            return 0;

        case IDM_EDITPAINT:
            // 切换到绘图设置面板
            ShowWindow(global::W_STYLE, SW_HIDE);
            ShowWindow(global::W_PAINT, SW_SHOW);
            global::S_CURRENT = global::W_PAINT;
            return 0;

            // ========== 样式面板控件命令处理 ==========
        case IDM_SHAPE:
            if (HIWORD(wParam) == CBN_SELCHANGE) {
                int idx = SendMessage((HWND)lParam, CB_GETCURSEL, 0, 0);
                if (idx != CB_ERR) {
                    std::wstring shape;
                    switch(idx) {
                        case 0: shape = L"rounded_rectangle"; break;
                        case 1: shape = L"rectangle"; break;
                        case 2: shape = L"ellipse"; break;
                        case 3: shape = L"circle"; break;
                    }
                    if (!shape.empty()) {
                        ApplyStyleToSelectedNodes([shape](MindNode* n){ n->style.shape = shape; });
                    }
                }
            }
            return 0;

        case IDM_FILL:
            if (HIWORD(wParam) == CBN_SELCHANGE) {
                int idx = SendMessage((HWND)lParam, CB_GETCURSEL, 0, 0);
                if (idx != CB_ERR) {
                    std::wstring color = GetColorFromIndex(idx, hWnd);
                    if (!color.empty()) {
                        ApplyStyleToSelectedNodes([color](MindNode* n){ n->style.fillColor = color; });
                    }
                }
            }
            return 0;

        case IDM_FRAME:
            if (HIWORD(wParam) == CBN_SELCHANGE) {
                int idx = SendMessage((HWND)lParam, CB_GETCURSEL, 0, 0);
                if (idx != CB_ERR) {
                    std::wstring style;
                    switch(idx) {
                        case 0: style = L"solid"; break;
                        case 1: style = L"dashed"; break;
                        case 2: style = L"dotted"; break;
                    }
                    if (!style.empty()) {
                        ApplyStyleToSelectedNodes([style](MindNode* n){ n->style.borderStyle = style; });
                    }
                }
            }
            return 0;

        case IDM_FRAMETHICK:
            if (HIWORD(wParam) == EN_CHANGE) {
                WCHAR buf[16];
                GetWindowText((HWND)lParam, buf, 16);
                try {
                    int val = std::stoi(buf);
                    if (val >= 0) {
                        ApplyStyleToSelectedNodes([val](MindNode* n){ n->style.borderWidth = val; });
                    }
                } catch(...) {}
            }
            return 0;

        case IDM_FRAMECOLOR:
            if (HIWORD(wParam) == CBN_SELCHANGE) {
                int idx = SendMessage((HWND)lParam, CB_GETCURSEL, 0, 0);
                if (idx != CB_ERR) {
                    std::wstring color = GetColorFromIndex(idx, hWnd);
                    if (!color.empty()) {
                        ApplyStyleToSelectedNodes([color](MindNode* n){ n->style.borderColor = color; });
                    }
                }
            }
            return 0;

        case IDM_FONT:
            if (HIWORD(wParam) == CBN_SELCHANGE) {
                int idx = SendMessage((HWND)lParam, CB_GETCURSEL, 0, 0);
                if (idx != CB_ERR) {
                    int count = SendMessage((HWND)lParam, CB_GETCOUNT, 0, 0);
                    if (idx == count - 1) { // "其他字体……"
                         ReloadFontList();
                         SendMessage((HWND)lParam, CB_RESETCONTENT, 0, 0);
                         for (size_t i = 0; i < g_fontList.size() - 1; ++i) {
                             SendMessage((HWND)lParam, CB_ADDSTRING, 0, (LPARAM)g_fontList[i]);
                         }
                         SendMessage((HWND)lParam, CB_SETCURSEL, 0, 0); // Reset selection
                    } else if (idx == 0) {
                        ApplyStyleToSelectedNodes([](MindNode* n){ n->style.fontFamily = L"Arial"; });
                    } else {
                        WCHAR buf[64];
                        SendMessage((HWND)lParam, CB_GETLBTEXT, idx, (LPARAM)buf);
                        ApplyStyleToSelectedNodes([buf](MindNode* n){ n->style.fontFamily = buf; });
                    }
                }
            }
            return 0;

        case IDM_TEXTSIZE:
            if (HIWORD(wParam) == EN_CHANGE) {
                WCHAR buf[16];
                GetWindowText((HWND)lParam, buf, 16);
                try {
                    int val = std::stoi(buf);
                    if (val > 0) {
                        ApplyStyleToSelectedNodes([val](MindNode* n){ n->style.fontSize = val; });
                    }
                } catch(...) {}
            }
            return 0;

        case IDM_TEXTCOLOR:
            if (HIWORD(wParam) == CBN_SELCHANGE) {
                int idx = SendMessage((HWND)lParam, CB_GETCURSEL, 0, 0);
                if (idx != CB_ERR) {
                    std::wstring color = GetColorFromIndex(idx, hWnd);
                    if (!color.empty()) {
                        ApplyStyleToSelectedNodes([color](MindNode* n){ n->style.textColor = color; });
                    }
                }
            }
            return 0;

        case IDM_BOLD:
            ApplyStyleToSelectedNodes([](MindNode* n){ n->style.bold = !n->style.bold; });
            return 0;

        case IDM_ITALIC:
            ApplyStyleToSelectedNodes([](MindNode* n){ n->style.italic = !n->style.italic; });
            return 0;

        case IDM_UNDERLINE:
            ApplyStyleToSelectedNodes([](MindNode* n){ n->style.underline = !n->style.underline; });
            return 0;

        case IDM_TEXTALIGN:
            if (HIWORD(wParam) == CBN_SELCHANGE) {
                int idx = SendMessage((HWND)lParam, CB_GETCURSEL, 0, 0);
                if (idx != CB_ERR) {
                    std::wstring align;
                    switch(idx) {
                        case 0: align = L"left"; break;
                        case 1: align = L"center"; break;
                        case 2: align = L"right"; break;
                    }
                    ApplyStyleToSelectedNodes([align](MindNode* n){ n->style.textAlignment = align; });
                }
            }
            return 0;

            // ========== 绘图设置面板控件命令处理 ==========
        case IDM_PRESET:
            if (HIWORD(wParam) == CBN_SELCHANGE) {
                int idx = SendMessage((HWND)lParam, CB_GETCURSEL, 0, 0);
                if (idx != CB_ERR) {
                    // TODO: Apply preset styles
                }
            }
            return 0;

        case IDM_AUTO_BALANCE:
        case IDM_COMPACT_LAYOUT:
        case IDM_SIBLING_ALIGN:
            // 仅更新自定义按钮状态
            {
                HWND hWndButton = (HWND)lParam;
                bool checked = (SendMessage(hWndButton, BM_GETCHECK, 0, 0) == BST_CHECKED);
                COLORREF bgColor = checked ? RGB(180, 180, 180) : RGB(240, 240, 240);
                COLORREF borderColor = RGB(200, 200, 200);
                if (checked) {
                    // If checked, apply the style immediately
                    /*
                    ApplyStyleToSelectedNodes([&](MindNode* n) {
                        n->layout.autoBalance = (LOWORD(wParam) == IDM_AUTO_BALANCE) ? true : n->layout.autoBalance;
                        n->layout.compact = (LOWORD(wParam) == IDM_COMPACT_LAYOUT) ? true : n->layout.compact;
                        n->layout.siblingAlign = (LOWORD(wParam) == IDM_SIBLING_ALIGN) ? true : n->layout.siblingAlign;
                    });
                    */
                }
                // Update button appearance
                SendMessage(hWndButton, BM_SETSTATE, checked ? BST_UNCHECKED : BST_CHECKED, 0);
                InvalidateRect(hWndButton, NULL, TRUE);
            }
            return 0;
        }
        break;
    

    case WM_MOUSEWHEEL:
        // 处理鼠标滚轮滚动
    {
        int zDelta = GET_WHEEL_DELTA_WPARAM(wParam);
        HWND hCurrentScroll = (global::S_CURRENT == global::W_STYLE) ? global::W_STYLE : global::W_PAINT;

        // 转换为滚动条消息
        SendMessage(hWnd, WM_VSCROLL,
            MAKEWPARAM((zDelta > 0) ? SB_LINEUP : SB_LINEDOWN, 0),
            (LPARAM)hCurrentScroll);
    }
    break;

    case WM_NOTIFY: {
        NMHDR* nmhdr = (NMHDR*)lParam;

        if (nmhdr->hwndFrom == global::S_TAB &&
            nmhdr->code == TCN_SELCHANGE)
        {
            int selected = TabCtrl_GetCurSel(global::S_TAB);

            switch (selected) {
            case 0:
                SendMessageW(hWnd, WM_COMMAND, IDM_EDITSTYLE, 0);
                break;
            case 1:
                SendMessageW(hWnd, WM_COMMAND, IDM_EDITPAINT, 0);
                break;
            }
        }
    }
    }
    return DefWindowProc(hWnd, message, wParam, lParam);
}