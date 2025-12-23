// Canvas implementation replacing stub
#include "MindMapData.h"
#include "canvas_impl.h"
#include "resolve.h" // Added include
#include <windows.h>
#include <shellapi.h>
#include <windowsx.h>
#include <string>
#include <vector>
#include <sstream>
#include <algorithm> // Added for max
#include <map>       // Added for map
#include <cmath>
#include "MindMapManager.h"
#include "MINDTREE.h"
#include "commands.h"
#include "Resource.h" // Added include for IDM_ constants

#ifndef GET_X_LPARAM
#define GET_X_LPARAM(lp) ((int)(short)LOWORD(lp))
#endif
#ifndef GET_Y_LPARAM
#define GET_Y_LPARAM(lp) ((int)(short)HIWORD(lp))
#endif

static int g_zoom = 100;
static HTREEITEM g_filterRoot = NULL;
static HWND g_canvasWnd = NULL;
static const wchar_t* CANVAS_CLASS_NAME = L"MINDTREE_CANVAS_IMPL";
static HCURSOR g_linkCursor = NULL;

static bool g_isDragging = false;
static POINT g_dragStart = { 0, 0 };
static POINT g_dragEnd = { 0, 0 };

static int g_panOffsetX = 0;
static int g_panOffsetY = 0;
static bool g_isPanning = false;
static POINT g_lastMousePos = { 0, 0 };

// Node dragging state
static bool g_isNodeDragging = false;
struct DragNodeInfo {
    MindNode* node;
    POINT startPos;
    bool wasManual;
};
static std::vector<DragNodeInfo> g_dragNodes;
static POINT g_dragMouseStartPos = { 0, 0 };
static MindNode* g_clickedNode = nullptr; // Track which node was clicked for drag

// Layout constants
static const int LAYOUT_H_GAP = 60;
static const int LAYOUT_V_GAP = 20;

// Edit Control State
static HWND g_hEditNode = NULL;
static WNDPROC g_oldEditProc = NULL;
static MindNode* g_editingNode = nullptr;

// Note Popup State
static HWND g_hNotePopup = NULL;
static MindNode* g_notePopupNode = nullptr;
static const wchar_t* NOTE_POPUP_CLASS = L"MINDTREE_NOTE_POPUP";

// Summary Popup State
static HWND g_hSummaryPopup = NULL;
static MindNode* g_summaryPopupNode = nullptr;
static const wchar_t* SUMMARY_POPUP_CLASS = L"MINDTREE_SUMMARY_POPUP";

// Info Popup State (Label/Link)
static const wchar_t* INFO_POPUP_CLASS = L"MINDTREE_INFO_POPUP";
static HWND g_hLabelPopup = NULL;
static HWND g_hLinkPopup = NULL;
static MindNode* g_hoverNode = nullptr;
static UINT_PTR g_hoverTimerId = 0;
static POINT g_hoverPoint = { 0, 0 };

static void CloseNotePopup() {
    if (g_hNotePopup && IsWindow(g_hNotePopup)) {
        DestroyWindow(g_hNotePopup);
    }
    g_hNotePopup = NULL;
    g_notePopupNode = nullptr;
}

static void CloseSummaryPopup() {
    if (g_hSummaryPopup && IsWindow(g_hSummaryPopup)) {
        DestroyWindow(g_hSummaryPopup);
    }
    g_hSummaryPopup = NULL;
    g_summaryPopupNode = nullptr;
}

static void CloseInfoPopups() {
    if (g_hLabelPopup && IsWindow(g_hLabelPopup)) DestroyWindow(g_hLabelPopup);
    if (g_hLinkPopup && IsWindow(g_hLinkPopup)) DestroyWindow(g_hLinkPopup);
    g_hLabelPopup = NULL;
    g_hLinkPopup = NULL;
}

static LRESULT CALLBACK NotePopupWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        RECT rc; GetClientRect(hWnd, &rc);
        
        // Draw yellow background
        HBRUSH hbr = CreateSolidBrush(RGB(255, 255, 225));
        FillRect(hdc, &rc, hbr);
        DeleteObject(hbr);
        
        // Draw border
        FrameRect(hdc, &rc, (HBRUSH)GetStockObject(BLACK_BRUSH));
        
        // Draw text
        if (g_notePopupNode) {
            InflateRect(&rc, -5, -5);
            SetBkMode(hdc, TRANSPARENT);
            DrawTextW(hdc, g_notePopupNode->note.c_str(), -1, &rc, DT_LEFT | DT_WORDBREAK | DT_NOPREFIX);
        }
        
        EndPaint(hWnd, &ps);
        return 0;
    }
    case WM_LBUTTONDBLCLK: {
        // Edit note
        if (g_notePopupNode) {
            // Ensure node is selected so HandleNote works
            MindMapManager::Instance().ClearSelection();
            MindMapManager::Instance().AddToSelection(g_notePopupNode);
            if (global::G_TREEVIEW && g_notePopupNode->hTreeItem) {
                TreeView_SelectItem(global::G_TREEVIEW, g_notePopupNode->hTreeItem);
            }
            
            // Close popup first
            CloseNotePopup();
            
            // Trigger note editing
            SendMessage(global::HOME, WM_COMMAND, IDM_NOTE, 0);
        }
        return 0;
    }
    case WM_MOUSELEAVE: {
        // Check if mouse moved to node
        POINT pt; GetCursorPos(&pt);
        if (g_canvasWnd) {
            ScreenToClient(g_canvasWnd, &pt);
            // We need to hit test again or just rely on Canvas WM_MOUSEMOVE
            // But if we are here, mouse left popup.
            // If it went to canvas, Canvas will get WM_MOUSEMOVE.
            // If it went outside, we should close.
            // Let's just close if not over canvas?
            // Actually, simpler: Close if not over popup AND not over node.
            // But we don't have node rect here easily without recalculating.
            // Let's rely on CanvasWndProc to keep it open or close it.
            // But if mouse leaves popup to outside app, Canvas won't get msg.
            // So we should close if mouse is not over canvas.
            RECT rcCanvas; GetWindowRect(g_canvasWnd, &rcCanvas);
            POINT screenPt; GetCursorPos(&screenPt);
            if (!PtInRect(&rcCanvas, screenPt)) {
                CloseNotePopup();
            }
        }
        return 0;
    }
    case WM_MOUSEMOVE: {
        // Track mouse leave
        TRACKMOUSEEVENT tme;
        tme.cbSize = sizeof(tme);
        tme.dwFlags = TME_LEAVE;
        tme.hwndTrack = hWnd;
        TrackMouseEvent(&tme);
        return 0;
    }
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

static void ShowNotePopup(MindNode* node, const RECT& nodeRect) {
    if (!node || node->note.empty()) return;
    if (g_hNotePopup && g_notePopupNode == node) return; // Already showing for this node
    
    CloseNotePopup();
    g_notePopupNode = node;
    
    // Calculate size with fixed width 80
    HDC hdc = GetDC(g_canvasWnd);
    // Fixed width 80, padding 5 on each side -> text width 70
    RECT rcText = { 0, 0, 70, 0 }; 
    DrawTextW(hdc, node->note.c_str(), -1, &rcText, DT_CALCRECT | DT_WORDBREAK | DT_NOPREFIX);
    ReleaseDC(g_canvasWnd, hdc);
    
    int w = 80;
    int h = rcText.bottom - rcText.top + 20; // Increased padding (10 top + 10 bottom)
    if (h < 40) h = 40; // Minimum height
    
    POINT pt = { nodeRect.left, nodeRect.top - h };
    ClientToScreen(g_canvasWnd, &pt);
    
    g_hNotePopup = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW, NOTE_POPUP_CLASS, L"", 
        WS_POPUP | WS_VISIBLE | WS_BORDER, 
        pt.x, pt.y, w, h, 
        g_canvasWnd, NULL, global::hInst, NULL);
}

static LRESULT CALLBACK SummaryPopupWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        RECT rc; GetClientRect(hWnd, &rc);
        
        HBRUSH hbr = CreateSolidBrush(RGB(255, 255, 225));
        FillRect(hdc, &rc, hbr);
        DeleteObject(hbr);
        
        FrameRect(hdc, &rc, (HBRUSH)GetStockObject(BLACK_BRUSH));
        
        if (g_summaryPopupNode) {
            RECT rcText = rc;
            rcText.top += 10;
            rcText.bottom -= 10;
            SetBkMode(hdc, TRANSPARENT);
            DrawTextW(hdc, g_summaryPopupNode->summary.c_str(), -1, &rcText, DT_LEFT | DT_WORDBREAK | DT_NOPREFIX);
        }
        
        EndPaint(hWnd, &ps);
        return 0;
    }
    case WM_LBUTTONDBLCLK: {
        if (g_summaryPopupNode) {
            MindMapManager::Instance().ClearSelection();
            MindMapManager::Instance().AddToSelection(g_summaryPopupNode);
            if (global::G_TREEVIEW && g_summaryPopupNode->hTreeItem) {
                TreeView_SelectItem(global::G_TREEVIEW, g_summaryPopupNode->hTreeItem);
            }
            
            CloseSummaryPopup();
            SendMessage(global::HOME, WM_COMMAND, IDM_SUMMRY, 0);
        }
        return 0;
    }
    case WM_MOUSELEAVE: {
        if (g_canvasWnd) {
            RECT rcCanvas; GetWindowRect(g_canvasWnd, &rcCanvas);
            POINT screenPt; GetCursorPos(&screenPt);
            if (!PtInRect(&rcCanvas, screenPt)) {
                CloseSummaryPopup();
            }
        }
        return 0;
    }
    case WM_MOUSEMOVE: {
        TRACKMOUSEEVENT tme;
        tme.cbSize = sizeof(tme);
        tme.dwFlags = TME_LEAVE;
        tme.hwndTrack = hWnd;
        TrackMouseEvent(&tme);
        return 0;
    }
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

