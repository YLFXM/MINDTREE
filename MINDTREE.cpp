// MINDTREE.cpp : 定义应用程序的入口点。
//

#include "framework.h"
#include "MINDTREE.h"
#include "controls.h"
#include "message.h"
#include "resolve.h"
#include "file_system.h"
#include "edit_system.h"
#include "page_system.h"
#include "MindMapManager.h"
#include "commands.h"
#include "borderless_button.h"
#include "canvas_impl.h" // Added for Canvas_ZoomBy

namespace global {
    // 全局变量:
    HINSTANCE hInst = nullptr;                                // 当前实例
    WCHAR szTitle[MAX_LOADSTRING] = { 0 };                  // 标题栏文本
    WCHAR szWindowClass[MAX_LOADSTRING] = { 0 };            // 主窗口类名
    std::wstring currentFilePath = L"";                     // 当前文件路径
    HWND HOME = NULL;                                       //主窗口句柄
    HWND TOOLS = NULL;                                     //工具栏窗口句柄
    HWND GUIDE = NULL;                                     //导航面板窗口句柄
    HWND STYLE = NULL;                                     //右侧面板窗口句柄
    HWND EXPLAIN = NULL;                                   //快捷键说明窗口句柄
    HWND FILES = NULL;                                     //文件标题栏窗口句柄
    HWND CLOTH = NULL;                                     //画布标题栏窗口句柄

    BORDERLESS_BUTTON_CONFIG  defconfig = { 0 };
    

    // 新增：Tools 窗口类名（若需要不同名称可修改）
    WCHAR ToolsClass[MAX_LOADSTRING] = L"TOOLS_WINDOW_CLASS";
	WCHAR GuideClass[MAX_LOADSTRING] = L"GUIDE_WINDOW_CLASS";
	WCHAR StyleClass[MAX_LOADSTRING] = L"STYLE_WINDOW_CLASS";

    // 按钮全局变量
    HWND SUB = NULL, PARENT = NULL, MATE = NULL, RETURN = NULL, CLOSETOOLS = NULL, RIGHT = NULL, OPENTOOLS = NULL;
    RECT button = { 0 };
    RECT tools = { 0 };
    HWND G_TAB = NULL, G_CLOSE = NULL, G_TREEVIEW = NULL, G_SEARCH = NULL, G_CURRENT = NULL;
    RECT guide = { 0 };
    HWND S_TAB = NULL, S_CLOSE = NULL, S_CURRENT = NULL, W_STYLE = NULL, W_PAINT = NULL;
    RECT style = { 0 };
    
    // Zoom controls
    HWND ZOOM_DEC = NULL;
    HWND ZOOM_INC = NULL;
    HWND ZOOM_LABEL = NULL;
}

// 此代码模块中包含的函数的前向声明:
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);


int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // TODO: 在此处放置代码。

    // 初始化全局字符串
    LoadStringW(hInstance, IDS_APP_TITLE, global::szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_MINDTREE, global::szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);
    
    // Initialize Search/Replace
    InitSearchReplace();

    // 执行应用程序初始化:
    if (!InitInstance (hInstance, nCmdShow))
    {
        return FALSE;
    }

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_MINDTREE));

    MSG msg;

    // 主消息循环:
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    return (int) msg.wParam;
}



