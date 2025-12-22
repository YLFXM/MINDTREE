#include "MindMapData.h"
#include "commands.h"
#include "MindMapManager.h"
#include "MindMapData.h"
#include "framework.h"
#include "page_system.h"
#include <shellapi.h>
#include <string>
#include <commdlg.h>
#include "canvas_stub.h"
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
    Canvas_Invalidate();
}

void HandleWorkmate(HWND hWnd) {
    HistorySystem::RecordScope rec([] { return MindMapManager::Instance().CreateSnapshot(); });
    std::wstring text = PromptForString(hWnd, L"输入同级主题内容", L"同级主题");
    if (text.empty()) return;
    MindMapManager::Instance().AddSiblingNodeToSelected(text);
    Canvas_Invalidate();
}

void HandleParent(HWND hWnd) {
    HistorySystem::RecordScope rec([] { return MindMapManager::Instance().CreateSnapshot(); });
    std::wstring text = PromptForString(hWnd, L"输入父主题内容", L"父主题");
    if (text.empty()) return;
    MindMapManager::Instance().AddParentNodeToSelected(text);
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
    OPENFILENAME ofn; wchar_t szFile[MAX_PATH] = {0}; ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn); ofn.hwndOwner = hWnd; ofn.lpstrFile = szFile; ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_OVERWRITEPROMPT; ofn.lpstrFilter = L"PNG Files\0*.png\0JPEG Files\0*.jpg;*.jpeg\0All Files\0*.*\0";
    if (GetSaveFileName(&ofn)) {
        std::wstring saved = ofn.lpstrFile;
        // 创建空文件占位，避免路径不存在
        HANDLE hFile = CreateFileW(saved.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile != INVALID_HANDLE_VALUE) {
            CloseHandle(hFile);
        }
        std::wstring msg = std::wstring(L"画布文件已创建: ") + saved;
        int res = MessageBox(hWnd, msg.c_str(), L"新建绘图", MB_YESNO | MB_ICONQUESTION);
        // 询问是否立刻用系统默认程序打开
        if (res == IDYES) {
            ShellExecuteW(NULL, L"open", saved.c_str(), NULL, NULL, SW_SHOWNORMAL);
        }
    }
}

void HandleNewPaintWithTopic(HWND hWnd) {
    HistorySystem::RecordScope rec([] { return MindMapManager::Instance().CreateSnapshot(); });
    if (!global::G_TREEVIEW) return;
    HTREEITEM hSel = TreeView_GetSelection(global::G_TREEVIEW);
    if (!hSel) { MessageBox(hWnd, L"请选择主题以新建关联绘图。", L"提示", MB_OK); return; }
    OPENFILENAME ofn; wchar_t szFile[MAX_PATH] = {0}; ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn); ofn.hwndOwner = hWnd; ofn.lpstrFile = szFile; ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_OVERWRITEPROMPT; ofn.lpstrFilter = L"PNG Files\0*.png\0All Files\0*.*\0";
    if (GetSaveFileName(&ofn)) {
        MindNode* node = MindMapManager::Instance().GetNodeFromHandle(hSel);
        if (!node) return;
        node->paintPath = ofn.lpstrFile;
        {
            // 创建空文件占位，确保路径存在
            HANDLE hFile = CreateFileW(node->paintPath.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
            if (hFile != INVALID_HANDLE_VALUE) {
                CloseHandle(hFile);
            }
            std::wstring msg = std::wstring(L"已为主题创建画布: ") + node->paintPath;
            int res = MessageBox(hWnd, msg.c_str(), L"新建绘图", MB_YESNO | MB_ICONQUESTION);
            // 通知画布加载；必要时立即打开外部程序
            Canvas_LoadPaintForNode(node);
            if (res == IDYES) {
                ShellExecuteW(NULL, L"open", node->paintPath.c_str(), NULL, NULL, SW_SHOWNORMAL);
            }
        }
    }
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
    const int W = 360, H = 140;
    HWND hDlg = CreateWindowEx(WS_EX_DLGMODALFRAME, L"STATIC", title.c_str(), WS_POPUP | WS_VISIBLE | WS_CAPTION | WS_SYSMENU,
        CW_USEDEFAULT, CW_USEDEFAULT, W, H, hWnd, NULL, global::hInst, NULL);
    if (!hDlg) return L"";
    SetWindowLongPtr(hDlg, GWLP_WNDPROC, (LONG_PTR)PromptWndProc);

    HWND hEdit = CreateWindowEx(0, L"EDIT", initial.c_str(), WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
        10, 40, W - 20, 24, hDlg, NULL, global::hInst, NULL);
    HWND hOk = CreateWindowEx(0, L"BUTTON", L"OK", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
        W/2 - 80, 90, 70, 24, hDlg, (HMENU)1, global::hInst, NULL);
    HWND hCancel = CreateWindowEx(0, L"BUTTON", L"Cancel", WS_CHILD | WS_VISIBLE,
        W/2 + 10, 90, 70, 24, hDlg, (HMENU)2, global::hInst, NULL);
    RECT rcOwner; GetWindowRect(hWnd, &rcOwner); int x = rcOwner.left + (rcOwner.right - rcOwner.left - W) / 2;
    int y = rcOwner.top + (rcOwner.bottom - rcOwner.top - H) / 2; SetWindowPos(hDlg, HWND_TOP, x, y, 0, 0, SWP_NOSIZE);
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