static void ShowSummaryPopup(MindNode* node, const RECT& nodeRect) {
    if (!node || node->summary.empty()) return;
    if (g_hSummaryPopup && g_summaryPopupNode == node) return;
    
    CloseSummaryPopup();
    g_summaryPopupNode = node;
    
    HDC hdc = GetDC(g_canvasWnd);
    RECT rcText = { 0, 0, 100, 0 }; 
    DrawTextW(hdc, node->summary.c_str(), -1, &rcText, DT_CALCRECT | DT_WORDBREAK | DT_NOPREFIX);
    ReleaseDC(g_canvasWnd, hdc);
    
    int w = 100;
    int h = rcText.bottom - rcText.top + 20; 
    
    POINT pt = { nodeRect.left, nodeRect.bottom };
    ClientToScreen(g_canvasWnd, &pt);
    
    g_hSummaryPopup = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW, SUMMARY_POPUP_CLASS, L"", 
        WS_POPUP | WS_VISIBLE | WS_BORDER, 
        pt.x, pt.y, w, h, 
        g_canvasWnd, NULL, global::hInst, NULL);
}

static LRESULT CALLBACK InfoPopupWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_PAINT) {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        RECT rc; GetClientRect(hWnd, &rc);
        
        HBRUSH hbr = CreateSolidBrush(RGB(255, 255, 225));
        FillRect(hdc, &rc, hbr);
        DeleteObject(hbr);
        FrameRect(hdc, &rc, (HBRUSH)GetStockObject(BLACK_BRUSH));
        
        wchar_t buf[256];
        GetWindowTextW(hWnd, buf, 256);
        
        InflateRect(&rc, -2, -2);
        SetBkMode(hdc, TRANSPARENT);
        DrawTextW(hdc, buf, -1, &rc, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
        
        EndPaint(hWnd, &ps);
        return 0;
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

static void ShowInfoPopups(MindNode* node, POINT pt) {
    CloseInfoPopups();
    if (!node) return;
    
    int x = pt.x + 10;
    int y = pt.y + 10;
    
    // Label
    if (!node->label.empty()) {
        HDC hdc = GetDC(g_canvasWnd);
        RECT rcText = {0,0,0,0};
        DrawTextW(hdc, node->label.c_str(), -1, &rcText, DT_CALCRECT | DT_SINGLELINE | DT_NOPREFIX);
        ReleaseDC(g_canvasWnd, hdc);
        int w = rcText.right - rcText.left + 10;
        if (w > 300) w = 300;
        
        g_hLabelPopup = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW, INFO_POPUP_CLASS, node->label.c_str(),
            WS_POPUP | WS_VISIBLE | WS_BORDER, x, y, w, 20, g_canvasWnd, NULL, global::hInst, NULL);
        y += 20;
    }
    
    // Link
    std::wstring linkText;
    if (node->outlinkTopicId > 0) linkText = L"Topic: " + std::to_wstring(node->outlinkTopicId);
    else if (!node->outlinkURL.empty()) linkText = node->outlinkURL;
    else if (!node->outlinkFile.empty()) linkText = node->outlinkFile;
    else if (!node->outlinkPage.empty()) linkText = L"Page: " + node->outlinkPage;
    
    if (!linkText.empty()) {
        HDC hdc = GetDC(g_canvasWnd);
        RECT rcText = {0,0,0,0};
        DrawTextW(hdc, linkText.c_str(), -1, &rcText, DT_CALCRECT | DT_SINGLELINE | DT_NOPREFIX);
        ReleaseDC(g_canvasWnd, hdc);
        int w = rcText.right - rcText.left + 10;
        if (w > 300) w = 300;
        
        g_hLinkPopup = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW, INFO_POPUP_CLASS, linkText.c_str(),
            WS_POPUP | WS_VISIBLE | WS_BORDER, x, y, w, 20, g_canvasWnd, NULL, global::hInst, NULL);
    }
}

static LRESULT CALLBACK NodeEditProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_KEYDOWN && wParam == VK_RETURN) {
        SetFocus(GetParent(hWnd)); // Trigger KillFocus
        return 0;
    }
    if (msg == WM_KILLFOCUS) {
        if (g_editingNode) {
            int len = GetWindowTextLengthW(hWnd);
            std::vector<wchar_t> buf(len + 1);
            GetWindowTextW(hWnd, buf.data(), len + 1);
            g_editingNode->text = buf.data();
            
            if (global::G_TREEVIEW && g_editingNode->hTreeItem) {
                TVITEM tvi{};
                tvi.hItem = g_editingNode->hTreeItem;
                tvi.mask = TVIF_TEXT;
                tvi.pszText = buf.data();
                TreeView_SetItem(global::G_TREEVIEW, &tvi);
            }
            Canvas_CalculateNodeSize(g_editingNode);
            Canvas_Invalidate();
        }
        DestroyWindow(hWnd);
        g_hEditNode = NULL;
        g_editingNode = nullptr;
        return 0;
    }
    return CallWindowProc(g_oldEditProc, hWnd, msg, wParam, lParam);
}

// Helper: flatten tree into vector
static void LocalFlatten(MindNode* node, std::vector<MindNode*>& out) {
    if (!node) return;
    out.push_back(node);
    for (MindNode* c : node->children) LocalFlatten(c, out);
}

// Helper: find TreeView item by node ID in current tree
static HTREEITEM FindTreeItemByID(HWND hTreeView, int nodeId, HTREEITEM hStart = TVI_ROOT) {
    if (!hTreeView) return nullptr;
    
    HTREEITEM hItem = (hStart == TVI_ROOT) ? TreeView_GetRoot(hTreeView) : TreeView_GetChild(hTreeView, hStart);
    while (hItem) {
        TVITEM tvi{};
        tvi.mask = TVIF_PARAM;
        tvi.hItem = hItem;
        if (TreeView_GetItem(hTreeView, &tvi)) {
            if ((int)tvi.lParam == nodeId) {
                return hItem;
            }
        }
        
        HTREEITEM hFound = FindTreeItemByID(hTreeView, nodeId, hItem);
        if (hFound) return hFound;
        
        hItem = TreeView_GetNextSibling(hTreeView, hItem);
    }
    return nullptr;
}

// Forward
static void Canvas_EnsureWindow();
static LRESULT CALLBACK CanvasWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

static void Canvas_ShowNodeInfo(HWND hWnd, MindNode* n) {
    if (!n) return;
    std::wstringstream ss;
    ss << L"ID: " << n->id << L"\n";
    ss << L"标题: " << n->text << L"\n";
    ss << L"标签: " << n->label << L"\n";
    ss << L"笔记: " << n->note << L"\n";
    ss << L"概要: " << n->summary << L"\n";
    ss << L"外链(URL): " << n->outlinkURL << L"\n";
    ss << L"外链文件: " << n->outlinkFile << L"\n";
    ss << L"外链页面: " << n->outlinkPage << L"\n";
    ss << L"框架: " << n->frameStyle << L"\n";
    ss << L"画布: " << n->paintPath << L"\n";
    MessageBoxW(hWnd, ss.str().c_str(), L"节点信息", MB_OK | MB_ICONINFORMATION);
}

struct NodeDraw {
    MindNode* node;
    int depth;
    int order;
};

static void BuildLayout(MindNode* node, int depth, int& order, std::vector<NodeDraw>& out) {
    if (!node) return;
    out.push_back({ node, depth, order++ });
    for (MindNode* c : node->children) {
        BuildLayout(c, depth + 1, order, out);
    }
}

static void BuildLayoutFromRoots(const std::vector<MindNode*>& roots, std::vector<NodeDraw>& out) {
    int order = 0;
    for (MindNode* r : roots) {
        BuildLayout(r, 0, order, out);
    }
}

