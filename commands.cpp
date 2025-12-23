#include "MindMapData.h"
#include "MindMapManager.h"
#include "canvas_impl.h"
#include "commands.h"
#include "resolve.h"
#include "framework.h"
#include "page_system.h"
#include <shellapi.h>
#include <string>
#include <commdlg.h>
#include "history_system.h"

// 简单的标题拼装：主题文本 + 可选标签 + 笔记/概要标记
static void UpdateNodeTitleWithMeta(MindNode* node) {
    if (!node || !global::G_TREEVIEW) return;
    std::wstring title = node->text;
    if (!node->label.empty()) {
        title += L" [" + node->label + L"]";
    }
    // 笔记/概要有内容时添加轻量标记
    if (!node->note.empty())  title += L" ✎"; // note marker
    if (!node->summary.empty()) title += L" Ⓢ"; // summary marker

    TVITEM tvi = {0};
    tvi.hItem = node->hTreeItem;
    tvi.mask = TVIF_TEXT;
    tvi.pszText = (LPWSTR)title.c_str();
    TreeView_SetItem(global::G_TREEVIEW, &tvi);
}

void HandleSubtopic(HWND hWnd) {
    HistorySystem::RecordScope rec([] { return MindMapManager::Instance().CreateSnapshot(); });
    std::wstring text = PromptForString(hWnd, L"输入子主题内容", L"子主题");
    if (text.empty()) return;
    MindMapManager::Instance().AddSubNodeToSelected(text);
    
    // Calculate size for the new node (which is now selected)
    auto& sel = MindMapManager::Instance().GetSelectedNodes();
    for (auto n : sel) Canvas_CalculateNodeSize(n);
    
    Canvas_Invalidate();
}

void HandleWorkmate(HWND hWnd) {
    HistorySystem::RecordScope rec([] { return MindMapManager::Instance().CreateSnapshot(); });
    std::wstring text = PromptForString(hWnd, L"输入同级主题内容", L"同级主题");
    if (text.empty()) return;
    MindMapManager::Instance().AddSiblingNodeToSelected(text);
    
    auto& sel = MindMapManager::Instance().GetSelectedNodes();
    for (auto n : sel) Canvas_CalculateNodeSize(n);
    
    Canvas_Invalidate();
}

void HandleParent(HWND hWnd) {
    HistorySystem::RecordScope rec([] { return MindMapManager::Instance().CreateSnapshot(); });
    std::wstring text = PromptForString(hWnd, L"输入父主题内容", L"父主题");
    if (text.empty()) return;
    MindMapManager::Instance().AddParentNodeToSelected(text);
    
    auto& sel = MindMapManager::Instance().GetSelectedNodes();
    for (auto n : sel) Canvas_CalculateNodeSize(n);
    
    Canvas_Invalidate();
}

void HandleFreeTopic(HWND hWnd) {
    HistorySystem::RecordScope rec([] { return MindMapManager::Instance().CreateSnapshot(); });
    // 弹出输入框获取自由主题内容
    std::wstring text = PromptForString(hWnd, L"输入自由主题内容", L"自由主题");
    if (text.empty()) return;

    // 创建一个不挂在根节点下的自由节点，直接作为 TreeView 顶层项
    MindNode* newNode = MindMapManager::Instance().AddFreeNode(text);
    if (!newNode) return;
    UpdateNodeTitleWithMeta(newNode);
    
    Canvas_CalculateNodeSize(newNode);
    
    Canvas_Invalidate();
}

void HandleLabel(HWND hWnd) {
    HistorySystem::RecordScope rec([] { return MindMapManager::Instance().CreateSnapshot(); });
    if (!global::G_TREEVIEW) { MessageBox(hWnd, L"Tree view not available.", L"Error", MB_OK); return; }
    HTREEITEM hSel = TreeView_GetSelection(global::G_TREEVIEW);
    if (!hSel) { MessageBox(hWnd, L"请选择一个主题来设置标签。", L"提示", MB_OK); return; }
    MindNode* node = MindMapManager::Instance().GetNodeFromHandle(hSel);
    if (!node) return;
    std::wstring label = PromptForString(hWnd, L"输入标签", node->label);
    if (!label.empty()) {
        node->label = label;
        UpdateNodeTitleWithMeta(node);
    }
}