//
//  函数: MyRegisterClass()
//
//  目标: 注册窗口类。
//  说明: 除了主窗口类外，增加了 TOOLS 子窗口类注册，WndProc 使用 ToolsWndProc（在 message.cpp 中）
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = WndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = hInstance;
    wcex.hIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_thetree));
    wcex.hCursor        = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW+1);
    wcex.lpszMenuName   = MAKEINTRESOURCEW(IDC_MINDTREE);
    wcex.lpszClassName  = global::szWindowClass;
    wcex.hIconSm        = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_thetree));

    ATOM aMain = RegisterClassExW(&wcex);


    HBRUSH hBrush = CreateSolidBrush(RGB(240, 240, 240)); // 浅灰色
    // 注册 TOOLS 子窗口类（使用 message.cpp 中的 ToolsWndProc）
    WNDCLASSEXW wcexTools = { 0 };
    wcexTools.cbSize = sizeof(WNDCLASSEXW);
    wcexTools.style = CS_HREDRAW | CS_VREDRAW;
    wcexTools.lpfnWndProc = ToolsWndProc;
    wcexTools.cbClsExtra = 0;
    wcexTools.cbWndExtra = 0;
    wcexTools.hInstance = hInstance;
    wcexTools.hIcon = NULL;
    wcexTools.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcexTools.hbrBackground = hBrush;
    wcexTools.lpszClassName = global::ToolsClass;
    RegisterClassExW(&wcexTools);

    WNDCLASSEXW wcexGuide = { 0 };
    wcexGuide.cbSize = sizeof(WNDCLASSEXW);
    wcexGuide.style = CS_HREDRAW | CS_VREDRAW;
    wcexGuide.lpfnWndProc = GuideWndProc;
    wcexGuide.cbClsExtra = 0;
    wcexGuide.cbWndExtra = 0;
    wcexGuide.hInstance = hInstance;
    wcexGuide.hIcon = NULL;
    wcexGuide.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcexGuide.hbrBackground = hBrush;
    wcexGuide.lpszClassName = global::GuideClass;
    RegisterClassExW(&wcexGuide);

    WNDCLASSEXW wcexStyle = { 0 };
    wcexStyle.cbSize = sizeof(WNDCLASSEXW);
    wcexStyle.style = CS_HREDRAW | CS_VREDRAW;
    wcexStyle.lpfnWndProc = StyleWndproc;
    wcexStyle.cbClsExtra = 0;
    wcexStyle.cbWndExtra = 0;
    wcexStyle.hInstance = hInstance;
    wcexStyle.hIcon = NULL;
    wcexStyle.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcexStyle.hbrBackground = hBrush;
    wcexStyle.lpszClassName = global::StyleClass;
    RegisterClassExW(&wcexStyle);


    return aMain;
}

//
//   函数: InitInstance(HINSTANCE, int)
//
//   目标: 保存实例句柄并创建主窗口
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
   global::hInst = hInstance; // 将实例句柄存储在全局变量中

   HWND hWnd = CreateWindowW(global::szWindowClass, global::szTitle, WS_OVERLAPPEDWINDOW | WS_HSCROLL | WS_VSCROLL,
      CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInstance, nullptr);

   if (!hWnd)
   {
      return FALSE;
   }
   
   ShowWindow(hWnd, nCmdShow);
   UpdateWindow(hWnd);

   RECT home;
   GetClientRect(hWnd, &home);

   global::HOME = hWnd;

   // 改为以子窗口方式创建 TOOLS（不再使用 CreateDialog）
   global::TOOLS = CreateWindowEx(
       WS_EX_CONTROLPARENT,
       global::ToolsClass,
       NULL,
       WS_CHILD | WS_VISIBLE,
       0, 0, home.right - home.left, 70,
       hWnd, NULL, global::hInst, NULL);

   // 调整/初始化工具区域、创建控件等
   Ftools(home);
   UpdateWindow(global::TOOLS);

   global::EXPLAIN = CreateDialog(hInstance, MAKEINTRESOURCE(IDD_EXPLAIN), hWnd, Explain);

   global::GUIDE = CreateWindowEx(
       WS_EX_CONTROLPARENT,
       global::GuideClass,
       NULL,
       WS_CHILD,
       0, 70, 220, home.bottom - 70,
       hWnd, NULL, global::hInst, NULL);
   Setguide(home);
   global::STYLE = CreateWindowEx(
       WS_EX_CONTROLPARENT,
       global::StyleClass,
       NULL,
       WS_CHILD,
       home.right - 220, 70, 220, home.bottom - 70,
       hWnd, NULL, global::hInst, NULL);
   Setstyle(home);
   
   // Use Tab Control for FILES
   global::FILES = CreateWindowEx(NULL, WC_TABCONTROL, L"", WS_VISIBLE | WS_CHILD | TCS_TABS, 0, 70, home.right, 25, hWnd, NULL, global::hInst, NULL);
   // Add initial tab
   TCITEM tie;
   tie.mask = TCIF_TEXT;
   tie.pszText = (LPWSTR)L"Page 1      "; // Added padding for close button
   TabCtrl_InsertItem(global::FILES, 0, &tie);
   SetFILES(home);
   ShowWindow(global::FILES, SW_SHOW);
   
   // Subclass FILES tab control
   SetWindowSubclass(global::FILES, FilesTabSubclassProc, 0, 0);

   // Change CLOTH to Tab Control for Paint switching
   global::CLOTH = CreateWindowEx(NULL, WC_TABCONTROL, L"", WS_VISIBLE | WS_CHILD | TCS_TABS | TCS_BOTTOM, 0, home.bottom - 25, home.right, 25, hWnd, NULL, global::hInst, NULL);
   
   // Initialize CLOTH with default paint
   if (global::CLOTH) {
       TCITEM tie;
       tie.mask = TCIF_TEXT;
       tie.pszText = (LPWSTR)L"Default Paint      "; // Added padding
       TabCtrl_InsertItem(global::CLOTH, 0, &tie);
   }
   SetCLOTH(home);
   
   // Subclass CLOTH tab control
   SetWindowSubclass(global::CLOTH, ClothTabSubclassProc, 0, 0);

   return TRUE;
}