static COLORREF HexToRGB(const std::wstring& hex) {
    if (hex.empty()) return RGB(0, 0, 0);
    const wchar_t* p = hex.c_str();
    if (*p == L'#') p++;
    wchar_t* end = nullptr;
    unsigned int val = wcstoul(p, &end, 16);
    return RGB((val >> 16) & 0xFF, (val >> 8) & 0xFF, val & 0xFF);
}

// Layout helper structure
struct LayoutInfo {
    int width;
    int height;
    int subtreeHeight;
    int x;
    int y;
    int direction; // 1: Right, -1: Left
};

// Calculate subtree height
static int CalculateSubtreeHeight(MindNode* node, std::map<MindNode*, LayoutInfo>& infoMap, int zoom) {
    if (!node) return 0;
    
    double scale = (double)zoom / 100.0;
    int w = 100;
    int h = 38;
    if (node->style.width > 0) w = node->style.width;
    if (node->style.height > 0) h = node->style.height;
    
    // Apply zoom to node size
    int rw = (int)(w * scale);
    int rh = (int)(h * scale);

    LayoutInfo& info = infoMap[node];
    info.width = rw;
    info.height = rh;

    if (node->children.empty()) {
        info.subtreeHeight = rh + (int)(LAYOUT_V_GAP * scale);
        return info.subtreeHeight;
    }

    int childrenHeight = 0;
    for (MindNode* child : node->children) {
        childrenHeight += CalculateSubtreeHeight(child, infoMap, zoom);
    }

    // Subtree height is max of node height and children height
    info.subtreeHeight = max(rh + (int)(LAYOUT_V_GAP * scale), childrenHeight);
    return info.subtreeHeight;
}

// Assign positions recursively
static void AssignPositions(MindNode* node, int x, int y, int direction, std::map<MindNode*, LayoutInfo>& infoMap, int zoom) {
    if (!node) return;
    
    LayoutInfo& info = infoMap[node];
    
    // If manually positioned, use the stored coordinates
    if (node->style.manualPosition) {
        double scale = (double)zoom / 100.0;
        info.x = (int)(node->style.x * scale) + g_panOffsetX;
        info.y = (int)(node->style.y * scale) + g_panOffsetY;
        // Keep the direction passed from parent or default
        // If we wanted to be smarter, we could recalculate direction based on parent pos
    } else {
        info.x = x;
        info.y = y;
    }
    info.direction = direction;

    if (node->children.empty()) return;

    double scale = (double)zoom / 100.0;
    int hGap = (int)(LAYOUT_H_GAP * scale);
    
    // Calculate starting Y for children to center them vertically relative to parent
    int currentY = y - info.subtreeHeight / 2;
    
    // Adjust currentY to start of children block
    // The children block height is sum of children's subtree heights
    int childrenBlockHeight = 0;
    for (MindNode* child : node->children) {
        childrenBlockHeight += infoMap[child].subtreeHeight;
    }
    
    // Center the children block relative to the parent's center Y
    currentY = y - childrenBlockHeight / 2;

    for (MindNode* child : node->children) {
        LayoutInfo& childInfo = infoMap[child];
        int childY = currentY + childInfo.subtreeHeight / 2;
        
        // Child X is parent X + direction * (parent_half_width + gap + child_half_width)
        int childX = x + direction * (info.width / 2 + hGap + childInfo.width / 2);
        
        AssignPositions(child, childX, childY, direction, infoMap, zoom);
        currentY += childInfo.subtreeHeight;
    }
}

static void ComputeNodeRects(const std::vector<MindNode*>& drawRoots, int zoom, const RECT& rcClient, std::vector<NodeDraw>& layout, std::vector<RECT>& rects, std::vector<POINT>& centers) {
    layout.clear();
    rects.clear();
    centers.clear();
    
    // We need to rebuild the layout vector to match the drawing order or just use it for hit testing
    // The original BuildLayoutFromRoots was depth-first. We can still use it to populate the layout vector
    // but we will calculate positions differently.
    BuildLayoutFromRoots(drawRoots, layout);

    if (drawRoots.empty()) return;

    std::map<MindNode*, LayoutInfo> infoMap;
    double scale = (double)zoom / 100.0;
    int centerX = (rcClient.right - rcClient.left) / 2;
    int centerY = (rcClient.bottom - rcClient.top) / 2;

    // Process each root (usually just one main root)
    for (MindNode* root : drawRoots) {
        // 1. Calculate sizes
        // For the root, we need to handle Level 1 specially (Left/Right distribution)
        
        int w = 100; int h = 38;
        if (root->style.width > 0) w = root->style.width;
        if (root->style.height > 0) h = root->style.height;
        int rw = (int)(w * scale);
        int rh = (int)(h * scale);
        
        LayoutInfo& rootInfo = infoMap[root];
        rootInfo.width = rw;
        rootInfo.height = rh;
        
        // Split children into Left and Right groups
        std::vector<MindNode*> leftChildren;
        std::vector<MindNode*> rightChildren;
        
        for (size_t i = 0; i < root->children.size(); ++i) {
            if (i % 2 == 0) rightChildren.push_back(root->children[i]);
            else leftChildren.push_back(root->children[i]);
        }

        // Calculate heights for subtrees
        int rightHeight = 0;
        for (MindNode* c : rightChildren) {
            rightHeight += CalculateSubtreeHeight(c, infoMap, zoom);
        }
        int leftHeight = 0;
        for (MindNode* c : leftChildren) {
            leftHeight += CalculateSubtreeHeight(c, infoMap, zoom);
        }

        // 2. Assign Positions
        // Root position: Use style.x/y if set (dragging), else Center
        int rootX = centerX;
        int rootY = centerY;
        
        if (root->style.manualPosition) {
            rootX = (int)(root->style.x * scale) + g_panOffsetX;
            rootY = (int)(root->style.y * scale) + g_panOffsetY;
        } else {
            rootX += g_panOffsetX;
            rootY += g_panOffsetY;
        }
        
        rootInfo.x = rootX;
        rootInfo.y = rootY;
        rootInfo.direction = 0; // Root has no single direction

        int hGap = (int)(LAYOUT_H_GAP * scale);

        // Layout Right side
        int currentY = rootY - rightHeight / 2;
        for (MindNode* c : rightChildren) {
            LayoutInfo& cInfo = infoMap[c];
            int childY = currentY + cInfo.subtreeHeight / 2;
            int childX = rootX + (rootInfo.width / 2 + hGap + cInfo.width / 2);
            AssignPositions(c, childX, childY, 1, infoMap, zoom);
            currentY += cInfo.subtreeHeight;
        }

        // Layout Left side
        currentY = rootY - leftHeight / 2;
        for (MindNode* c : leftChildren) {
            LayoutInfo& cInfo = infoMap[c];
            int childY = currentY + cInfo.subtreeHeight / 2;
            int childX = rootX - (rootInfo.width / 2 + hGap + cInfo.width / 2);
            AssignPositions(c, childX, childY, -1, infoMap, zoom);
            currentY += cInfo.subtreeHeight;
        }
    }

    // 3. Populate rects and centers based on calculated info
    for (const auto& nd : layout) {
        if (infoMap.find(nd.node) != infoMap.end()) {
            LayoutInfo& info = infoMap[nd.node];
            centers.push_back({ info.x, info.y });
            RECT box{ info.x - info.width / 2, info.y - info.height / 2, info.x + info.width / 2, info.y + info.height / 2 };
            rects.push_back(box);
            
            // Update node style position to match layout (so dragging starts from correct place)
            // Only update if not currently dragging this specific node to avoid jitter?
            // Actually, if we want "recalculate", we should update.
            // But if we update style.x/y, then next frame root uses it.
            // For children, we ignore style.x/y in calculation, so updating it is fine (it's just a cache).
            // For root, we use it.
            if (!g_isNodeDragging || g_dragNodes.empty() || std::find_if(g_dragNodes.begin(), g_dragNodes.end(), [&](const DragNodeInfo& info) { return info.node == nd.node; }) == g_dragNodes.end()) {
                 // Update the cache, but don't set manualPosition flag here
                 nd.node->style.x = (int)((info.x - g_panOffsetX) / scale);
                 nd.node->style.y = (int)((info.y - g_panOffsetY) / scale);
            }
        } else {
            // Fallback
            centers.push_back({ 0, 0 });
            rects.push_back({ 0, 0, 0, 0 });
        }
    }
}