void HandleAnnotation(HWND hWnd) {
    HistorySystem::RecordScope rec([] { return MindMapManager::Instance().CreateSnapshot(); });
    if (!global::G_TREEVIEW) { MessageBox(hWnd, L"Tree view not available.", L"Error", MB_OK); return; }
    HTREEITEM hSel = TreeView_GetSelection(global::G_TREEVIEW);
    if (!hSel) { MessageBox(hWnd, L"请选择一个主题来添加注解。", L"提示", MB_OK); return; }
    MindNode* node = MindMapManager::Instance().GetNodeFromHandle(hSel);
    if (!node) return;
    std::wstring annotation = PromptForString(hWnd, L"输入注解", node->annotation);
    if (!annotation.empty()) {
        node->annotation = annotation;
        UpdateNodeTitleWithMeta(node);
        MessageBox(hWnd, L"注解已保存。", L"注解", MB_OK);
    }
}

// Global state for link creation (shared with canvas)
static MindNode* g_linkSourceNode = nullptr;
static bool g_linkModeActive = false;

bool LinkModeIsActive() { return g_linkModeActive; }
MindNode* LinkModeGetSource() { return g_linkSourceNode; }

void LinkModeStart(MindNode* sourceNode) {
    g_linkSourceNode = sourceNode;
    g_linkModeActive = (sourceNode != nullptr);
    SetCursor(LoadCursor(NULL, g_linkModeActive ? IDC_CROSS : IDC_ARROW));
}

void LinkModeCancel() {
    g_linkSourceNode = nullptr;
    g_linkModeActive = false;
    SetCursor(LoadCursor(NULL, IDC_ARROW));
    Canvas_Invalidate();
}

bool LinkModeTryComplete(MindNode* targetNode) {
    if (!g_linkModeActive || !g_linkSourceNode || !targetNode) return false;

    HistorySystem::RecordScope rec([] { return MindMapManager::Instance().CreateSnapshot(); });

    // Ignore self-link
    if (g_linkSourceNode == targetNode) return false;

    // Check if link already exists
    for (int linkedId : g_linkSourceNode->linkedNodeIds) {
        if (linkedId == targetNode->id) {
            LinkModeCancel();
            return true; // treat as handled
        }
    }

    // Create bidirectional link
    g_linkSourceNode->linkedNodeIds.push_back(targetNode->id);
    targetNode->linkedNodeIds.push_back(g_linkSourceNode->id);

    LinkModeCancel();
    Canvas_Invalidate();
    return true;
}

void HandleLink(HWND hWnd) {
    // Enter link mode: first node is the current selection; second node picked by single click on canvas/tree.
    if (!global::G_TREEVIEW) return;
    HTREEITEM hSel = TreeView_GetSelection(global::G_TREEVIEW);
    if (!hSel) return;
    MindNode* node = MindMapManager::Instance().GetNodeFromHandle(hSel);
    if (!node) return;

    LinkModeStart(node);
}



void HandleNote(HWND hWnd) {
    HistorySystem::RecordScope rec([] { return MindMapManager::Instance().CreateSnapshot(); });
    if (!global::G_TREEVIEW) { MessageBox(hWnd, L"Tree view not available.", L"Error", MB_OK); return; }
    HTREEITEM hSel = TreeView_GetSelection(global::G_TREEVIEW);
    if (!hSel) { MessageBox(hWnd, L"请选择一个主题来编辑笔记。", L"提示", MB_OK); return; }
    MindNode* node = MindMapManager::Instance().GetNodeFromHandle(hSel);
    if (!node) return;
    std::wstring note = PromptForString(hWnd, L"编辑笔记", node->note);
    if (!note.empty()) {
        node->note = note;
        UpdateNodeTitleWithMeta(node);
        MessageBox(hWnd, L"笔记已保存。", L"笔记", MB_OK);
    }
}

void HandleOutlinkPage(HWND hWnd) {
    HistorySystem::RecordScope rec([] { return MindMapManager::Instance().CreateSnapshot(); });
    if (!global::G_TREEVIEW) return;
    HTREEITEM hSel = TreeView_GetSelection(global::G_TREEVIEW);
    if (!hSel) { MessageBox(hWnd, L"请选择主题以链接网页。", L"提示", MB_OK); return; }
    MindNode* node = MindMapManager::Instance().GetNodeFromHandle(hSel);
    if (!node) return;
    
    std::wstring url = PromptForString(hWnd, L"输入网页地址 (URL):", node->outlinkURL);
    if (url.empty()) return;
    
    node->outlinkURL = url;
    MessageBox(hWnd, L"网页链接已保存。", L"关联网页", MB_OK);
}

