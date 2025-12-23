//������Ϣ����Դ�ļ�
#include "MindMapData.h"
#include "framework.h"
#include "MINDTREE.h"
#include "controls.h"
#include "message.h"
#include "resolve.h"
#include "MindMapManager.h"
#include "borderless_button.h"
#include "canvas_impl.h" // Added for Canvas_GetZoom/Canvas_ZoomBy

static WNDPROC g_oldZoomEditProc = NULL;

static LRESULT CALLBACK ZoomEditProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_KEYDOWN && wParam == VK_RETURN) {
        // Remove focus to trigger EN_KILLFOCUS in parent
        SetFocus(global::HOME); 
        return 0;
    }
    return CallWindowProc(g_oldZoomEditProc, hWnd, msg, wParam, lParam);
}

void Ftools(RECT home) {
    SetWindowPos(global::TOOLS, HWND_TOP, 0, 0, home.right - home.left, 70, SWP_NOMOVE);
    if (global::SUB != NULL) {
        GetClientRect(global::TOOLS, &global::tools);
        int width = global::tools.right - global::tools.left;
        int height = global::tools.bottom - global::tools.top;
        GetClientRect(global::SUB, &global::button);
        if (global::button.left != width / 10 * 3 - 16) {
            SetWindowPos(global::SUB, HWND_TOP, width / 9 * 3 - 16, (height - 20) / 2 - 16, 0, 0, SWP_NOSIZE);
            SetWindowPos(global::PARENT, HWND_TOP, width / 9 * 4 - 16, (height - 20) / 2 - 16, 0, 0, SWP_NOSIZE);
            SetWindowPos(global::MATE, HWND_TOP, width / 9 * 5 - 16, (height - 20) / 2 - 16, 0, 0, SWP_NOSIZE);
            SetWindowPos(global::RETURN, HWND_TOP, width / 9 * 6 - 16, (height - 20) / 2 - 16, 0, 0, SWP_NOSIZE);
            SetWindowPos(global::CLOSETOOLS, HWND_TOP, 0, 50, width, 20, NULL);
			SetWindowPos(global::OPENTOOLS, HWND_TOP, 0, 0, width, 20, NULL);
			SetWindowPos(global::RIGHT, HWND_TOP, width - 36, (height - 20) / 2 - 16, 0, 0, SWP_NOSIZE);
        }
    }
}

void Setguide(RECT home) {
     SetWindowPos(global::GUIDE, HWND_TOP, 0, 20 + 50 * IsWindowVisible(global::SUB), 220, home.bottom - home.top - 20 - 50 * IsWindowVisible(global::SUB), NULL);
     if (global::G_TAB != NULL) {
         GetClientRect(global::GUIDE, &global::guide);
         int width = global::guide.right - global::guide.left;
         int height = global::guide.bottom - global::guide.top;
         GetClientRect(global::G_TREEVIEW, &global::button);
         if (global::button.bottom - global::button.top != height) {
             SetWindowPos(global::G_TAB, HWND_TOP, 0, 0, width - 20, 20, NULL);
             SetWindowPos(global::G_TREEVIEW, HWND_TOP, 0, 20, width - 20, height - 20, NULL);
			 SetWindowPos(global::G_CLOSE, HWND_TOP, width - 20, 0, 20, height, NULL);
         }
     }
}

void Fguidetopic() {
    if (!global::G_TREEVIEW) return;
    MindMapManager::Instance().RebuildTreeUI();
}

void Fguidenote() {
    static HTREEITEM s_noteListRoot = NULL;
    if (!global::G_TREEVIEW) return;

    // 1. 获取所有含笔记的节点
    auto nodes = MindMapManager::Instance().GetAllNodesWithNote();

    // 2. 清理 UI 和映射表
    MindMapManager::Instance().ClearUI();
    s_noteListRoot = NULL;

    TVINSERTSTRUCT tvisRoot{};
    tvisRoot.hParent = TVI_ROOT;
    tvisRoot.hInsertAfter = TVI_LAST;
    tvisRoot.item.mask = TVIF_TEXT | TVIF_PARAM;
    tvisRoot.item.pszText = (LPWSTR)L"含笔记节点";
    tvisRoot.item.lParam = (LPARAM)(-1); // 使用 -1 作为根节点的标记 ID
    s_noteListRoot = TreeView_InsertItem(global::G_TREEVIEW, &tvisRoot);
    
    // 为根节点创建一个临时节点对象来注册映射，或者只映射 Handle
    // 这里我们不需要真实的节点对象，只需记录根节点句柄

    for (MindNode* n : nodes) {
        if (!n) continue;
        std::wstring text = n->text;
        if (!n->note.empty()) {
            std::wstring notePreview = n->note;
            const size_t maxLen = 80;
            if (notePreview.size() > maxLen) {
                notePreview = notePreview.substr(0, maxLen) + L"...";
            }
            text += L" | 笔记: " + notePreview;
        }
        TVINSERTSTRUCT tvis{};
        tvis.hParent = s_noteListRoot;
        tvis.hInsertAfter = TVI_LAST;
        tvis.item.mask = TVIF_TEXT | TVIF_PARAM;
        tvis.item.pszText = (LPWSTR)text.c_str();
        tvis.item.lParam = (LPARAM)n->id;
        
        HTREEITEM hItem = TreeView_InsertItem(global::G_TREEVIEW, &tvis);
        if (hItem) {
            MindMapManager::Instance().RegisterNode(hItem, n);
        }
    }

    if (s_noteListRoot) {
        TreeView_Expand(global::G_TREEVIEW, s_noteListRoot, TVE_EXPAND);
    }
}

