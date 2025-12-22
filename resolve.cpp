//������Ϣ����Դ�ļ�
#include "MindMapData.h"
#include "framework.h"
#include "MINDTREE.h"
#include "controls.h"
#include "message.h"
#include "resolve.h"
#include "MindMapManager.h"

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

void SetCLOTH(RECT home) {
    SetWindowPos(global::CLOTH, HWND_TOP, 220 * IsWindowVisible(global::GUIDE), home.bottom - 20, home.right - 220 * (IsWindowVisible(global::GUIDE) + IsWindowVisible(global::STYLE)), 20, SWP_SHOWWINDOW);
}