// message.cpp
// 对话/窗口消息处理函数实现文件

#include "MindMapData.h"
#include "framework.h"
#include "MINDTREE.h"
#include "controls.h"
#include "message.h"
#include "resolve.h"
#include "MindMapManager.h"
#include "canvas_stub.h"
#include "history_system.h"
#include "borderless_button.h"

// 各控件选项文本
LPCWSTR shapestr[] = { L"圆角矩形",L"矩形",L"椭圆",L"圆形",L"菱形",L"自定义" ,NULL };
LPCWSTR framestr[] = { L"实线",L"双实线",L"虚线",L"点划线",L"点双划线" ,NULL };
LPCWSTR colorstr[] = { L"黑色",L"白色",L"浅灰",L"红色",L"黄色",L"自定义" ,NULL };
LPCWSTR alignstr[] = { L"左对齐",L"居中对齐",L"右对齐" ,NULL };
LPCWSTR structureStr[] = { L"思维导图", L"树状图", L"括号图", L"鱼骨图", NULL };
LPCWSTR presetStr[] = { L"未确定", NULL };
LPCWSTR font[] = { L"跟随系统",L"其他字体……",NULL };

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
        CreateDropList(global::W_STYLE, global::hInst, IDM_FONT, 80, yPos, 100, 200, font, lengthof(font));
        yPos += 30;

        CreateText(global::W_STYLE, global::hInst, -1, 10, yPos, 80, 20, L"字体大小:");
        CreateEditBox(global::W_STYLE, global::hInst, IDM_TEXTSIZE, 100, yPos, 50, 20, L"12");
        yPos += 30;

        CreateText(global::W_STYLE, global::hInst, -1, 10, yPos, 80, 20, L"文字颜色:");
        CreateDropList(global::W_STYLE, global::hInst, IDM_TEXTCOLOR, 100, yPos, 80, 200, colorstr, lengthof(colorstr));
        yPos += 30;

        // 文字样式按钮
        CreateButton(global::W_STYLE, global::hInst, IDM_BOLD, 20, yPos, 40, 25, L"粗体");
        CreateButton(global::W_STYLE, global::hInst, IDM_ITALIC, 60, yPos, 40, 25, L"斜体");
        CreateButton(global::W_STYLE, global::hInst, IDM_UNDERLINE, 100, yPos, 60, 25, L"下划线");
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
            // 处理形状选择
            return 0;

        case IDM_FILL:
            // 处理填充颜色选择
            return 0;

        case IDM_FRAME:
            // 处理边框样式选择
            return 0;

        case IDM_FRAMETHICK:
            // 处理边框粗细设置
            return 0;

        case IDM_FRAMECOLOR:
            // 处理边框颜色选择
            return 0;

        case IDM_FONT:
            // 处理字体选择
            return 0;

        case IDM_TEXTSIZE:
            // 处理字体大小设置
            return 0;

        case IDM_TEXTCOLOR:
            // 处理文字颜色选择
            return 0;

        case IDM_BOLD:
            // 处理粗体切换
            return 0;

        case IDM_ITALIC:
            // 处理斜体切换
            return 0;

        case IDM_UNDERLINE:
            // 处理下划线切换
            return 0;

        case IDM_TEXTALIGN:
            // 处理文字对齐方式选择
            return 0;

        case IDM_BRANCH_THICK:
            // 处理分支线粗细设置
            return 0;

        case IDM_BRANCH_COLOR:
            // 处理分支线颜色选择
            return 0;

        case IDM_STRUCTURE:
            // 处理结构选择
            return 0;

            // ========== 绘图面板控件命令处理 ==========
        case IDM_PRESET:
            // 处理预设样式选择
            return 0;

        case IDM_AUTO_BALANCE:
            // 处理自动平衡布局切换
            return 0;

        case IDM_COMPACT_LAYOUT:
            // 处理紧凑布局切换
            return 0;

        case IDM_SIBLING_ALIGN:
            // 处理同级节点对齐切换
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

void S_CLOSEcliked() {

}