void HandleOutlinkFile(HWND hWnd) {
    HistorySystem::RecordScope rec([] { return MindMapManager::Instance().CreateSnapshot(); });
    if (!global::G_TREEVIEW) return;
    HTREEITEM hSel = TreeView_GetSelection(global::G_TREEVIEW);
    if (!hSel) { MessageBox(hWnd, L"请选择主题以链接文件。", L"提示", MB_OK); return; }
    OPENFILENAME ofn; wchar_t szFile[MAX_PATH] = {0}; ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn); ofn.hwndOwner = hWnd; ofn.lpstrFile = szFile; ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST; ofn.lpstrFilter = L"All Files\0*.*\0";
    if (GetOpenFileName(&ofn)) {
        MindNode* node = MindMapManager::Instance().GetNodeFromHandle(hSel);
        if (!node) return;
        node->outlinkFile = ofn.lpstrFile;
        MessageBox(hWnd, L"文件链接已保存。", L"关联文件", MB_OK);
    }
}

void HandleOutlinkTopic(HWND hWnd) {
    HistorySystem::RecordScope rec([] { return MindMapManager::Instance().CreateSnapshot(); });
    if (!global::G_TREEVIEW) return;
    HTREEITEM hSel = TreeView_GetSelection(global::G_TREEVIEW);
    if (!hSel) { MessageBox(hWnd, L"请选择主题以链接到其它主题。", L"提示", MB_OK); return; }
    std::wstring res = PromptForString(hWnd, L"输入目标主题ID(数字)");
    if (res.empty()) return;
    int tid = std::stoi(res);
    MindNode* node = MindMapManager::Instance().GetNodeFromHandle(hSel);
    if (!node) return;
    node->outlinkTopicId = tid;
    {
        std::wstring msg = std::wstring(L"已关联主题ID: ") + std::to_wstring(tid);
        MessageBox(hWnd, msg.c_str(), L"关联主题", MB_OK);
    }
}

void HandleNewPaint(HWND hWnd) {
    // Create a new paint (sheet) in the current page
    MindMapManager::Instance().AddNewPaint();
    
    // Update the CLOTH tab control
    if (global::CLOTH) {
        int count = MindMapManager::Instance().GetPaintCount();
        std::wstring title = MindMapManager::Instance().GetPaintTitle(count - 1) + L"      "; // Added padding
        
        TCITEM tie;
        tie.mask = TCIF_TEXT;
        tie.pszText = (LPWSTR)title.c_str();
        TabCtrl_InsertItem(global::CLOTH, count - 1, &tie);
        TabCtrl_SetCurSel(global::CLOTH, count - 1);
    }
    
    Canvas_Invalidate();
}

void HandleNewPaintWithTopic(HWND hWnd) {
    HistorySystem::RecordScope rec([] { return MindMapManager::Instance().CreateSnapshot(); });
    if (!global::G_TREEVIEW) return;
    HTREEITEM hSel = TreeView_GetSelection(global::G_TREEVIEW);
    if (!hSel) { MessageBox(hWnd, L"请选择主题以新建关联绘图。", L"提示", MB_OK); return; }
    
    MindNode* node = MindMapManager::Instance().GetNodeFromHandle(hSel);
    if (!node) return;

    // Create a new paint (sheet) using the selected node as root
    MindMapManager::Instance().AddNewPaintFromNode(node);
    
    // Update the CLOTH tab control
    if (global::CLOTH) {
        int count = MindMapManager::Instance().GetPaintCount();
        std::wstring title = MindMapManager::Instance().GetPaintTitle(count - 1) + L"      "; // Added padding
        
        TCITEM tie;
        tie.mask = TCIF_TEXT;
        tie.pszText = (LPWSTR)title.c_str();
        TabCtrl_InsertItem(global::CLOTH, count - 1, &tie);
        TabCtrl_SetCurSel(global::CLOTH, count - 1);
    }
    
    Canvas_Invalidate();
}

void HandleLarger(HWND hWnd) {
    Canvas_ZoomBy(+10);
    Canvas_Invalidate();
}

void HandleSmaller(HWND hWnd) {
    Canvas_ZoomBy(-10);
    Canvas_Invalidate();
}

void HandleAdapt(HWND hWnd) {
    // Reset zoom to 100% as a simple 'adapt' behaviour for now
    {
        Canvas_FitToView();
        Canvas_Invalidate();
    }
}

void HandleLineOnly(HWND hWnd) {
    if (!global::G_TREEVIEW) return;
    HTREEITEM hSel = TreeView_GetSelection(global::G_TREEVIEW);
    if (!hSel) { MessageBox(hWnd, L"请选择主题以仅显示该分支。", L"提示", MB_OK); return; }
    // Toggle filter: if already filtered on this node, clear; else set
    static HTREEITEM lastFilter = NULL;
    if (lastFilter == hSel) {
        Canvas_ClearFilter();
        lastFilter = NULL;
    } else {
        Canvas_SetFilterRoot(hSel);
        lastFilter = hSel;
    }
}