static void ExecuteLink(HWND hWnd, MindNode* n, int type, const std::vector<NodeDraw>& layout, const RECT& rc) {
    if (type == 1) { // Topic
        MindNode* target = MindMapManager::Instance().FindNodeById(MindMapManager::Instance().GetRoot(), n->outlinkTopicId);
        if (target) {
            for (size_t j = 0; j < layout.size(); ++j) {
                if (layout[j].node == target) {
                    double scale = (double)Canvas_GetZoom() / 100.0;
                    int tx = (int)((80 + layout[j].depth * 160) * scale);
                    int ty = (int)((60 + layout[j].order * 80) * scale);
                    g_panOffsetX = (rc.right - rc.left) / 2 - tx;
                    g_panOffsetY = (rc.bottom - rc.top) / 2 - ty;
                    MindMapManager::Instance().ClearSelection();
                    MindMapManager::Instance().AddToSelection(target);
                    if (global::G_TREEVIEW && target->hTreeItem) {
                        TreeView_SelectItem(global::G_TREEVIEW, target->hTreeItem);
                    }
                    Canvas_Invalidate();
                    break;
                }
            }
        }
    } else if (type == 2) { // URL
        ShellExecuteW(NULL, L"open", n->outlinkURL.c_str(), NULL, NULL, SW_SHOWNORMAL);
    } else if (type == 3) { // File
        ShellExecuteW(NULL, L"open", n->outlinkFile.c_str(), NULL, NULL, SW_SHOWNORMAL);
    }
}

static void Canvas_EnsureWindow() {
    if (g_canvasWnd && IsWindow(g_canvasWnd)) return;
    if (!global::HOME || !IsWindow(global::HOME)) return;

    WNDCLASSW wc = {0};
    if (!GetClassInfoW(global::hInst, CANVAS_CLASS_NAME, &wc)) {
        wc.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
        wc.lpfnWndProc = CanvasWndProc;
        wc.hInstance = global::hInst;
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        wc.lpszClassName = CANVAS_CLASS_NAME;
        RegisterClassW(&wc);
    }
    
    // Register Note Popup Class
    WNDCLASSW wcNote = {0};
    if (!GetClassInfoW(global::hInst, NOTE_POPUP_CLASS, &wcNote)) {
        wcNote.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
        wcNote.lpfnWndProc = NotePopupWndProc;
        wcNote.hInstance = global::hInst;
        wcNote.hCursor = LoadCursor(NULL, IDC_ARROW);
        wcNote.lpszClassName = NOTE_POPUP_CLASS;
        wcNote.hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH);
        RegisterClassW(&wcNote);
    }

    // Register Summary Popup Class
    WNDCLASSW wcSum = {0};
    if (!GetClassInfoW(global::hInst, SUMMARY_POPUP_CLASS, &wcSum)) {
        wcSum.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
        wcSum.lpfnWndProc = SummaryPopupWndProc;
        wcSum.hInstance = global::hInst;
        wcSum.hCursor = LoadCursor(NULL, IDC_ARROW);
        wcSum.lpszClassName = SUMMARY_POPUP_CLASS;
        wcSum.hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH);
        RegisterClassW(&wcSum);
    }

    // Register Info Popup Class
    WNDCLASSW wcInfo = {0};
    if (!GetClassInfoW(global::hInst, INFO_POPUP_CLASS, &wcInfo)) {
        wcInfo.style = CS_HREDRAW | CS_VREDRAW;
        wcInfo.lpfnWndProc = InfoPopupWndProc;
        wcInfo.hInstance = global::hInst;
        wcInfo.hCursor = LoadCursor(NULL, IDC_ARROW);
        wcInfo.lpszClassName = INFO_POPUP_CLASS;
        wcInfo.hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH);
        RegisterClassW(&wcInfo);
    }

    RECT rc; GetClientRect(global::HOME, &rc);
    g_canvasWnd = CreateWindowExW(0, CANVAS_CLASS_NAME, L"", WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
        200 * IsWindowVisible(global::GUIDE), 50 * IsWindowVisible(global::TOOLS), rc.right - rc.left - 200 * (IsWindowVisible(global::GUIDE) 
            + IsWindowVisible(global::STYLE)), rc.bottom - rc.top - 50 * IsWindowVisible(global::TOOLS), global::HOME, NULL, global::hInst, NULL);
}

static void Canvas_EditNode(HWND hWnd, MindNode* n, const RECT& rcNode) {
    if (!n) return;
    
    g_editingNode = n;
    int w = rcNode.right - rcNode.left;
    int h = rcNode.bottom - rcNode.top;
    
    int zoom = Canvas_GetZoom();
    int fontSize = n->style.fontSize > 0 ? n->style.fontSize : 12;
    fontSize = (int)(fontSize * ((double)zoom / 100.0));
    if (fontSize < 8) fontSize = 8;
    
    // Create Edit Control centered vertically
    int editH = fontSize * 3 / 2 + 4;
    if (editH > h) editH = h;
    int editY = rcNode.top + (h - editH) / 2;

    g_hEditNode = CreateWindowW(L"EDIT", n->text.c_str(), 
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_CENTER | ES_AUTOHSCROLL,
        rcNode.left, editY, w, editH,
        hWnd, NULL, global::hInst, NULL);
    
    int weight = n->style.bold ? FW_BOLD : FW_NORMAL;
    HFONT hFont = CreateFontW(fontSize, 0, 0, 0, weight, n->style.italic, n->style.underline, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, n->style.fontFamily.empty() ? L"Arial" : n->style.fontFamily.c_str());
    SendMessage(g_hEditNode, WM_SETFONT, (WPARAM)hFont, TRUE);
    
    g_oldEditProc = (WNDPROC)SetWindowLongPtr(g_hEditNode, GWLP_WNDPROC, (LONG_PTR)NodeEditProc);
    SetFocus(g_hEditNode);
    SendMessage(g_hEditNode, EM_SETSEL, 0, -1);
}

void Canvas_CalculateNodeSize(MindNode* node) {
    if (!node) return;
    HDC hdc = GetDC(g_canvasWnd ? g_canvasWnd : NULL);
    
    int fontSize = node->style.fontSize > 0 ? node->style.fontSize : 12;
    int weight = node->style.bold ? FW_BOLD : FW_NORMAL;
    DWORD italic = node->style.italic ? TRUE : FALSE;
    DWORD underline = node->style.underline ? TRUE : FALSE;
    int lfHeight = -MulDiv(fontSize, GetDeviceCaps(hdc, LOGPIXELSY), 96);
    
    HFONT hFont = CreateFontW(lfHeight, 0, 0, 0, weight, italic, underline, 0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, node->style.fontFamily.empty() ? L"Arial" : node->style.fontFamily.c_str());
    
    HGDIOBJ oldFont = SelectObject(hdc, hFont);
    
    RECT rc = {0, 0, 0, 0};
    std::wstring text = node->text.empty() ? (L"ID " + std::to_wstring(node->id)) : node->text;
    DrawTextW(hdc, text.c_str(), (int)text.size(), &rc, DT_CALCRECT | DT_SINGLELINE);
    
    SelectObject(hdc, oldFont);
    DeleteObject(hFont);
    ReleaseDC(g_canvasWnd ? g_canvasWnd : NULL, hdc);
    
    int textW = rc.right - rc.left;
    int textH = rc.bottom - rc.top;
    
    // Padding
    int padX = 20;
    int padY = 10;
    
    int w = textW + padX;
    int h = textH + padY;
    
    // Min size
    if (w < 60) w = 60;
    if (h < 28) h = 28;
    
    // Shape adjustment
    if (node->style.shape == L"circle") {
        int d = (int)sqrt(w*w + h*h);
        w = d;
        h = d;
    } else if (node->style.shape == L"ellipse") {
        w = (int)(w * 1.42);
        h = (int)(h * 1.42);
    } else if (node->style.shape == L"diamond") {
        w = w * 2;
        h = h * 2;
    }
    
    node->style.width = w;
    node->style.height = h;
}