//
//  函数: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  目标: 处理主窗口的消息。
//
//  WM_COMMAND  - 处理应用程序菜单
//  WM_PAINT    - 绘制主窗口
//  WM_DESTROY  - 发送退出消息并返回
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    RECT home;
    GetClientRect(hWnd, &home);
    if (global::tools.right != home.right){
        Ftools(home);
        Setstyle(home);
        Setguide(home);
        SetFILES(home);
        SetCLOTH(home);
        SetCanvas(home); // Added SetCanvas call
    }
    
    // Handle Find/Replace Message
    if (message == uFindReplaceMsg && uFindReplaceMsg != 0) {
        ProcessSearchReplace(hWnd, lParam);
        return 0;
    }

    switch (message)
    {
    case WM_NOTIFY:
        {
            LPNMHDR pnmh = (LPNMHDR)lParam;
            if (pnmh->hwndFrom == global::FILES && pnmh->code == TCN_SELCHANGE) {
                int iPage = TabCtrl_GetCurSel(global::FILES);
                MindMapManager::Instance().SwitchToPage(iPage);
                
                // Update CLOTH tabs for the new page
                if (global::CLOTH) {
                    TabCtrl_DeleteAllItems(global::CLOTH);
                    int paintCount = MindMapManager::Instance().GetPaintCount();
                    for (int i = 0; i < paintCount; ++i) {
                        std::wstring title = MindMapManager::Instance().GetPaintTitle(i) + L"      "; // Added padding
                        TCITEM tie;
                        tie.mask = TCIF_TEXT;
                        tie.pszText = (LPWSTR)title.c_str();
                        TabCtrl_InsertItem(global::CLOTH, i, &tie);
                    }
                    // Select the active paint (default 0 for now, or store active index)
                    // MindMapManager currently resets to activePaintIndex on SwitchToPage
                    // We need to get the active index from manager if exposed, or assume 0/stored.
                    // Let's assume SwitchToPage sets it up. We just need to reflect it in UI.
                    // Since we don't have GetActivePaintIndex exposed yet, let's default to 0 or add it.
                    TabCtrl_SetCurSel(global::CLOTH, 0); 
                }
                Canvas_Invalidate();
            }
            else if (pnmh->hwndFrom == global::CLOTH && pnmh->code == TCN_SELCHANGE) {
                int iPaint = TabCtrl_GetCurSel(global::CLOTH);
                MindMapManager::Instance().SwitchToPaint(iPaint);
                Canvas_Invalidate();
            }
        }
        break;
    case WM_COMMAND:
        {
            int wmId = LOWORD(wParam);
            int wmEvent = HIWORD(wParam); // Get notification code

            // Handle Zoom Edit Control
            if (wmId == IDM_ZOOM_EDIT && wmEvent == EN_KILLFOCUS) {
                // When edit control loses focus, update zoom
                WCHAR buf[16];
                GetWindowText(global::ZOOM_LABEL, buf, 16);
                std::wstring text = buf;
                // Remove '%' if present
                size_t percentPos = text.find(L'%');
                if (percentPos != std::wstring::npos) {
                    text = text.substr(0, percentPos);
                }
                try {
                    int newZoom = std::stoi(text);
                    Canvas_SetZoom(newZoom);
                    Canvas_Invalidate();
                    UpdateZoomLabel(); // Reformat text with %
                } catch (...) {
                    UpdateZoomLabel(); // Restore valid value on error
                }
                return 0;
            } else if (wmId == IDM_ZOOM_EDIT && wmEvent == EN_SETFOCUS) {
                 // Optional: Select all text when focused
                 SendMessage(global::ZOOM_LABEL, EM_SETSEL, 0, -1);
                 return 0;
            }

            // 分析菜单选择:
            switch (wmId)
            {
            case IDM_ABOUT://关于
                DialogBox(global::hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
                break;
			case IDM_CLOSE://关闭当前窗口
                DestroyWindow(hWnd);
                break;
			case IDM_EXIT://退出程序
                DestroyWindow(hWnd);
				break;
			case IDM_OPEN://打开
                FileOpen(hWnd);
                Canvas_Invalidate();
                break;
			case IDM_SAVE://保存
                FileSave(hWnd);
                break;
			case IDM_SAVEAS://另存为
                FileSaveAs(hWnd);
                break;
			case IDM_CLOSEFILE://关闭文件
                FileClose(hWnd);
                break;
			case IDM_NEWFILE://新建文件
                FileNew(hWnd);
                Canvas_Invalidate();
                break;
			case IDM_NEWPAGE://新建页面
                PageNew(hWnd);
                Canvas_Invalidate();
                break;
			case IDM_CANCEL://撤销

                EditUndo(hWnd);
                break;
			case IDM_REWIND://重做
                EditRedo(hWnd);
                break;
			case IDM_CUT://剪切
                EditCut(hWnd);
                break;
			case IDM_COPY://复制
                EditCopy(hWnd);
                break;
			case IDM_PASTE://粘贴
                EditPaste(hWnd);
                break;
			case IDM_DELETESUB://删除分支
                EditDeleteSub(hWnd);
                break;
			case IDM_DELETE://删除该主题（分支接到父主题上）>
                EditDelete(hWnd);
                break;
			case IDM_RETURN://回到中心主题
                NavigateReturnToCenter(hWnd);
                break;
            case IDM_ALL://全选
                EditSelectAll(hWnd);
                break;
			case IDM_SEARCH://搜索和替换
                EditSearch(hWnd);
                break;
			case IDM_NEWPAGEANDNEWFILE://在新页面中新建文件
                PageNewFile(hWnd);
                Canvas_Invalidate();
                break;
			case IDM_NEWPAGEANDFILE://在新页面中打开文件
                PageOpenFile(hWnd);
                Canvas_Invalidate();
                break;
			case IDM_SUB://下级主题
                HandleSubtopic(hWnd);
                break;
			case IDM_WORKMATE://同级主题
                HandleWorkmate(hWnd);
                break;
			case IDM_PARENT://上级主题
                HandleParent(hWnd);
                break;
			case IDM_FREETOPIC://自由主题
                HandleFreeTopic(hWnd);
                break;
			case IDM_LABEL://标签
                HandleLabel(hWnd);
                break;
			case IDM_ANNOTATION://注释
                HandleAnnotation(hWnd);
                break;
			case IDM_SUMMRY://概要
                HandleSummary(hWnd);
                break;
			case IDM_LINK://联系
                HandleLink(hWnd);
                break;
			case IDM_FRAME://外框
                HandleFrame(hWnd);
                break;
			case IDM_NOTE://注释
                HandleNote(hWnd);
                break;
			case IDM_OUTLINKPAGE://链接页面
                HandleOutlinkPage(hWnd);
                break;
			case IDM_OUTLINKFILE://链接文件
                HandleOutlinkFile(hWnd);
                break;
			case IDM_OUTLINKTOPIC://链接主题
                HandleOutlinkTopic(hWnd);
                break;
			case IDM_NEWPAINT://新建绘图
                HandleNewPaint(hWnd);
                Canvas_Invalidate();
                break;
			case IDM_NEWPAINTWITHTOPIC://新建与主题关联的绘图
                HandleNewPaintWithTopic(hWnd);
                Canvas_Invalidate();
                break;
			case IDM_LARGER://放大
                HandleLarger(hWnd);
                break;
			case IDM_SMALLER://缩小
                HandleSmaller(hWnd);
                break;
			case IDM_ADAPT://适应画布
                HandleAdapt(hWnd);
                break;
			case IDM_LINEONLY://仅显示该条分支
                HandleLineOnly(hWnd);
                break;
			case IDM_GUIDETOPIC://主题向导
                if (!IsWindowVisible(global::GUIDE)) {
                    Setguide(home);
                    ShowWindow(global::GUIDE, SW_NORMAL);
                    SetFILES(home);
                    SetCLOTH(home);
                    SetCanvas(home); // Added SetCanvas call
                }
                Fguidetopic();
                break;
			case IDM_GUIDENOTE://笔记向导
                if (!IsWindowVisible(global::GUIDE)) {
                    Setguide(home);
                    ShowWindow(global::GUIDE, SW_NORMAL);
                    SetFILES(home);
                    SetCLOTH(home);
                    SetCanvas(home); // Added SetCanvas call
                }
                Fguidenote();
                break;
			case IDM_GUIDELABEL://标签向导
                if (!IsWindowVisible(global::GUIDE)) {
                    Setguide(home);
                    ShowWindow(global::GUIDE, SW_NORMAL);
                    SetFILES(home);
                    SetCLOTH(home);
                    SetCanvas(home); // Added SetCanvas call
                }
                Fguidelabel();
                break;
			case IDM_QUESTION://快捷键说明
				ShowWindow(global::EXPLAIN, SW_NORMAL);
				UpdateWindow(global::EXPLAIN);
                break;
			case IDM_EDITSTYLE://样式面板
                if (!IsWindowVisible(global::STYLE)) {
                    Setstyle(home);
                    ShowWindow(global::STYLE, SW_NORMAL);
                    UpdateWindow(global::STYLE);
                    SetFILES(home);
                    SetCLOTH(home);
                    SetCanvas(home); // Added SetCanvas call
                }
                break;
			case IDM_EDITPAINT://绘图面板
                if (!IsWindowVisible(global::STYLE)) {
                    Setstyle(home);
                    ShowWindow(global::STYLE, SW_NORMAL);
                    UpdateWindow(global::STYLE);
                    SetFILES(home);
                    SetCLOTH(home);
                    SetCanvas(home); // Added SetCanvas call
                }
                break;
			case IDM_TOOLS://工具栏
                Ftools(home);
                if(!IsWindowVisible(global::TOOLS)){
                    ShowWindow(global::TOOLS, SW_NORMAL);
                    SetFILES(home);
                    SetCanvas(home); // Added SetCanvas call (though TOOLS visibility might not change layout size directly if it overlays, but SetFILES moves, so Canvas should move)
                }
				UpdateWindow(global::TOOLS);
                break;
            case IDM_ZOOM_DEC:
                Canvas_ZoomBy(-10);
                Canvas_Invalidate();
                UpdateZoomLabel(); // Added call
                break;
            case IDM_ZOOM_INC:
                Canvas_ZoomBy(10);
                Canvas_Invalidate();
                UpdateZoomLabel(); // Added call
                break;
            default:
                return DefWindowProc(hWnd, message, wParam, lParam);
            }
        }
        break;
    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            // TODO: 在此处添加使用 hdc 的任何绘图代码...
            EndPaint(hWnd, &ps);
        }
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

