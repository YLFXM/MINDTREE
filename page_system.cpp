#include "page_system.h"
#include "MINDTREE.h"
#include "file_system.h"
#include "MindMapManager.h"
#include <commctrl.h>
#include <string>

void PageNew(HWND hWnd) {
    // Create a new page in Manager
    MindMapManager::Instance().AddNewPage();
    
    // Add Tab to UI
    if (global::FILES) {
        int count = MindMapManager::Instance().GetPageCount();
        std::wstring title = L"Page " + std::to_wstring(count);
        
        TCITEM tie;
        tie.mask = TCIF_TEXT;
        tie.pszText = (LPWSTR)title.c_str();
        TabCtrl_InsertItem(global::FILES, count-1, &tie);
        TabCtrl_SetCurSel(global::FILES, count-1);
    }
}

void PageNewFile(HWND hWnd) {
    // Create new page implies new file in this context
    PageNew(hWnd);
}

void PageOpenFile(HWND hWnd) {
    // Open file in a "new page"
    PageNew(hWnd);
    FileOpen(hWnd);
    
    // Update Tab Title
    if (global::FILES) {
        int cur = TabCtrl_GetCurSel(global::FILES);
        std::wstring title = MindMapManager::Instance().GetPageTitle(cur);
        
        TCITEM tie;
        tie.mask = TCIF_TEXT;
        tie.pszText = (LPWSTR)title.c_str();
        TabCtrl_SetItem(global::FILES, cur, &tie);
    }
}

void NavigateReturnToCenter(HWND hWnd) {
    // 回到中心主题：选择 TreeView 的根节点并确保可见
    if (global::G_TREEVIEW) {
        HTREEITEM hRoot = TreeView_GetRoot(global::G_TREEVIEW);
        if (hRoot) {
            TreeView_SelectItem(global::G_TREEVIEW, hRoot);
            TreeView_EnsureVisible(global::G_TREEVIEW, hRoot);
            SetFocus(global::G_TREEVIEW);
        }
    }
}

