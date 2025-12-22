// Minimal canvas stub implementation so commands can call drawing-related APIs during testing.
#include "MindMapData.h"
#include "canvas_stub.h"
#include <windows.h>
#include <shellapi.h>
#include <windowsx.h> // <- 为 GET_X_LPARAM / GET_Y_LPARAM 提供定义（也包含常用的鼠标/键盘辅助宏）
#include <string>
#include <vector>
#include <sstream>
#include "MindMapManager.h"
#include "MINDTREE.h"
#include "commands.h"

#ifndef GET_X_LPARAM
// 备用定义（如果 windowsx.h 未提供）
#define GET_X_LPARAM(lp) ((int)(short)LOWORD(lp))
#endif
#ifndef GET_Y_LPARAM
#define GET_Y_LPARAM(lp) ((int)(short)HIWORD(lp))
#endif

static int g_zoom = 100;
static HTREEITEM g_filterRoot = NULL;
static HWND g_canvasWnd = NULL;
static const wchar_t* CANVAS_CLASS_NAME = L"MINDTREE_CANVAS_STUB";
static HCURSOR g_linkCursor = NULL;

static bool g_isDragging = false;
static POINT g_dragStart = { 0, 0 };
static POINT g_dragEnd = { 0, 0 };

static int g_panOffsetX = 0;
static int g_panOffsetY = 0;
static bool g_isPanning = false;
static POINT g_lastMousePos = { 0, 0 };

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
        
        // Check children recursively
        HTREEITEM hFound = FindTreeItemByID(hTreeView, nodeId, hItem);
        if (hFound) return hFound;
        
        // Check next sibling
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