void Fguidelabel() {
    static HTREEITEM s_labelListRoot = NULL;
    if (!global::G_TREEVIEW) return;

    // 1. 获取所有含标签的节点
    auto nodes = MindMapManager::Instance().GetAllNodesWithLabel();

    // 2. 清理 UI 和映射表
    MindMapManager::Instance().ClearUI();
    s_labelListRoot = NULL;

    TVINSERTSTRUCT tvisRoot{};
    tvisRoot.hParent = TVI_ROOT;
    tvisRoot.hInsertAfter = TVI_LAST;
    tvisRoot.item.mask = TVIF_TEXT | TVIF_PARAM;
    tvisRoot.item.pszText = (LPWSTR)L"含标签节点";
    tvisRoot.item.lParam = (LPARAM)(-1); // 使用 -1 作为根节点的标记 ID
    s_labelListRoot = TreeView_InsertItem(global::G_TREEVIEW, &tvisRoot);
    
    // 为根节点创建一个临时节点对象来注册映射，或者只映射 Handle
    // 这里我们不需要真实的节点对象，只需记录根节点句柄

    for (MindNode* n : nodes) {
        if (!n) continue;
        std::wstring text = n->text;
        if (!n->label.empty()) {
            text += L" | 标签: " + n->label;
        }
        TVINSERTSTRUCT tvis{};
        tvis.hParent = s_labelListRoot;
        tvis.hInsertAfter = TVI_LAST;
        tvis.item.mask = TVIF_TEXT | TVIF_PARAM;
        tvis.item.pszText = (LPWSTR)text.c_str();
        tvis.item.lParam = (LPARAM)n->id;
        
        HTREEITEM hItem = TreeView_InsertItem(global::G_TREEVIEW, &tvis);
        if (hItem) {
            MindMapManager::Instance().RegisterNode(hItem, n);
        }
    }

    if (s_labelListRoot) {
        TreeView_Expand(global::G_TREEVIEW, s_labelListRoot, TVE_EXPAND);
    }
}

void Setstyle(RECT home) {
    SetWindowPos(global::STYLE, HWND_TOP, home.right - 220, 20 + 50 * IsWindowVisible(global::SUB), 220, home.bottom - home.top - 20 - 50 * IsWindowVisible(global::SUB), NULL);
	GetClientRect(global::STYLE, &global::style);
    SetWindowPos(global::W_STYLE, HWND_TOP, 20, 20, global::style.right - 20, global::style.bottom - 20, SWP_NOMOVE);
	SetWindowPos(global::W_PAINT, HWND_TOP, 20, 20, global::style.right - 20, global::style.bottom - 20, SWP_NOMOVE);
	SetWindowPos(global::S_CLOSE, HWND_TOP, 0, 0, 20, global::style.bottom, NULL);
}

void SetFILES(RECT home) {
    SetWindowPos(global::FILES, HWND_TOP, 220 * IsWindowVisible(global::GUIDE), 20 + 50 * IsWindowVisible(global::SUB), home.right - 220 * (IsWindowVisible(global::GUIDE) + IsWindowVisible(global::STYLE)), 20, SWP_SHOWWINDOW);

}

void UpdateZoomLabel() {
    if (global::ZOOM_LABEL) {
        int zoom = Canvas_GetZoom();
        std::wstring text = std::to_wstring(zoom) + L"%";
        SetWindowText(global::ZOOM_LABEL, text.c_str());
    }
}