static LRESULT CALLBACK CanvasWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE:
        g_linkCursor = LoadCursor(NULL, IDC_CROSS);
        return 0;
    case WM_TIMER:
        if (wParam == 2) {
            KillTimer(hWnd, 2);
            g_hoverTimerId = 0;
            if (g_hoverNode) {
                ShowInfoPopups(g_hoverNode, g_hoverPoint);
            }
        }
        return 0;
    case WM_SETCURSOR:
        if (LinkModeIsActive()) {
            if (g_linkCursor) SetCursor(g_linkCursor); else SetCursor(LoadCursor(NULL, IDC_CROSS));
            return TRUE;
        }
        break;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        RECT rc; GetClientRect(hWnd, &rc);
        HBRUSH hbr = CreateSolidBrush(RGB(250,250,250));
        FillRect(hdc, &rc, hbr);
        DeleteObject(hbr);

        // Removed header drawing
        int zoom = Canvas_GetZoom(); // Re-added zoom variable definition
        // std::wstringstream ss;
        // ss << L"Canvas Impl - Zoom:" << zoom;
        // std::wstring header = ss.str();
        // SetBkMode(hdc, TRANSPARENT);
        // RECT hdr = rc; hdr.bottom = hdr.top + 24;
        // DrawTextW(hdc, header.c_str(), (int)header.size(), &hdr, DT_LEFT | DT_SINGLELINE | DT_TOP);

        int w = rc.right - rc.left;
        int h = rc.bottom - rc.top;
        MindNode* filterNode = nullptr;
        if (g_filterRoot) filterNode = MindMapManager::Instance().GetNodeFromHandle(g_filterRoot);

        std::vector<MindNode*> drawRoots;
        if (filterNode) {
            drawRoots.push_back(filterNode);
        } else {
            MindNode* root = MindMapManager::Instance().GetRoot();
            if (root) drawRoots.push_back(root);
            for (auto fn : MindMapManager::Instance().GetFreeNodes()) {
                drawRoots.push_back(fn);
            }
        }

        MindNode* selectedNode = nullptr;
        if (global::G_TREEVIEW) {
            HTREEITEM hSel = TreeView_GetSelection(global::G_TREEVIEW);
            if (hSel) selectedNode = MindMapManager::Instance().GetNodeFromHandle(hSel);
        }

        std::vector<NodeDraw> layout;
        std::vector<RECT> rects;
        std::vector<POINT> positions;
        ComputeNodeRects(drawRoots, zoom, rc, layout, rects, positions);

        for (size_t i = 0; i < layout.size(); ++i) {
            MindNode* n = layout[i].node;
            if (!n || !n->parent) continue;
            // find parent index in layout
            for (size_t j = 0; j < layout.size(); ++j) {
                if (layout[j].node == n->parent) {
                    MindNode* parent = n->parent;

                    // Use parent's branch style for the connection line
                    int bWidth = parent->style.branchWidth > 0 ? parent->style.branchWidth : 1;
                    COLORREF bColor = HexToRGB(parent->style.branchColor);
                    
                    HPEN hBranchPen = CreatePen(PS_SOLID, bWidth, bColor);
                    HGDIOBJ oldPen = SelectObject(hdc, hBranchPen);

                    // Draw Bezier Curve
                    POINT pStart = positions[j]; // Fixed: centers -> positions
                    POINT pEnd = positions[i];   // Fixed: centers -> positions
                    RECT rStart = rects[j];
                    RECT rEnd = rects[i];

                    // Determine connection points (midpoints of closest edges)
                    POINT startPt, endPt;
                    
                    // Simple logic: if child is to the right, connect ParentRight to ChildLeft
                    if (pEnd.x > pStart.x) {
                        startPt = { rStart.right, pStart.y };
                        endPt = { rEnd.left, pEnd.y };
                    } else {
                        startPt = { rStart.left, pStart.y };
                        endPt = { rEnd.right, pEnd.y };
                    }

                    // Control points for smooth curve
                    // CP1: Start + horizontal offset
                    // CP2: End - horizontal offset
                    int dist = abs(endPt.x - startPt.x);
                    int cpOffset = dist / 2;
                    
                    POINT cp1 = { startPt.x + (pEnd.x > pStart.x ? cpOffset : -cpOffset), startPt.y };
                    POINT cp2 = { endPt.x - (pEnd.x > pStart.x ? cpOffset : -cpOffset), endPt.y };

                    POINT bezier[4] = { startPt, cp1, cp2, endPt };
                    PolyBezier(hdc, bezier, 4);

                    SelectObject(hdc, oldPen);
                    DeleteObject(hBranchPen);
                    break;
                }
            }
        }

        HPEN hLinkPen = CreatePen(PS_DASH, 2, RGB(0, 120, 215));
        HGDIOBJ oldPen = SelectObject(hdc, hLinkPen); // Re-declared oldPen here
        for (size_t i = 0; i < layout.size(); ++i) {
            MindNode* n = layout[i].node;
            if (!n || n->linkedNodeIds.empty()) continue;
            
            for (int linkedId : n->linkedNodeIds) {
                for (size_t j = 0; j < layout.size(); ++j) {
                    if (layout[j].node && layout[j].node->id == linkedId) {
                        MoveToEx(hdc, positions[i].x, positions[i].y, NULL);
                        LineTo(hdc, positions[j].x, positions[j].y);
                        int midX = (positions[i].x + positions[j].x) / 2;
                        int midY = (positions[i].y + positions[j].y) / 2;
                        Ellipse(hdc, midX - 3, midY - 3, midX + 3, midY + 3);
                        break;
                    }
                }
            }
        }
        SelectObject(hdc, oldPen);
        DeleteObject(hLinkPen);

        HPEN hFramePen = CreatePen(PS_SOLID, 1, RGB(255, 100, 100));
        oldPen = SelectObject(hdc, hFramePen);
        SelectObject(hdc, GetStockObject(NULL_BRUSH));
        for (const auto& frame : MindMapManager::Instance().GetFrames()) {
            RECT frameRect = { 10000, 10000, -10000, -10000 };
            bool hasNodes = false;
            for (int id : frame.nodeIds) {
                for (size_t i = 0; i < layout.size(); ++i) {
                    if (layout[i].node && layout[i].node->id == id) {
                        RECT box = rects[i];
                        if (box.left < frameRect.left) frameRect.left = box.left;
                        if (box.top < frameRect.top) frameRect.top = box.top;
                        if (box.right > frameRect.right) frameRect.right = box.right;
                        if (box.bottom > frameRect.bottom) frameRect.bottom = box.bottom;
                        hasNodes = true;
                    }
                }
            }
            if (hasNodes) {
                InflateRect(&frameRect, 10, 10);
                Rectangle(hdc, frameRect.left, frameRect.top, frameRect.right, frameRect.bottom);
            }
        }
        SelectObject(hdc, oldPen);
        DeleteObject(hFramePen);

        int idx = 0;
        for (size_t i = 0; i < layout.size(); ++i) {
            MindNode* n = layout[i].node;
            if (!n) continue;
            RECT box = rects[i];
            bool isSelected = (n == selectedNode) || MindMapManager::Instance().IsNodeSelected(n);
            
            COLORREF fillColor = HexToRGB(n->style.fillColor);
            if (fillColor == RGB(0,0,0) && n->style.fillColor != L"#000000") {
                fillColor = (n == filterNode) ? RGB(255, 245, 200) : RGB(235, 245, 255);
            }
            
            HBRUSH fill = CreateSolidBrush(fillColor);
            FillRect(hdc, &box, fill);
            DeleteObject(fill);
            
            COLORREF borderColor = HexToRGB(n->style.borderColor);
            int borderWidth = n->style.borderWidth > 0 ? n->style.borderWidth : 1;
            int penStyle = PS_SOLID;
            if (n->style.borderStyle == L"dashed") penStyle = PS_DASH;
            else if (n->style.borderStyle == L"dotted") penStyle = PS_DOT;
            
            HPEN borderPen = CreatePen(penStyle, borderWidth, borderColor);
            HGDIOBJ oldPen = SelectObject(hdc, borderPen);
            SelectObject(hdc, GetStockObject(NULL_BRUSH));
            
            if (n->style.shape == L"ellipse" || n->style.shape == L"circle") {
                Ellipse(hdc, box.left, box.top, box.right, box.bottom);
            } else if (n->style.shape == L"rounded_rectangle") {
                int w = box.right - box.left;
                int h = box.bottom - box.top;
                int r = min(w, h) / 4;
                RoundRect(hdc, box.left, box.top, box.right, box.bottom, r, r);
            } else {
                Rectangle(hdc, box.left, box.top, box.right, box.bottom);
            }
            SelectObject(hdc, oldPen);
            DeleteObject(borderPen);

            // Draw selection highlight (outer glow/border)
            if (isSelected) {
                int selWidth = 2;
                HPEN selPen = CreatePen(PS_SOLID, selWidth, RGB(0, 120, 215));
                HGDIOBJ oldSelPen = SelectObject(hdc, selPen);
                SelectObject(hdc, GetStockObject(NULL_BRUSH));
                
                RECT selBox = box;
                InflateRect(&selBox, 3, 3); // Draw slightly outside

                if (n->style.shape == L"ellipse" || n->style.shape == L"circle") {
                    Ellipse(hdc, selBox.left, selBox.top, selBox.right, selBox.bottom);
                } else if (n->style.shape == L"rounded_rectangle") {
                    int w = selBox.right - selBox.left;
                    int h = selBox.bottom - selBox.top;
                    int r = min(w, h) / 4;
                    RoundRect(hdc, selBox.left, selBox.top, selBox.right, selBox.bottom, r, r);
                } else {
                    Rectangle(hdc, selBox.left, selBox.top, selBox.right, selBox.bottom);
                }
                
                SelectObject(hdc, oldSelPen);
                DeleteObject(selPen);
            }
            
            COLORREF textColor = HexToRGB(n->style.textColor);
            
            // Link styling: Sky Blue if any link is present
            bool hasLink = !n->outlinkURL.empty() || !n->outlinkFile.empty() || !n->outlinkPage.empty() || n->outlinkTopicId > 0;
            if (hasLink) {
                textColor = RGB(0, 170, 255);
            }

            SetTextColor(hdc, textColor);
            
            int fontSize = n->style.fontSize > 0 ? n->style.fontSize : 12;
            fontSize = (int)(fontSize * ((double)zoom / 100.0));
            if (fontSize < 8) fontSize = 8;
            
            int weight = n->style.bold ? FW_BOLD : FW_NORMAL;
            
            HFONT hFont = CreateFontW(fontSize, 0, 0, 0, weight, n->style.italic, n->style.underline || hasLink, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, n->style.fontFamily.empty() ? L"Arial" : n->style.fontFamily.c_str());
            HFONT oldFont = (HFONT)SelectObject(hdc, hFont);
            
            SetBkMode(hdc, TRANSPARENT); // Ensure text background is transparent
            std::wstring text = n->text.empty() ? (L"ID " + std::to_wstring(n->id)) : n->text;
            
            UINT format = DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS;
            if (n->style.textAlignment == L"left") format |= DT_LEFT;
            else if (n->style.textAlignment == L"right") format |= DT_RIGHT;
            else format |= DT_CENTER;
            
            // Add padding for left/right alignment to avoid text touching border
            RECT textBox = box;
            if (n->style.textAlignment == L"left") textBox.left += 5;
            else if (n->style.textAlignment == L"right") textBox.right -= 5;
            
            DrawTextW(hdc, text.c_str(), (int)text.size(), &textBox, format);
            
            SelectObject(hdc, oldFont);
            DeleteObject(hFont);
            
            if (++idx > 300) break;
        }

        if (g_isDragging) {
            HPEN hDragPen = CreatePen(PS_DASH, 1, RGB(0, 0, 255));
            HGDIOBJ oldPen = SelectObject(hdc, hDragPen);
            SelectObject(hdc, GetStockObject(NULL_BRUSH));
            Rectangle(hdc, g_dragStart.x, g_dragStart.y, g_dragEnd.x, g_dragEnd.y);
            SelectObject(hdc, oldPen);
            DeleteObject(hDragPen);
        }

        EndPaint(hWnd, &ps);
        return 0;
    }
    case WM_SIZE:
        InvalidateRect(hWnd, NULL, TRUE);
        return 0;
    case WM_LBUTTONDOWN: {
        SetFocus(hWnd);
        CloseInfoPopups();
        if (g_hoverTimerId) { KillTimer(hWnd, g_hoverTimerId); g_hoverTimerId = 0; }
        g_hoverNode = nullptr;

        POINT pt{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        RECT rc; GetClientRect(hWnd, &rc);

        std::vector<MindNode*> drawRoots;
        MindNode* filterNode = nullptr;
        if (g_filterRoot) filterNode = MindMapManager::Instance().GetNodeFromHandle(g_filterRoot);
        if (filterNode) {
            drawRoots.push_back(filterNode);
        } else {
            MindNode* root = MindMapManager::Instance().GetRoot();
            if (root) drawRoots.push_back(root);
            for (auto fn : MindMapManager::Instance().GetFreeNodes()) {
                drawRoots.push_back(fn);
            }
        }
        if (drawRoots.empty()) return 0;
        std::vector<NodeDraw> layout;
        std::vector<RECT> rects;
        std::vector<POINT> centers;
        ComputeNodeRects(drawRoots, Canvas_GetZoom(), rc, layout, rects, centers);

        bool hit = false;
        for (size_t i = 0; i < layout.size(); ++i) {
            if (PtInRect(&rects[i], pt)) {
                MindNode* n = layout[i].node;
                if (!n) continue;
                hit = true;

                // Check for link click on text (if not Ctrl-clicking)
                bool hasLink = !n->outlinkURL.empty() || !n->outlinkFile.empty() || !n->outlinkPage.empty() || n->outlinkTopicId > 0;
                if (hasLink && !(GetKeyState(VK_CONTROL) & 0x8000)) {
                    HDC hdc = GetDC(hWnd);
                    int zoom = Canvas_GetZoom();
                    int fontSize = n->style.fontSize > 0 ? n->style.fontSize : 12;
                    fontSize = (int)(fontSize * ((double)zoom / 100.0));
                    if (fontSize < 8) fontSize = 8;
                    int weight = n->style.bold ? FW_BOLD : FW_NORMAL;
                    HFONT hFont = CreateFontW(fontSize, 0, 0, 0, weight, n->style.italic, TRUE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, n->style.fontFamily.empty() ? L"Arial" : n->style.fontFamily.c_str());
                    HFONT oldFont = (HFONT)SelectObject(hdc, hFont);
                    
                    RECT textRect = {0,0,0,0};
                    std::wstring text = n->text.empty() ? (L"ID " + std::to_wstring(n->id)) : n->text;
                    DrawTextW(hdc, text.c_str(), (int)text.size(), &textRect, DT_CALCRECT | DT_SINGLELINE);
                    
                    SelectObject(hdc, oldFont);
                    DeleteObject(hFont);
                    ReleaseDC(hWnd, hdc);
                    
                    RECT nodeRect = rects[i];
                    int nodeW = nodeRect.right - nodeRect.left;
                    int nodeH = nodeRect.bottom - nodeRect.top;
                    int textW = textRect.right - textRect.left;
                    int textH = textRect.bottom - textRect.top;
                    
                    int textX = nodeRect.left + (nodeW - textW) / 2;
                    int textY = nodeRect.top + (nodeH - textH) / 2;
                    
                    RECT finalTextRect = { textX, textY, textX + textW, textY + textH };
                    
                    if (PtInRect(&finalTextRect, pt)) {
                        if (n->outlinkTopicId > 0) ExecuteLink(hWnd, n, 1, layout, rc);
                        else if (!n->outlinkURL.empty()) ExecuteLink(hWnd, n, 2, layout, rc);
                        else if (!n->outlinkFile.empty()) ExecuteLink(hWnd, n, 3, layout, rc);
                        return 0;
                    }
                }

                // Handle Selection Logic FIRST
                bool isCtrl = (GetKeyState(VK_CONTROL) & 0x8000);
                if (!LinkModeIsActive()) {
                     bool isSelected = MindMapManager::Instance().IsNodeSelected(n);
                     if (!isCtrl) {
                         if (!isSelected) {
                             MindMapManager::Instance().ClearSelection();
                             MindMapManager::Instance().AddToSelection(n);
                         }
                         // If isSelected, keep selection for potential drag
                     }
                     
                     // Sync TreeView
                     if (global::G_TREEVIEW) {
                         HTREEITEM hItem = nullptr;
                         if (n->hTreeItem) {
                             TVITEM tvi{};
                             tvi.mask = TVIF_PARAM;
                             tvi.hItem = n->hTreeItem;
                             if (TreeView_GetItem(global::G_TREEVIEW, &tvi) && (int)tvi.lParam == n->id) {
                                 hItem = n->hTreeItem;
                             }
                         }
                         if (!hItem) hItem = FindTreeItemByID(global::G_TREEVIEW, n->id);
                         if (hItem) {
                             TreeView_SelectItem(global::G_TREEVIEW, hItem);
                             SetFocus(global::G_TREEVIEW);
                             UpdateStylePanelUI(n);
                         }
                     }
                }

                // Check if we should start dragging (if not Ctrl-clicking)
                if (!isCtrl) {
                    if (n == MindMapManager::Instance().GetRoot()) {
                        g_isPanning = true;
                        g_lastMousePos = pt;
                        SetCapture(hWnd);
                    } else {
                        g_isNodeDragging = true;
                        g_clickedNode = n;
                        g_dragMouseStartPos = pt;
                        g_dragNodes.clear();
                        
                        auto selectedNodesSet = MindMapManager::Instance().GetSelectedNodes();
                        std::vector<MindNode*> selectedNodes(selectedNodesSet.begin(), selectedNodesSet.end());
                        
                        // Ensure n is in selectedNodes (it should be due to logic above)
                        bool found = false;
                        for(auto s : selectedNodes) if(s == n) found = true;
                        if(!found) selectedNodes.push_back(n);

                        for (MindNode* s : selectedNodes) {
                             if (s == MindMapManager::Instance().GetRoot()) continue; // Don't drag root
                             
                             bool wasManual = s->style.manualPosition;
                             if (!wasManual) {
                                 // Calculate current position from layout
                                 for(size_t k=0; k<layout.size(); ++k) {
                                     if(layout[k].node == s) {
                                         double scale = (double)Canvas_GetZoom() / 100.0;
                                         int cx = (rects[k].left + rects[k].right) / 2;
                                         int cy = (rects[k].top + rects[k].bottom) / 2;
                                         s->style.x = (int)((cx - g_panOffsetX) / scale);
                                         s->style.y = (int)((cy - g_panOffsetY) / scale);
                                         s->style.manualPosition = true;
                                         break;
                                     }
                                 }
                             }
                             g_dragNodes.push_back({ s, {s->style.x, s->style.y}, wasManual });
                        }
                        SetCapture(hWnd);
                    }
                }

                if (isCtrl) {
                    struct LinkInfo { int type; std::wstring label; };
                    std::vector<LinkInfo> availableLinks;
                    if (n->outlinkTopicId > 0) availableLinks.push_back({ 1, L"跳转到主题" });
                    if (!n->outlinkURL.empty()) availableLinks.push_back({ 2, L"打开网页" });
                    if (!n->outlinkFile.empty()) availableLinks.push_back({ 3, L"打开文件" });

                    if (availableLinks.empty()) return 0;
                    if (availableLinks.size() == 1) {
                        ExecuteLink(hWnd, n, availableLinks[0].type, layout, rc);
                        return 0;
                    } else {
                        HMENU hMenu = CreatePopupMenu();
                        for (size_t i = 0; i < availableLinks.size(); ++i) {
                            AppendMenuW(hMenu, MF_STRING, (UINT_PTR)(i + 1), availableLinks[i].label.c_str());
                        }
                        POINT screenPt = pt;
                        ClientToScreen(hWnd, &screenPt);
                        int sel = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_TOPALIGN, screenPt.x, screenPt.y, 0, hWnd, NULL);
                        DestroyMenu(hMenu);
                        if (sel > 0 && (size_t)sel <= availableLinks.size()) {
                            ExecuteLink(hWnd, n, availableLinks[sel - 1].type, layout, rc);
                        }
                        return 0;
                    }
                }

                if (LinkModeIsActive()) {
                    if (LinkModeTryComplete(n)) {
                        if (global::G_TREEVIEW && n->hTreeItem) {
                            TreeView_SelectItem(global::G_TREEVIEW, n->hTreeItem);
                        }
                        Canvas_Invalidate();
                    }
                }
                break;
            }
        }

        if (!hit) {
            g_isPanning = true;
            g_lastMousePos = pt;
            SetCapture(hWnd);
            
            // Clear selection when clicking on empty space
            MindMapManager::Instance().ClearSelection();
            if (global::G_TREEVIEW) {
                TreeView_SelectItem(global::G_TREEVIEW, NULL);
            }
            Canvas_Invalidate();
        }
        return 0;
    }
    case WM_RBUTTONDOWN: {
        SetFocus(hWnd);
        CloseInfoPopups();
        if (g_hoverTimerId) { KillTimer(hWnd, g_hoverTimerId); g_hoverTimerId = 0; }
        g_hoverNode = nullptr;

        POINT pt{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        g_isDragging = true;
        g_dragStart = pt;
        g_dragEnd = pt;
        SetCapture(hWnd);
        MindMapManager::Instance().ClearSelection();
        Canvas_Invalidate();
        return 0;
    }
    case WM_MOUSEMOVE: {
        POINT pt{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        if (g_isNodeDragging && !g_dragNodes.empty()) {
            double scale = (double)Canvas_GetZoom() / 100.0;
            int dx = pt.x - g_dragMouseStartPos.x;
            int dy = pt.y - g_dragMouseStartPos.y;
            
            for (auto& item : g_dragNodes) {
                item.node->style.x = item.startPos.x + (int)(dx / scale);
                item.node->style.y = item.startPos.y + (int)(dy / scale);
            }
            Canvas_Invalidate();
        } else if (g_isPanning) {
            g_panOffsetX += (pt.x - g_lastMousePos.x);
            g_panOffsetY += (pt.y - g_lastMousePos.y);
            g_lastMousePos = pt;
            Canvas_Invalidate();
        } else if (g_isDragging) {
            g_dragEnd = pt;
            Canvas_Invalidate();
        }
        
        // Note Popup Logic & Info Popup Logic
        if (!g_isNodeDragging && !g_isPanning && !g_isDragging) {
            RECT rc; GetClientRect(hWnd, &rc);
            std::vector<MindNode*> drawRoots;
            MindNode* filterNode = nullptr;
            if (g_filterRoot) filterNode = MindMapManager::Instance().GetNodeFromHandle(g_filterRoot);
            if (filterNode) {
                drawRoots.push_back(filterNode);
            } else {
                MindNode* root = MindMapManager::Instance().GetRoot();
                if (root) drawRoots.push_back(root);
                for (auto fn : MindMapManager::Instance().GetFreeNodes()) {
                    drawRoots.push_back(fn);
                }
            }
            
            if (!drawRoots.empty()) {
                std::vector<NodeDraw> layout;
                std::vector<RECT> rects;
                std::vector<POINT> centers;
                ComputeNodeRects(drawRoots, Canvas_GetZoom(), rc, layout, rects, centers);
                
                bool hitNode = false;
                for (size_t i = 0; i < layout.size(); ++i) {
                    if (PtInRect(&rects[i], pt)) {
                        hitNode = true;
                        MindNode* n = layout[i].node;
                        
                        // Note Popup
                        if (n && !n->note.empty() && MindMapManager::Instance().IsNodeSelected(n)) {
                            ShowNotePopup(n, rects[i]);
                        } else {
                            if (g_notePopupNode != n) CloseNotePopup();
                        }

                        // Summary Popup
                        if (n && !n->summary.empty() && MindMapManager::Instance().IsNodeSelected(n)) {
                            ShowSummaryPopup(n, rects[i]);
                        } else {
                            if (g_summaryPopupNode != n) CloseSummaryPopup();
                        }

                        // Info Popup (Label/Link)
                        if (n != g_hoverNode) {
                            CloseInfoPopups();
                            if (g_hoverTimerId) { KillTimer(hWnd, g_hoverTimerId); g_hoverTimerId = 0; }
                            g_hoverNode = n;
                            
                            bool hasLink = !n->outlinkURL.empty() || !n->outlinkFile.empty() || !n->outlinkPage.empty() || n->outlinkTopicId > 0;
                            if (!n->label.empty() || hasLink) {
                                GetCursorPos(&g_hoverPoint);
                                g_hoverTimerId = SetTimer(hWnd, 2, 2000, NULL);
                            }
                        } else {
                            // Still hovering same node, update point
                            GetCursorPos(&g_hoverPoint);
                        }

                        break;
                    }
                }
                
                if (!hitNode) {
                    CloseNotePopup();
                    CloseSummaryPopup();
                    
                    if (g_hoverNode) {
                        CloseInfoPopups();
                        if (g_hoverTimerId) { KillTimer(hWnd, g_hoverTimerId); g_hoverTimerId = 0; }
                        g_hoverNode = nullptr;
                    }
                }
            }
        }
        break;
    }
    case WM_LBUTTONUP: {
        if (g_isNodeDragging) {
            // Check drag distance
            POINT pt{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            int dx = abs(pt.x - g_dragMouseStartPos.x);
            int dy = abs(pt.y - g_dragMouseStartPos.y);
            
            bool isClick = (dx < 10 && dy < 10);

            // If moved less than 10 pixels (click)
            if (isClick) {
                // If we clicked a selected node without dragging, and Ctrl is not held,
                // we should select ONLY this node (deselect others).
                // We check Ctrl state from current message time (approx) or assume it hasn't changed much.
                if (g_clickedNode && !(GetKeyState(VK_CONTROL) & 0x8000)) {
                     MindMapManager::Instance().ClearSelection();
                     MindMapManager::Instance().AddToSelection(g_clickedNode);
                     if (global::G_TREEVIEW && g_clickedNode->hTreeItem) {
                        TreeView_SelectItem(global::G_TREEVIEW, g_clickedNode->hTreeItem);
                     }
                     Canvas_Invalidate();
                }

                // Revert manual positions if they weren't manual before
                for (auto& item : g_dragNodes) {
                    if (!item.wasManual) {
                        item.node->style.manualPosition = false;
                    }
                }
                Canvas_Invalidate();
            }

            g_isNodeDragging = false;
            g_dragNodes.clear();
            g_clickedNode = nullptr;
            ReleaseCapture();
        }
        if (g_isPanning) {
            g_isPanning = false;
            ReleaseCapture();
        }
        return 0;
    }
    case WM_RBUTTONUP: {
        if (g_isDragging) {
            g_isDragging = false;
            ReleaseCapture();
            
            RECT dragRect;
            dragRect.left = min(g_dragStart.x, g_dragEnd.x);
            dragRect.top = min(g_dragStart.y, g_dragEnd.y);
            dragRect.right = max(g_dragStart.x, g_dragEnd.x);
            dragRect.bottom = max(g_dragStart.y, g_dragEnd.y);

            // Check if it's a click (small movement)
            bool isClick = (abs(g_dragStart.x - g_dragEnd.x) < 5 && abs(g_dragStart.y - g_dragEnd.y) < 5);

            RECT rc; GetClientRect(hWnd, &rc);
            std::vector<MindNode*> drawRoots;
            MindNode* filterNode = nullptr;
            if (g_filterRoot) filterNode = MindMapManager::Instance().GetNodeFromHandle(g_filterRoot);
            if (filterNode) {
                drawRoots.push_back(filterNode);
            } else {
                MindNode* root = MindMapManager::Instance().GetRoot();
                if (root) drawRoots.push_back(root);
                for (auto fn : MindMapManager::Instance().GetFreeNodes()) {
                    drawRoots.push_back(fn);
                }
            }
            
            if (!drawRoots.empty()) {
                std::vector<NodeDraw> layout;
                std::vector<RECT> rects;
                std::vector<POINT> centers;
                ComputeNodeRects(drawRoots, Canvas_GetZoom(), rc, layout, rects, centers);

                if (isClick) {
                    POINT pt = g_dragEnd;
                    for (size_t i = 0; i < layout.size(); ++i) {
                        if (PtInRect(&rects[i], pt)) {
                            MindNode* n = layout[i].node;
                            if (!n) continue;
                            
                            // Select the node
                            MindMapManager::Instance().ClearSelection();
                            MindMapManager::Instance().AddToSelection(n);
                            if (global::G_TREEVIEW && n->hTreeItem) {
                                TreeView_SelectItem(global::G_TREEVIEW, n->hTreeItem);
                            }
                            Canvas_Invalidate();

                            // Show Context Menu
                            HMENU hMenu = CreatePopupMenu();
                            AppendMenuW(hMenu, MF_STRING, 1001, L"修改文本");
                            AppendMenuW(hMenu, MF_STRING, IDM_NOTE, L"修改笔记");
                            AppendMenuW(hMenu, MF_STRING, IDM_LABEL, L"修改标签");
                            AppendMenuW(hMenu, MF_STRING, IDM_SUMMRY, L"修改概要");
                            
                            HMENU hSubLink = CreatePopupMenu();
                            AppendMenuW(hSubLink, MF_STRING, IDM_OUTLINKPAGE, L"网页");
                            AppendMenuW(hSubLink, MF_STRING, IDM_OUTLINKFILE, L"文件");
                            AppendMenuW(hSubLink, MF_STRING, IDM_OUTLINKTOPIC, L"主题");
                            AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hSubLink, L"添加链接");
                            
                            AppendMenuW(hMenu, MF_STRING, IDM_LINK, L"添加联系");

                            POINT screenPt = pt;
                            ClientToScreen(hWnd, &screenPt);
                            int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_TOPALIGN, screenPt.x, screenPt.y, 0, hWnd, NULL);
                            DestroyMenu(hMenu);

                            if (cmd == 1001) {
                                Canvas_EditNode(hWnd, n, rects[i]);
                            } else if (cmd > 0) {
                                SendMessage(global::HOME, WM_COMMAND, cmd, 0);
                            }
                            return 0;
                        }
                    }
                } else {
                    // Drag selection logic
                    for (size_t i = 0; i < layout.size(); ++i) {
                        RECT intersect;
                        if (IntersectRect(&intersect, &dragRect, &rects[i])) {
                            MindMapManager::Instance().AddToSelection(layout[i].node);
                        }
                    }
                }
            }
            Canvas_Invalidate();
        }
        return 0;
    }
    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE && LinkModeIsActive()) {
            LinkModeCancel();
            return 0;
        }
        break;
    case WM_LBUTTONDBLCLK: {
        POINT pt{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        RECT rc; GetClientRect(hWnd, &rc);

        std::vector<MindNode*> drawRoots;
        MindNode* filterNode = nullptr;
        if (g_filterRoot) filterNode = MindMapManager::Instance().GetNodeFromHandle(g_filterRoot);
        if (filterNode) {
            drawRoots.push_back(filterNode);
        } else {
            MindNode* root = MindMapManager::Instance().GetRoot();
            if (root) drawRoots.push_back(root);
            for (auto fn : MindMapManager::Instance().GetFreeNodes()) {
                drawRoots.push_back(fn);
            }
        }
        if (drawRoots.empty()) return 0;

        std::vector<NodeDraw> layout;
        std::vector<RECT> rects;
        std::vector<POINT> centers;
        ComputeNodeRects(drawRoots, Canvas_GetZoom(), rc, layout, rects, centers);

        for (size_t i = 0; i < layout.size(); ++i) {
            if (PtInRect(&rects[i], pt)) {
                MindNode* n = layout[i].node;
                if (!n) break;
                
                Canvas_EditNode(hWnd, n, rects[i]);
                break;
            }
        }
        return 0;
    }
    case WM_MOUSEWHEEL: {
        if (GetKeyState(VK_CONTROL) & 0x8000) {
            int zDelta = GET_WHEEL_DELTA_WPARAM(wParam);
            int change = (zDelta > 0) ? 10 : -10;
            Canvas_ZoomBy(change);
            Canvas_Invalidate();
            UpdateZoomLabel(); // Update label on mouse wheel zoom
            return 0;
        }
        break;
    }
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

void Canvas_ZoomBy(int deltaPercent) {
    g_zoom += deltaPercent;
    if (g_zoom < 10) g_zoom = 10;
    if (g_zoom > 400) g_zoom = 400;
}

void Canvas_SetZoom(int percent) {
    g_zoom = percent;
    if (g_zoom < 10) g_zoom = 10;
    if (g_zoom > 400) g_zoom = 400;
}

int Canvas_GetZoom() {
    return g_zoom;
}

void Canvas_FitToView() {
    if (!g_canvasWnd || !IsWindow(g_canvasWnd)) return;
    RECT rc; GetClientRect(g_canvasWnd, &rc);
    int viewW = rc.right - rc.left;
    int viewH = rc.bottom - rc.top;
    if (viewW <= 0 || viewH <= 0) return;

    std::vector<MindNode*> drawRoots;
    MindNode* filterNode = nullptr;
    if (g_filterRoot) filterNode = MindMapManager::Instance().GetNodeFromHandle(g_filterRoot);
    if (filterNode) {
        drawRoots.push_back(filterNode);
    } else {
        MindNode* root = MindMapManager::Instance().GetRoot();
        if (root) drawRoots.push_back(root);
        for (auto fn : MindMapManager::Instance().GetFreeNodes()) {
            drawRoots.push_back(fn);
        }
    }
    if (drawRoots.empty()) return;

    std::vector<NodeDraw> layout;
    BuildLayoutFromRoots(drawRoots, layout);
    if (layout.empty()) return;

    const int baseX = 80;
    const int baseY = 60;
    const int stepX = 160;
    const int stepY = 80;
    const int nodeW = 100;
    const int nodeH = 38;

    RECT worldBounds = { 1000000, 1000000, -1000000, -1000000 };
    for (const auto& nd : layout) {
        int x = baseX + nd.depth * stepX;
        int y = baseY + nd.order * stepY;
        if (x - nodeW / 2 < worldBounds.left) worldBounds.left = x - nodeW / 2;
        if (y - nodeH / 2 < worldBounds.top) worldBounds.top = y - nodeH / 2;
        if (x + nodeW / 2 > worldBounds.right) worldBounds.right = x + nodeW / 2;
        if (y + nodeH / 2 > worldBounds.bottom) worldBounds.bottom = y + nodeH / 2;
    }

    int contentW = worldBounds.right - worldBounds.left;
    int contentH = worldBounds.bottom - worldBounds.top;
    if (contentW <= 0) contentW = 1;
    if (contentH <= 0) contentH = 1;

    double padding = 0.1;
    int targetW = (int)(viewW * (1.0 - padding * 2));
    int targetH = (int)(viewH * (1.0 - padding * 2));

    double zoomX = (double)targetW / contentW;
    double zoomY = (double)targetH / contentH;
    double zoom = min(zoomX, zoomY);
    
    int finalZoom = (int)(zoom * 100);
    if (finalZoom < 20) finalZoom = 20;
    if (finalZoom > 200) finalZoom = 200;

    g_zoom = finalZoom;
    double scale = (double)g_zoom / 100.0;

    int scaledContentW = (int)(contentW * scale);
    int scaledContentH = (int)(contentH * scale);
    
    g_panOffsetX = (viewW - scaledContentW) / 2 - (int)(worldBounds.left * scale);
    g_panOffsetY = (viewH - scaledContentH) / 2 - (int)(worldBounds.top * scale);
}

void Canvas_Invalidate() {
    Canvas_EnsureWindow();
    if (g_canvasWnd && IsWindow(g_canvasWnd)) {
        InvalidateRect(g_canvasWnd, NULL, TRUE);
        UpdateWindow(g_canvasWnd);
    } else if (global::HOME && IsWindow(global::HOME)) {
        InvalidateRect(global::HOME, NULL, TRUE);
        UpdateWindow(global::HOME);
    }
}

void Canvas_SetFilterRoot(HTREEITEM hRoot) {
    g_filterRoot = hRoot;
    Canvas_Invalidate();
}

void Canvas_ClearFilter() {
    g_filterRoot = NULL;
    Canvas_Invalidate();
}

void Canvas_LoadPaintForNode(MindNode* node) {
    if (!node) return;
    Canvas_Invalidate();
}

void Canvas_ShowPaintForNode(MindNode* node) {
    if (!node) return;
    Canvas_Invalidate();
}