// Implement summary and frame handlers (use PromptForString)
void HandleSummary(HWND hWnd) {
    if (!global::G_TREEVIEW) { MessageBox(hWnd, L"Tree view not available.", L"Error", MB_OK); return; }
    HTREEITEM hSel = TreeView_GetSelection(global::G_TREEVIEW);
    if (!hSel) { MessageBox(hWnd, L"请选择一个主题来编辑概要。", L"提示", MB_OK); return; }
    MindNode* node = MindMapManager::Instance().GetNodeFromHandle(hSel);
    if (!node) return;
    std::wstring s = PromptForString(hWnd, L"编辑概要", node->summary);
    if (!s.empty()) {
        node->summary = s;
        UpdateNodeTitleWithMeta(node);
        MessageBox(hWnd, L"概要已保存。", L"概要", MB_OK);
    }
}

void HandleFrame(HWND hWnd) {
    auto& sel = MindMapManager::Instance().GetSelectedNodes();
    if (sel.size() > 1) {
        std::vector<int> ids;
        for (auto n : sel) ids.push_back(n->id);
        MindMapManager::Instance().AddFrame(ids, L"rect");
        Canvas_Invalidate();
        MessageBox(hWnd, L"外框已添加。", L"框架", MB_OK);
        return;
    }

    if (!global::G_TREEVIEW) { MessageBox(hWnd, L"Tree view not available.", L"Error", MB_OK); return; }
    HTREEITEM hSel = TreeView_GetSelection(global::G_TREEVIEW);
    if (!hSel) { MessageBox(hWnd, L"请选择一个或多个主题来设置框架。", L"提示", MB_OK); return; }
    MindNode* node = MindMapManager::Instance().GetNodeFromHandle(hSel);
    if (!node) return;
    std::wstring f = PromptForString(hWnd, L"设置框架样式(例如: box, rounded)", node->frameStyle);
    if (!f.empty()) {
        node->frameStyle = f;
        MessageBox(hWnd, L"框架样式已保存。", L"框架", MB_OK);
    }
}

// Simple modal prompt implementation
struct PromptState {
    HWND hEdit{};
    std::wstring* result{};
    bool* done{};
};