void SetCLOTH(RECT home) {
    int clothH = 25;
    int y = home.bottom - clothH;
    int width = home.right - 220 * (IsWindowVisible(global::GUIDE) + IsWindowVisible(global::STYLE));
    int x = 220 * IsWindowVisible(global::GUIDE);

    SetWindowPos(global::CLOTH, HWND_TOP, x, y, width, clothH, SWP_SHOWWINDOW);

    // Update Zoom Controls Position
    // Layout: [Dec][Label][Inc] at the right side of CLOTH area
    // But CLOTH is a TabControl, we might want to place these on top or next to it?
    // The user said "display in global::CLOTH's right".
    // Since CLOTH is a child window (TabControl), we can place these buttons as siblings on top of it or to the right if CLOTH doesn't take full width.
    // The current SetCLOTH makes CLOTH take the full available width.
    // Let's place them as children of the main window, positioned over the right side of the CLOTH area.
    
    int btnSize = 20;
    int labelW = 40;
    int margin = 5;
    
    int totalW = btnSize * 2 + labelW + margin * 2;
    int startX = x + width - totalW - margin;
    int startY = y + (clothH - btnSize) / 2;

    if (!global::ZOOM_DEC) {
        HICON hIconDec = (HICON)LoadImage(global::hInst, L"icon/decrease.ico", IMAGE_ICON, 12, 12, LR_LOADFROMFILE); // Reduced icon size to 12x12
        BORDERLESS_BUTTON_CONFIG config = {0};
        config.iconSize = 12; // Set icon size in config
        config.bgColorNormal = RGB(240, 240, 240); // Match background
        global::ZOOM_DEC = CreateBorderlessButton(global::HOME, global::hInst, 0, 0, btnSize, btnSize, NULL, hIconDec, IDM_ZOOM_DEC, config);
    }
    if (!global::ZOOM_LABEL) {
        // Changed to EDIT control for editable text
        global::ZOOM_LABEL = CreateWindow(L"EDIT", L"100%", WS_CHILD | WS_VISIBLE | ES_CENTER | ES_NUMBER, 0, 0, labelW, btnSize, global::HOME, (HMENU)IDM_ZOOM_EDIT, global::hInst, NULL);
        // Set font to match UI
        SendMessage(global::ZOOM_LABEL, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);
        
        // Subclass the edit control to handle Enter key
        g_oldZoomEditProc = (WNDPROC)SetWindowLongPtr(global::ZOOM_LABEL, GWLP_WNDPROC, (LONG_PTR)ZoomEditProc);
    }
    if (!global::ZOOM_INC) {
        HICON hIconInc = (HICON)LoadImage(global::hInst, L"icon/mate.ico", IMAGE_ICON, 12, 12, LR_LOADFROMFILE); // Reduced icon size to 12x12
        BORDERLESS_BUTTON_CONFIG config = {0};
        config.iconSize = 12; // Set icon size in config
        config.bgColorNormal = RGB(240, 240, 240); // Match background
        global::ZOOM_INC = CreateBorderlessButton(global::HOME, global::hInst, 0, 0, btnSize, btnSize, NULL, hIconInc, IDM_ZOOM_INC, config);
    }

    SetWindowPos(global::ZOOM_DEC, HWND_TOP, startX, startY, btnSize, btnSize, SWP_SHOWWINDOW);
    SetWindowPos(global::ZOOM_LABEL, HWND_TOP, startX + btnSize, startY, labelW, btnSize, SWP_SHOWWINDOW);
    SetWindowPos(global::ZOOM_INC, HWND_TOP, startX + btnSize + labelW, startY, btnSize, btnSize, SWP_SHOWWINDOW);
    
    UpdateZoomLabel();
}

void SetCanvas(RECT home) {
    if (!global::HOME || !IsWindow(global::HOME)) return;
    
    // Find the canvas window (it's a child of HOME with class MINDTREE_CANVAS_IMPL)
    HWND hCanvas = FindWindowExW(global::HOME, NULL, L"MINDTREE_CANVAS_IMPL", NULL);
    if (hCanvas) {
        int left = 220 * IsWindowVisible(global::GUIDE);
        int top = 20 + 50 * IsWindowVisible(global::SUB); // Assuming 20 for FILES tab, 50 for TOOLS if expanded
        // Actually, TOOLS height logic in Ftools is:
        // If expanded (SUB visible), height is ~70? 
        // Let's look at Ftools: SetWindowPos(global::TOOLS, ..., 70, ...)
        // And SetFILES: 20 + 50 * IsWindowVisible(global::SUB)
        // So top is same as FILES y + FILES height?
        // FILES is at y = 20 + 50*IsWindowVisible(global::SUB) with height 20.
        // Wait, SetFILES sets y position.
        // Let's re-read SetFILES:
        // SetWindowPos(global::FILES, ..., 20 + 50 * IsWindowVisible(global::SUB), ..., 20, ...)
        // So FILES is at y.
        // Canvas should be below FILES.
        
        // Let's look at Canvas_EnsureWindow in canvas_impl.cpp:
        // y = 50 * IsWindowVisible(global::TOOLS) -> This seems to rely on TOOLS window visibility, not SUB.
        // But Ftools logic suggests TOOLS is always visible but changes content?
        // Actually, InitInstance creates TOOLS with height 70.
        // And SetFILES puts FILES at y=70 (if TOOLS visible/expanded).
        
        // Let's align with SetFILES logic for Y start.
        // FILES y = 20 + 50 * IsWindowVisible(global::SUB)
        // FILES height = 20
        // So Canvas Y = FILES y + 20 = 40 + 50 * IsWindowVisible(global::SUB)
        
        int filesY = 20 + 50 * IsWindowVisible(global::SUB);
        int filesH = 20;
        int canvasY = filesY + filesH;
        
        // CLOTH is at bottom, height 20.
        int clothH = 20;
        
        int width = home.right - 220 * (IsWindowVisible(global::GUIDE) + IsWindowVisible(global::STYLE));
        int height = home.bottom - canvasY - clothH;
        
        SetWindowPos(hCanvas, HWND_TOP, left, canvasY, width, height, SWP_NOZORDER);
    }
}