// 根据当前缩放和布局参数生成节点矩形，供绘制和命中测试共用
static void ComputeNodeRects(const std::vector<MindNode*>& drawRoots, int zoom, const RECT& rcClient, std::vector<NodeDraw>& layout, std::vector<RECT>& rects, std::vector<POINT>& centers) {
    layout.clear();
    rects.clear();
    centers.clear();
    BuildLayoutFromRoots(drawRoots, layout);

    double scale = (double)zoom / 100.0;
    const int baseX = 80;
    const int baseY = 60;
    const int stepX = 160;
    const int stepY = 80;
    const int minW = 60, maxW = 200;
    const int minH = 28, maxH = 90;

    for (const auto& nd : layout) {
        int x = (int)((baseX + nd.depth * stepX) * scale) + g_panOffsetX;
        int y = (int)((baseY + nd.order * stepY) * scale) + g_panOffsetY;
        centers.push_back({ x, y });
        int rw = (int)((100 + (zoom - 100) / 4) * scale); if (rw < minW) rw = minW; if (rw > maxW) rw = maxW;
        int rh = (int)((38 + (zoom - 100) / 10) * scale); if (rh < minH) rh = minH; if (rh > maxH) rh = maxH;
        RECT box{ x - rw / 2, y - rh / 2, x + rw / 2, y + rh / 2 };
        rects.push_back(box);
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

    // Register class if needed
    WNDCLASSW wc = {0};
    if (!GetClassInfoW(global::hInst, CANVAS_CLASS_NAME, &wc)) {
        wc.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS; // enable double-clicks
        wc.lpfnWndProc = CanvasWndProc;
        wc.hInstance = global::hInst;
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        wc.lpszClassName = CANVAS_CLASS_NAME;
        RegisterClassW(&wc);
    }

    RECT rc; GetClientRect(global::HOME, &rc);
    g_canvasWnd = CreateWindowExW(0, CANVAS_CLASS_NAME, L"", WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
        0, 0, rc.right, rc.bottom, global::HOME, NULL, global::hInst, NULL);
    TCITEM tie = {};
    tie.mask = TCIF_TEXT;
    tie.pszText = (LPWSTR)L"paint";
    TabCtrl_InsertItem(global::CLOTH, 0, &tie);
    //global::g_canvasWnd = g_canvasWnd;
}

static LRESULT CALLBACK CanvasWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE:
        g_linkCursor = LoadCursor(NULL, IDC_CROSS);
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
        // background
        HBRUSH hbr = CreateSolidBrush(RGB(250,250,250));
        FillRect(hdc, &rc, hbr);
        DeleteObject(hbr);

        // header
        int zoom = Canvas_GetZoom();
        std::wstringstream ss;
        ss << L"Canvas Stub - Zoom:" << zoom;
        std::wstring header = ss.str();
        SetBkMode(hdc, TRANSPARENT);
        RECT hdr = rc; hdr.bottom = hdr.top + 24;
        DrawTextW(hdc, header.c_str(), (int)header.size(), &hdr, DT_LEFT | DT_SINGLELINE | DT_TOP);

        // nodes and edges with a simple layered layout
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

        // 当前 TreeView 选中节点（用于高亮）
        MindNode* selectedNode = nullptr;
        if (global::G_TREEVIEW) {
            HTREEITEM hSel = TreeView_GetSelection(global::G_TREEVIEW);
            if (hSel) selectedNode = MindMapManager::Instance().GetNodeFromHandle(hSel);
        }

        std::vector<NodeDraw> layout;
        std::vector<RECT> rects;
        std::vector<POINT> positions;
        ComputeNodeRects(drawRoots, zoom, rc, layout, rects, positions);

        // draw edges first (parent-child hierarchy)
        HPEN hEdgePen = CreatePen(PS_SOLID, 1, RGB(120, 120, 120));
        HGDIOBJ oldPen = SelectObject(hdc, hEdgePen);
        for (size_t i = 0; i < layout.size(); ++i) {
            MindNode* n = layout[i].node;
            if (!n || !n->parent) continue;
            // find parent index in layout
            for (size_t j = 0; j < layout.size(); ++j) {
                if (layout[j].node == n->parent) {
                    MoveToEx(hdc, positions[j].x, positions[j].y, NULL);
                    LineTo(hdc, positions[i].x, positions[i].y);
                    break;
                }
            }
        }
        SelectObject(hdc, oldPen);
        DeleteObject(hEdgePen);

        // draw relationship links (node-to-node connections)
        HPEN hLinkPen = CreatePen(PS_DASH, 2, RGB(0, 120, 215)); // Blue dashed line for relationships
        oldPen = SelectObject(hdc, hLinkPen);
        for (size_t i = 0; i < layout.size(); ++i) {
            MindNode* n = layout[i].node;
            if (!n || n->linkedNodeIds.empty()) continue;
            
            for (int linkedId : n->linkedNodeIds) {
                // Find the linked node in layout
                for (size_t j = 0; j < layout.size(); ++j) {
                    if (layout[j].node && layout[j].node->id == linkedId) {
                        // Draw dashed line between linked nodes
                        MoveToEx(hdc, positions[i].x, positions[i].y, NULL);
                        LineTo(hdc, positions[j].x, positions[j].y);
                        
                        // Draw small arrow at midpoint to indicate direction
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

        // draw frames
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

        // draw nodes
        int idx = 0;
        for (size_t i = 0; i < layout.size(); ++i) {
            MindNode* n = layout[i].node;
            if (!n) continue;
            RECT box = rects[i];
            bool isSelected = (n == selectedNode) || MindMapManager::Instance().IsNodeSelected(n);
            COLORREF fillColor = (n == filterNode) ? RGB(255, 245, 200) : RGB(235, 245, 255);
            if (isSelected) fillColor = RGB(200, 230, 255);
            HBRUSH fill = CreateSolidBrush(fillColor);
            FillRect(hdc, &box, fill);
            DeleteObject(fill);
            HBRUSH frameBrush = (HBRUSH)GetStockObject(BLACK_BRUSH);
            if (isSelected) {
                HPEN selPen = CreatePen(PS_SOLID, 2, RGB(0, 120, 215));
                HGDIOBJ oldPen = SelectObject(hdc, selPen);
                SelectObject(hdc, GetStockObject(NULL_BRUSH));
                Rectangle(hdc, box.left, box.top, box.right, box.bottom);
                SelectObject(hdc, oldPen);
                DeleteObject(selPen);
            } else {
                FrameRect(hdc, &box, frameBrush);
            }
            std::wstring text = n->text.empty() ? (L"ID " + std::to_wstring(n->id)) : n->text;
            DrawTextW(hdc, text.c_str(), (int)text.size(), &box, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
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

                // Ctrl+Click to handle links
                if (GetKeyState(VK_CONTROL) & 0x8000) {
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
                } else if (global::G_TREEVIEW) {
                    // Clear multi-selection when clicking a single node
                    MindMapManager::Instance().ClearSelection();
                    MindMapManager::Instance().AddToSelection(n);

                    // Find the current TreeView item for this node by ID
                    HTREEITEM hItem = nullptr;
                    if (n->hTreeItem) {
                        // Check if the stored handle is still valid in current view
                        TVITEM tvi{};
                        tvi.mask = TVIF_PARAM;
                        tvi.hItem = n->hTreeItem;
                        if (TreeView_GetItem(global::G_TREEVIEW, &tvi) && (int)tvi.lParam == n->id) {
                            hItem = n->hTreeItem;
                        }
                    }

                    // If stored handle is invalid, search by ID in current tree
                    if (!hItem) {
                        hItem = FindTreeItemByID(global::G_TREEVIEW, n->id);
                    }

                    if (hItem) {
                        TreeView_SelectItem(global::G_TREEVIEW, hItem);
                        SetFocus(global::G_TREEVIEW);
                        Canvas_Invalidate();
                    }
                }
                break;
            }
        }

        if (!hit) {
            // Start panning
            g_isPanning = true;
            g_lastMousePos = pt;
            SetCapture(hWnd);
        }
        return 0;
    }
    case WM_RBUTTONDOWN: {
        SetFocus(hWnd);
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
        if (g_isPanning) {
            g_panOffsetX += (pt.x - g_lastMousePos.x);
            g_panOffsetY += (pt.y - g_lastMousePos.y);
            g_lastMousePos = pt;
            Canvas_Invalidate();
        } else if (g_isDragging) {
            g_dragEnd = pt;
            Canvas_Invalidate();
        }
        break;
    }
    case WM_LBUTTONUP: {
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

            // Find nodes in dragRect
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

                for (size_t i = 0; i < layout.size(); ++i) {
                    RECT intersect;
                    if (IntersectRect(&intersect, &dragRect, &rects[i])) {
                        MindMapManager::Instance().AddToSelection(layout[i].node);
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
                if (global::G_TREEVIEW && n->hTreeItem) {
                    TreeView_SelectItem(global::G_TREEVIEW, n->hTreeItem);
                    SetFocus(global::G_TREEVIEW);
                }
                Canvas_ShowNodeInfo(hWnd, n);
                break;
            }
        }
        return 0;
    }
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

// API implementations
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

    // Calculate bounding box at zoom 100%
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

    // Add some padding (e.g., 10%)
    double padding = 0.1;
    int targetW = (int)(viewW * (1.0 - padding * 2));
    int targetH = (int)(viewH * (1.0 - padding * 2));

    double zoomX = (double)targetW / contentW;
    double zoomY = (double)targetH / contentH;
    double zoom = min(zoomX, zoomY);
    
    int finalZoom = (int)(zoom * 100);
    if (finalZoom < 20) finalZoom = 20;
    if (finalZoom > 200) finalZoom = 200; // Don't zoom in too much automatically

    g_zoom = finalZoom;
    double scale = (double)g_zoom / 100.0;

    // Center the content
    int scaledContentW = (int)(contentW * scale);
    int scaledContentH = (int)(contentH * scale);
    
    g_panOffsetX = (viewW - scaledContentW) / 2 - (int)(worldBounds.left * scale);
    g_panOffsetY = (viewH - scaledContentH) / 2 - (int)(worldBounds.top * scale);
}

void Canvas_Invalidate() {
    std::wstring dbg = L"Canvas invalidated. Zoom=" + std::to_wstring(Canvas_GetZoom()) + L"\n";
    OutputDebugStringW(dbg.c_str());
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
    MindNode* node = MindMapManager::Instance().GetNodeFromHandle(hRoot);
    if (node) {
        std::wstring dbg = L"Canvas filter set to node id=" + std::to_wstring(node->id) + L"\n";
        OutputDebugStringW(dbg.c_str());
    } else {
        OutputDebugStringW(L"Canvas filter set (node not found)\n");
    }
    Canvas_Invalidate();
}

void Canvas_ClearFilter() {
    g_filterRoot = NULL;
    OutputDebugStringW(L"Canvas filter cleared\n");
    Canvas_Invalidate();
}

void Canvas_LoadPaintForNode(MindNode* node) {
    if (!node) return;
    std::wstring dbg = L"Canvas load paint for node id=" + std::to_wstring(node->id) + L"\n";
    OutputDebugStringW(dbg.c_str());
    Canvas_Invalidate();
}

void Canvas_ShowPaintForNode(MindNode* node) {
    if (!node) return;
    if (node->paintPath.empty()) {
        OutputDebugStringW(L"No paint path for node\n");
        return;
    }
    std::wstring dbg = L"Showing paint: " + node->paintPath + L"\n";
    OutputDebugStringW(dbg.c_str());
    Canvas_Invalidate();
}