static LRESULT CALLBACK PromptWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    auto state = reinterpret_cast<PromptState*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    switch (msg) {
    case WM_COMMAND: {
        int id = LOWORD(wParam);
        if (!state) break;
        if (id == 1) { // OK
            int len = GetWindowTextLength(state->hEdit) + 1;
            std::wstring buf(len, L'\0');
            GetWindowText(state->hEdit, &buf[0], len);
            if (!buf.empty() && buf.back() == L'\0') buf.pop_back();
            *(state->result) = buf;
            *(state->done) = true;
            DestroyWindow(hwnd);
            return 0;
        } else if (id == 2) { // Cancel
            state->result->clear();
            *(state->done) = true;
            DestroyWindow(hwnd);
            return 0;
        }
        break;
    }
    case WM_CLOSE:
        if (state && state->result) state->result->clear();
        if (state && state->done) *(state->done) = true;
        DestroyWindow(hwnd);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

std::wstring PromptForString(HWND hWnd, const std::wstring& title, const std::wstring& initial) {
    const int clientW = 360;
    const int clientH = 140;
    
    // Calculate required window size to ensure client area is 360x140
    RECT rc = { 0, 0, clientW, clientH };
    AdjustWindowRectEx(&rc, WS_POPUP | WS_VISIBLE | WS_CAPTION | WS_SYSMENU, FALSE, WS_EX_DLGMODALFRAME);
    int winW = rc.right - rc.left;
    int winH = rc.bottom - rc.top;

    HWND hDlg = CreateWindowEx(WS_EX_DLGMODALFRAME, L"STATIC", title.c_str(), WS_POPUP | WS_VISIBLE | WS_CAPTION | WS_SYSMENU,
        CW_USEDEFAULT, CW_USEDEFAULT, winW, winH, hWnd, NULL, global::hInst, NULL);
    if (!hDlg) return L"";
    SetWindowLongPtr(hDlg, GWLP_WNDPROC, (LONG_PTR)PromptWndProc);

    HWND hEdit = CreateWindowEx(0, L"EDIT", initial.c_str(), WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
        10, 40, clientW - 20, 24, hDlg, NULL, global::hInst, NULL);
    HWND hOk = CreateWindowEx(0, L"BUTTON", L"OK", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
        clientW/2 - 80, 90, 70, 24, hDlg, (HMENU)1, global::hInst, NULL);
    HWND hCancel = CreateWindowEx(0, L"BUTTON", L"Cancel", WS_CHILD | WS_VISIBLE,
        clientW/2 + 10, 90, 70, 24, hDlg, (HMENU)2, global::hInst, NULL);
    
    RECT rcOwner; 
    if (hWnd && IsWindow(hWnd)) GetWindowRect(hWnd, &rcOwner);
    else { rcOwner.left = 0; rcOwner.top = 0; rcOwner.right = GetSystemMetrics(SM_CXSCREEN); rcOwner.bottom = GetSystemMetrics(SM_CYSCREEN); }
    
    int x = rcOwner.left + (rcOwner.right - rcOwner.left - winW) / 2;
    int y = rcOwner.top + (rcOwner.bottom - rcOwner.top - winH) / 2; 
    SetWindowPos(hDlg, HWND_TOP, x, y, 0, 0, SWP_NOSIZE);
    // 让对话框成为简易“模态”：禁用父窗口，完毕后恢复
    HWND hOwner = (hWnd && IsWindow(hWnd)) ? hWnd : NULL;
    if (hOwner) EnableWindow(hOwner, FALSE);
    SetFocus(hEdit);

    std::wstring result; bool done = false; MSG msg;
    PromptState state{ hEdit, &result, &done };
    SetWindowLongPtr(hDlg, GWLP_USERDATA, (LONG_PTR)&state);

    while (!done && GetMessage(&msg, NULL, 0, 0)) {
        if (IsDialogMessage(hDlg, &msg)) {
            continue;
        }
        TranslateMessage(&msg);
        DispatchMessage(&msg);
        if (!IsWindow(hDlg)) break;
    }

    if (hOwner) EnableWindow(hOwner, TRUE);
    if (hOwner) SetActiveWindow(hOwner);
    return result;
}

void HandleStructure(HWND hWnd) {
    if (!global::G_TREEVIEW) return;
    HTREEITEM hSel = TreeView_GetSelection(global::G_TREEVIEW);
    if (!hSel) return;
    MindNode* node = MindMapManager::Instance().GetNodeFromHandle(hSel);
    if (!node) return;

    int sel = SendMessage(GetDlgItem(global::W_STYLE, IDM_STRUCTURE), CB_GETCURSEL, 0, 0);
    if (sel != CB_ERR) {
        wchar_t buf[256];
        SendMessage(GetDlgItem(global::W_STYLE, IDM_STRUCTURE), CB_GETLBTEXT, sel, (LPARAM)buf);
        
        std::wstring oldStyle = node->style.nodeStructure;
        std::wstring newStyle = buf;
        
        if (oldStyle != newStyle) {
            HistorySystem::RecordScope rec([] { return MindMapManager::Instance().CreateSnapshot(); });
            
            // Apply to current node
            node->style.nodeStructure = newStyle;
            
            // Propagate to children recursively until a different style is encountered
            // Helper lambda for recursion
            std::function<void(MindNode*, const std::wstring&, const std::wstring&)> propagateStyle;
            propagateStyle = [&](MindNode* n, const std::wstring& targetStyle, const std::wstring& sourceOldStyle) {
                for (MindNode* child : n->children) {
                    // If child's style matches the parent's OLD style (or is "default"), update it.
                    // If child has a different explicit style, stop propagation.
                    // Note: "default" means it was inheriting. If we change parent, "default" child effectively changes too.
                    // But here we are setting explicit styles.
                    // Logic: "until a different style is encountered".
                    // If child style == sourceOldStyle, change to targetStyle and continue.
                    // If child style == "default", it implicitly follows parent, so we don't need to change it explicitly?
                    // Wait, if we change parent from A to B.
                    // Child was A (explicitly) -> Change to B, recurse.
                    // Child was "default" -> It inherits B automatically. Do we need to recurse?
                    // If Child was "default", its effective style changes.
                    // But the requirement says "modify all children styles... until a different style".
                    // This implies we are changing explicit settings.
                    
                    // Let's assume we only change children that had the SAME explicit style as the parent's old style.
                    // Or if they were "default" (inheriting), they continue to inherit (no change needed).
                    // But if the user explicitly set "mindmap" on parent and children, and now changes parent to "tree",
                    // they probably want children to become "tree" too.
                    
                    if (child->style.nodeStructure == sourceOldStyle) {
                        child->style.nodeStructure = targetStyle;
                        propagateStyle(child, targetStyle, sourceOldStyle);
                    }
                }
            };
            
            propagateStyle(node, newStyle, oldStyle);
            
            Canvas_Invalidate();
        }
    }
}
