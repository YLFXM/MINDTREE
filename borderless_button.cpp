#include "borderless_button.h"
#include <uxtheme.h>
#include <vssym32.h>

#pragma comment(lib, "uxtheme.lib")

// 默认颜色
#define DEFAULT_BG_NORMAL      RGB(240, 240, 240)
#define DEFAULT_BG_HOVER       RGB(255, 255, 255)
#define DEFAULT_BG_PRESSED     RGB(230, 230, 230)
#define DEFAULT_BG_DISABLED    RGB(250, 250, 250)

#define DEFAULT_BORDER_NORMAL  RGB(200, 200, 200)
#define DEFAULT_BORDER_HOVER   RGB(150, 150, 150)
#define DEFAULT_BORDER_PRESSED RGB(100, 100, 100)

#define DEFAULT_TEXT_NORMAL    RGB(50, 50, 50)
#define DEFAULT_TEXT_DISABLED  RGB(150, 150, 150)

// 窗口类名
static const WCHAR g_szButtonClass[] = L"BorderlessIconButton";

// 获取按钮实例数据
static BORDERLESS_BUTTON* GetButtonData(HWND hwnd)
{
    return (BORDERLESS_BUTTON*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
}

// 设置按钮状态并重绘
static void SetButtonState(HWND hwnd, BUTTON_STATE newState)
{
    BORDERLESS_BUTTON* pButton = GetButtonData(hwnd);
    if (!pButton) return;

    if (pButton->state != newState)
    {
        pButton->state = newState;
        InvalidateRect(hwnd, NULL, FALSE);
        UpdateWindow(hwnd);
    }
}

// 计算图标和文本位置
static void CalculateLayout(BORDERLESS_BUTTON* pButton, RECT* rc,
    RECT* rcIcon, RECT* rcText)
{
    // 清除矩形
    ZeroMemory(rcIcon, sizeof(RECT));
    ZeroMemory(rcText, sizeof(RECT));

    if (!pButton->config.showText && pButton->config.hIcon)
    {
        // 只有图标，居中显示
        int iconSize = pButton->config.iconSize ? pButton->config.iconSize : 32;
        rcIcon->left = rc->left + (rc->right - rc->left - iconSize) / 2;
        rcIcon->top = rc->top + (rc->bottom - rc->top - iconSize) / 2;
        rcIcon->right = rcIcon->left + iconSize;
        rcIcon->bottom = rcIcon->top + iconSize;
    }
    else if (pButton->config.showText && !pButton->config.hIcon)
    {
        // 只有文本，居中显示
        *rcText = *rc;
    }
    else if (pButton->config.showText && pButton->config.hIcon)
    {
        // 图标和文本都有
        int iconSize = pButton->config.iconSize ? pButton->config.iconSize : 24;
        int spacing = pButton->config.iconTextSpacing ? pButton->config.iconTextSpacing : 4;

        switch (pButton->config.iconAlignment)
        {
        case BB_ALIGN_LEFT:
            rcIcon->left = rc->left + spacing;
            rcIcon->top = rc->top + (rc->bottom - rc->top - iconSize) / 2;
            rcIcon->right = rcIcon->left + iconSize;
            rcIcon->bottom = rcIcon->top + iconSize;

            rcText->left = rcIcon->right + spacing;
            rcText->top = rc->top;
            rcText->right = rc->right - spacing;
            rcText->bottom = rc->bottom;
            break;

        case BB_ALIGN_RIGHT:
            rcIcon->right = rc->right - spacing;
            rcIcon->top = rc->top + (rc->bottom - rc->top - iconSize) / 2;
            rcIcon->left = rcIcon->right - iconSize;
            rcIcon->bottom = rcIcon->top + iconSize;

            rcText->left = rc->left + spacing;
            rcText->top = rc->top;
            rcText->right = rcIcon->left - spacing;
            rcText->bottom = rc->bottom;
            break;

        case BB_ALIGN_TOP:
            rcIcon->left = rc->left + (rc->right - rc->left - iconSize) / 2;
            rcIcon->top = rc->top + spacing;
            rcIcon->right = rcIcon->left + iconSize;
            rcIcon->bottom = rcIcon->top + iconSize;

            rcText->left = rc->left;
            rcText->top = rcIcon->bottom + spacing;
            rcText->right = rc->right;
            rcText->bottom = rc->bottom - spacing;
            break;

        case BB_ALIGN_BOTTOM:
            rcIcon->left = rc->left + (rc->right - rc->left - iconSize) / 2;
            rcIcon->bottom = rc->bottom - spacing;
            rcIcon->right = rcIcon->left + iconSize;
            rcIcon->top = rcIcon->bottom - iconSize;

            rcText->left = rc->left;
            rcText->top = rc->top + spacing;
            rcText->right = rc->right;
            rcText->bottom = rcIcon->top - spacing;
            break;

        case BB_ALIGN_CENTER:
        default:
            // 图标和文本垂直居中，图标在上，文本在下
            int totalHeight = iconSize + spacing + 20; // 20是预估的文本高度
            int startY = rc->top + (rc->bottom - rc->top - totalHeight) / 2;

            rcIcon->left = rc->left + (rc->right - rc->left - iconSize) / 2;
            rcIcon->top = startY;
            rcIcon->right = rcIcon->left + iconSize;
            rcIcon->bottom = rcIcon->top + iconSize;

            rcText->left = rc->left;
            rcText->top = rcIcon->bottom + spacing;
            rcText->right = rc->right;
            rcText->bottom = rcText->top + 20;
            break;
        }
    }
}

// 绘制圆角矩形
static void DrawRoundRect(HDC hdc, RECT* rc, int radius, COLORREF fillColor, COLORREF borderColor, int borderWidth)
{
    HBRUSH hBrush = CreateSolidBrush(fillColor);
    HPEN hPen = borderWidth > 0 ? CreatePen(PS_SOLID, borderWidth, borderColor) : NULL;

    HBRUSH hOldBrush = (HBRUSH)SelectObject(hdc, hBrush);
    HPEN hOldPen = (HPEN)SelectObject(hdc, hPen ? hPen : GetStockObject(NULL_PEN));

    // 使用RoundRect函数绘制圆角矩形
    RoundRect(hdc, rc->left, rc->top, rc->right, rc->bottom, radius, radius);

    // 清理
    SelectObject(hdc, hOldBrush);
    SelectObject(hdc, hOldPen);
    DeleteObject(hBrush);
    if (hPen) DeleteObject(hPen);
}

// 绘制按钮
static void DrawButton(HWND hwnd, HDC hdc)
{
    BORDERLESS_BUTTON* pButton = GetButtonData(hwnd);
    if (!pButton) return;

    RECT rc;
    GetClientRect(hwnd, &rc);

    // 调用自定义绘制回调
    if (pButton->config.customDrawCallback)
    {
        pButton->config.customDrawCallback(hwnd, hdc, &rc, pButton->state, pButton->config.userData);
        return;
    }

    // 确定颜色
    COLORREF bgColor, borderColor, textColor;

    switch (pButton->state)
    {
    case BBS_HOVER:
        bgColor = pButton->config.bgColorHover;
        borderColor = pButton->config.borderColorHover;
        textColor = DEFAULT_TEXT_NORMAL;
        break;

    case BBS_PRESSED:
        bgColor = pButton->config.bgColorPressed;
        borderColor = pButton->config.borderColorPressed;
        textColor = DEFAULT_TEXT_NORMAL;
        break;

    case BBS_DISABLED:
        bgColor = pButton->config.bgColorDisabled;
        borderColor = pButton->config.borderColorNormal;
        textColor = DEFAULT_TEXT_DISABLED;
        break;

    case BBS_FOCUSED:
        bgColor = pButton->config.bgColorHover;
        borderColor = RGB(0, 120, 215); // ??
        textColor = DEFAULT_TEXT_NORMAL;
        break;

    case BBS_NORMAL:
    default:
        bgColor = pButton->config.bgColorNormal;
        borderColor = pButton->config.borderColorNormal;
        textColor = DEFAULT_TEXT_NORMAL;
        break;
    }

    // 保存DC状态
    int savedDC = SaveDC(hdc);

    // 绘制背景
    if (pButton->config.style & BB_STYLE_FLAT)
    {
        // 扁平风格
        HBRUSH hBrush = CreateSolidBrush(bgColor);
        FillRect(hdc, &rc, hBrush);
        DeleteObject(hBrush);
    }
    else if (pButton->config.style & BB_STYLE_ROUNDED)
    {
        // 圆角风格
        int radius = pButton->config.cornerRadius ? pButton->config.cornerRadius : 5;
        DrawRoundRect(hdc, &rc, radius, bgColor, borderColor, pButton->config.borderWidth);
    }
    else
    {
        // 普通风格
        HBRUSH hBrush = CreateSolidBrush(bgColor);
        FillRect(hdc, &rc, hBrush);
        DeleteObject(hBrush);

        // 绘制边框
        if (pButton->config.borderWidth > 0)
        {
            HPEN hPen = CreatePen(PS_SOLID, pButton->config.borderWidth, borderColor);
            HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);
            HBRUSH hOldBrush = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));

            Rectangle(hdc, rc.left, rc.top, rc.right, rc.bottom);

            SelectObject(hdc, hOldPen);
            SelectObject(hdc, hOldBrush);
            DeleteObject(hPen);
        }
    }

    // 绘制图标和文本
    RECT rcIcon, rcText;
    CalculateLayout(pButton, &rc, &rcIcon, &rcText);

    // 绘制图标
    HICON hIconToDraw = NULL;
    switch (pButton->state)
    {
    case BBS_HOVER:
        hIconToDraw = pButton->config.hIconHover ? pButton->config.hIconHover : pButton->config.hIcon;
        break;
    case BBS_PRESSED:
        hIconToDraw = pButton->config.hIconPressed ? pButton->config.hIconPressed : pButton->config.hIcon;
        break;
    case BBS_DISABLED:
        hIconToDraw = pButton->config.hIconDisabled ? pButton->config.hIconDisabled : pButton->config.hIcon;
        break;
    default:
        hIconToDraw = pButton->config.hIcon;
        break;
    }

    if (hIconToDraw && !IsRectEmpty(&rcIcon))
    {
        DrawIconEx(hdc, rcIcon.left, rcIcon.top, hIconToDraw,
            rcIcon.right - rcIcon.left, rcIcon.bottom - rcIcon.top,
            0, NULL, DI_NORMAL);
    }

    // 绘制文本
    if (pButton->config.showText && wcslen(pButton->config.text) > 0 && !IsRectEmpty(&rcText))
    {
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, textColor);

        HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
        HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);

        // 根据对齐方式绘制文本
        UINT format = DT_SINGLELINE | DT_VCENTER;
        if (pButton->config.hIcon)
        {
            if (pButton->config.iconAlignment == BB_ALIGN_LEFT ||
                pButton->config.iconAlignment == BB_ALIGN_RIGHT)
            {
                format |= DT_LEFT;
            }
            else
            {
                format |= DT_CENTER;
            }
        }
        else
        {
            format |= DT_CENTER;
        }

        DrawText(hdc, pButton->config.text, -1, &rcText, format);
        SelectObject(hdc, hOldFont);
    }

    // 绘制焦点矩形
    if (pButton->hasFocus && pButton->state != BBS_DISABLED)
    {
        RECT focusRect = rc;
        InflateRect(&focusRect, -2, -2);
        DrawFocusRect(hdc, &focusRect);
    }

    // 恢复DC状态
    RestoreDC(hdc, savedDC);
}

// 窗口过程
static LRESULT CALLBACK BorderlessButtonWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    BORDERLESS_BUTTON* pButton = GetButtonData(hwnd);

    switch (msg)
    {
    case WM_CREATE:
    {
        CREATESTRUCT* pCreate = (CREATESTRUCT*)lParam;
        BORDERLESS_BUTTON_CONFIG* pConfig = (BORDERLESS_BUTTON_CONFIG*)pCreate->lpCreateParams;

        // 分配并初始化按钮数据
        pButton = (BORDERLESS_BUTTON*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(BORDERLESS_BUTTON));
        if (!pButton) return -1;

        pButton->hwnd = hwnd;
        pButton->hParent = pCreate->hwndParent;
        pButton->state = BBS_NORMAL;
        pButton->tracking = FALSE;
        pButton->hasFocus = FALSE;

        // 复制配置
        if (pConfig)
        {
            pButton->config = *pConfig;
        }
        else
        {
            // 使用默认配置
            ZeroMemory(&pButton->config, sizeof(BORDERLESS_BUTTON_CONFIG));
            pButton->config.bgColorNormal = DEFAULT_BG_NORMAL;
            pButton->config.bgColorHover = DEFAULT_BG_HOVER;
            pButton->config.bgColorPressed = DEFAULT_BG_PRESSED;
            pButton->config.bgColorDisabled = DEFAULT_BG_DISABLED;
            pButton->config.borderColorNormal = DEFAULT_BORDER_NORMAL;
            pButton->config.iconSize = 24;
            pButton->config.iconTextSpacing = 4;
            pButton->config.iconAlignment = BB_ALIGN_LEFT;
            pButton->config.style = BB_STYLE_NORMAL;
            pButton->config.showText = TRUE;
        }

        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)pButton);

        // 初始化文本
        int len = GetWindowTextLength(hwnd);
        if (len > 0)
        {
            GetWindowText(hwnd, pButton->config.text, min(len + 1, 256));
        }

        return 0;
    }

    case WM_DESTROY:
        if (pButton)
        {
            // 清理资源
            HeapFree(GetProcessHeap(), 0, pButton);
            SetWindowLongPtr(hwnd, GWLP_USERDATA, 0);
        }
        return 0;

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        if (pButton)
        {
            DrawButton(hwnd, hdc);
        }

        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_ERASEBKGND:
        return 1; // 我们已经绘制了所有内容

    case WM_MOUSEMOVE:
        if (pButton && pButton->state != BBS_DISABLED)
        {
            if (!pButton->tracking)
            {
                // 开始跟踪鼠标离开
                TRACKMOUSEEVENT tme;
                tme.cbSize = sizeof(TRACKMOUSEEVENT);
                tme.dwFlags = TME_LEAVE;
                tme.hwndTrack = hwnd;
                TrackMouseEvent(&tme);

                pButton->tracking = TRUE;
                SetButtonState(hwnd, BBS_HOVER);
            }
        }
        return 0;

    case WM_MOUSELEAVE:
        if (pButton)
        {
            pButton->tracking = FALSE;
            if (pButton->state != BBS_DISABLED)
            {
                SetButtonState(hwnd, pButton->hasFocus ? BBS_FOCUSED : BBS_NORMAL);
            }
        }
        return 0;

    case WM_LBUTTONDOWN:
        if (pButton && pButton->state != BBS_DISABLED)
        {
            SetCapture(hwnd);
            SetButtonState(hwnd, BBS_PRESSED);
        }
        return 0;

    case WM_LBUTTONUP:
        if (pButton && pButton->state != BBS_DISABLED)
        {
            ReleaseCapture();

            // 检查鼠标是否在按钮上
            POINT pt = { LOWORD(lParam), HIWORD(lParam) };
            RECT rc;
            GetClientRect(hwnd, &rc);

            if (PtInRect(&rc, pt))
            {
                SetButtonState(hwnd, BBS_HOVER);

                // 发送点击通知
                if (pButton->config.onClickCallback)
                {
                    pButton->config.onClickCallback(hwnd, pButton->config.userData);
                }

                // 发送WM_COMMAND给父窗口
                int id = GetWindowLong(hwnd, GWL_ID);
                SendMessage(pButton->hParent, WM_COMMAND, MAKEWPARAM(id, BN_CLICKED), (LPARAM)hwnd);
            }
            else
            {
                SetButtonState(hwnd, pButton->hasFocus ? BBS_FOCUSED : BBS_NORMAL);
            }
        }
        return 0;

    case WM_KEYDOWN:
        if (pButton && pButton->state != BBS_DISABLED)
        {
            if (wParam == VK_SPACE || wParam == VK_RETURN)
            {
                SetButtonState(hwnd, BBS_PRESSED);
            }
        }
        return 0;

    case WM_KEYUP:
        if (pButton && pButton->state != BBS_DISABLED)
        {
            if (wParam == VK_SPACE || wParam == VK_RETURN)
            {
                SetButtonState(hwnd, BBS_FOCUSED);

                // 发送点击通知
                if (pButton->config.onClickCallback)
                {
                    pButton->config.onClickCallback(hwnd, pButton->config.userData);
                }

                // 发送WM_COMMAND给父窗口
                int id = GetWindowLong(hwnd, GWL_ID);
                SendMessage(pButton->hParent, WM_COMMAND, MAKEWPARAM(id, BN_CLICKED), (LPARAM)hwnd);
            }
        }
        return 0;

    case WM_SETFOCUS:
        if (pButton && pButton->state != BBS_DISABLED)
        {
            pButton->hasFocus = TRUE;
            SetButtonState(hwnd, BBS_FOCUSED);
        }
        return 0;

    case WM_KILLFOCUS:
        if (pButton)
        {
            pButton->hasFocus = FALSE;
            if (pButton->state != BBS_DISABLED)
            {
                SetButtonState(hwnd, pButton->tracking ? BBS_HOVER : BBS_NORMAL);
            }
        }
        return 0;

    case WM_ENABLE:
        if (pButton)
        {
            if (wParam)
            {
                SetButtonState(hwnd, BBS_NORMAL);
            }
            else
            {
                SetButtonState(hwnd, BBS_DISABLED);
            }
        }
        return 0;

    case WM_SETTEXT:
        if (pButton)
        {
            // 更新文本
            wcsncpy_s(pButton->config.text, (LPCWSTR)lParam, 255);
            pButton->config.text[255] = L'\0';
            InvalidateRect(hwnd, NULL, TRUE);
        }
        return DefWindowProc(hwnd, msg, wParam, lParam);

    case BB_SETICON:  // 自定义消息：设置图标
        if (pButton)
        {
            BUTTON_STATE state = (BUTTON_STATE)wParam;
            HICON hIcon = (HICON)lParam;

            switch (state)
            {
            case BBS_NORMAL: pButton->config.hIcon = hIcon; break;
            case BBS_HOVER: pButton->config.hIconHover = hIcon; break;
            case BBS_PRESSED: pButton->config.hIconPressed = hIcon; break;
            case BBS_DISABLED: pButton->config.hIconDisabled = hIcon; break;
            }

            InvalidateRect(hwnd, NULL, TRUE);
            return TRUE;
        }
        return FALSE;

    case BB_SETCOLORS:  // 自定义消息：设置颜色
        if (pButton)
        {
            COLORREF* pColors = (COLORREF*)lParam;
            BUTTON_STATE state = (BUTTON_STATE)wParam;

            switch (state)
            {
            case BBS_NORMAL:
                if (pColors[0] != CLR_DEFAULT) pButton->config.bgColorNormal = pColors[0];
                if (pColors[1] != CLR_DEFAULT) pButton->config.borderColorNormal = pColors[1];
                break;
            case BBS_HOVER:
                if (pColors[0] != CLR_DEFAULT) pButton->config.bgColorHover = pColors[0];
                if (pColors[1] != CLR_DEFAULT) pButton->config.borderColorHover = pColors[1];
                break;
            case BBS_PRESSED:
                if (pColors[0] != CLR_DEFAULT) pButton->config.bgColorPressed = pColors[0];
                if (pColors[1] != CLR_DEFAULT) pButton->config.borderColorPressed = pColors[1];
                break;
            case BBS_DISABLED:
                if (pColors[0] != CLR_DEFAULT) pButton->config.bgColorDisabled = pColors[0];
                break;
            }

            InvalidateRect(hwnd, NULL, TRUE);
            return TRUE;
        }
        return FALSE;

    case WM_SIZE:
    case WM_MOVE:
        InvalidateRect(hwnd, NULL, TRUE);
        return 0;

    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
}

// 注册窗口类
BOOL RegisterBorderlessButtonClass(HINSTANCE hInstance)
{
    WNDCLASS wc = { 0 };

    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = BorderlessButtonWndProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = hInstance;
    wc.hIcon = NULL;
    wc.hCursor = LoadCursor(NULL, IDC_HAND);
    wc.hbrBackground = NULL;
    wc.lpszMenuName = NULL;
    wc.lpszClassName = g_szButtonClass;

    return RegisterClass(&wc) != 0;
}

// 注销窗口类
void UnregisterBorderlessButtonClass(HINSTANCE hInstance)
{
    UnregisterClass(g_szButtonClass, hInstance);
}

// 创建按钮
HWND CreateBorderlessButton(
    HWND hParent,
    HINSTANCE hInstance,
    int x, int y,
    int width, int height,
    LPCWSTR text,
    HICON hIcon,
    int id,
    BORDERLESS_BUTTON_CONFIG config)
{
    // 确保窗口类已注册
    static BOOL registered = FALSE;
    if (!registered)
    {
        if (!RegisterBorderlessButtonClass(hInstance))
            return NULL;
        registered = TRUE;
    }

    // Apply defaults if 0
    if (config.bgColorNormal == 0) config.bgColorNormal = DEFAULT_BG_NORMAL;
    if (config.bgColorHover == 0) config.bgColorHover = DEFAULT_BG_HOVER;
    if (config.bgColorPressed == 0) config.bgColorPressed = DEFAULT_BG_PRESSED;
    if (config.bgColorDisabled == 0) config.bgColorDisabled = DEFAULT_BG_DISABLED;
    
    if (config.borderColorNormal == 0) config.borderColorNormal = DEFAULT_BORDER_NORMAL;
    if (config.borderColorHover == 0) config.borderColorHover = DEFAULT_BORDER_HOVER;
    if (config.borderColorPressed == 0) config.borderColorPressed = DEFAULT_BORDER_PRESSED;
    
    if (config.iconSize == 0) config.iconSize = 24;
    if (config.iconTextSpacing == 0) config.iconTextSpacing = 4;

    // ??
    if (hIcon) config.hIcon = hIcon;
    if (text) wcsncpy_s(config.text, text, 255);

    // 
    HWND hwnd = CreateWindowEx(
        0,
        g_szButtonClass,
        text,
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_CLIPSIBLINGS,
        x, y, width, height,
        hParent,
        (HMENU)(INT_PTR)id,
        hInstance,
        &config
    );

    return hwnd;
}

// 设置图标
void BorderlessButtonSetIcon(HWND hButton, HICON hIcon, BUTTON_STATE state)
{
    SendMessage(hButton, BB_SETICON, (WPARAM)state, (LPARAM)hIcon);
}

// 设置文本
void BorderlessButtonSetText(HWND hButton, LPCWSTR text)
{
    SetWindowText(hButton, text);
}

// 设置颜色
void BorderlessButtonSetColors(HWND hButton, COLORREF bgColor, COLORREF borderColor, BUTTON_STATE state)
{
    COLORREF colors[2] = { bgColor, borderColor };
    SendMessage(hButton, BB_SETCOLORS, (WPARAM)state, (LPARAM)colors);
}

// 设置样式
void BorderlessButtonSetStyle(HWND hButton, UINT style)
{
    BORDERLESS_BUTTON* pButton = GetButtonData(hButton);
    if (pButton)
    {
        pButton->config.style = style;
        InvalidateRect(hButton, NULL, TRUE);
    }
}

// 设置状态
void BorderlessButtonSetState(HWND hButton, BUTTON_STATE state)
{
    SetButtonState(hButton, state);
}

// 设置回调函数
void BorderlessButtonSetCallback(HWND hButton, BUTTON_CLICK_CALLBACK callback, void* userData)
{
    BORDERLESS_BUTTON* pButton = GetButtonData(hButton);
    if (pButton)
    {
        pButton->config.onClickCallback = callback;
        pButton->config.userData = userData;
    }
}

// 设置自定义绘制函数
void BorderlessButtonSetCustomDraw(HWND hButton, BUTTON_DRAW_CALLBACK callback, void* userData)
{
    BORDERLESS_BUTTON* pButton = GetButtonData(hButton);
    if (pButton)
    {
        pButton->config.customDrawCallback = callback;
        pButton->config.userData = userData;
    }
}

// 获取图标
HICON BorderlessButtonGetIcon(HWND hButton, BUTTON_STATE state)
{
    BORDERLESS_BUTTON* pButton = GetButtonData(hButton);
    if (!pButton) return NULL;

    switch (state)
    {
    case BBS_HOVER: return pButton->config.hIconHover;
    case BBS_PRESSED: return pButton->config.hIconPressed;
    case BBS_DISABLED: return pButton->config.hIconDisabled;
    default: return pButton->config.hIcon;
    }
}

// 获取文本
LPCWSTR BorderlessButtonGetText(HWND hButton)
{
    BORDERLESS_BUTTON* pButton = GetButtonData(hButton);
    return pButton ? pButton->config.text : L"";
}

// 获取状态
BUTTON_STATE BorderlessButtonGetState(HWND hButton)
{
    BORDERLESS_BUTTON* pButton = GetButtonData(hButton);
    return pButton ? pButton->state : BBS_NORMAL;
}

// 从文件加载图标
HICON LoadIconFromFile(LPCWSTR filename, int size)
{
    return (HICON)LoadImage(NULL, filename, IMAGE_ICON, size, size, LR_LOADFROMFILE);
}

// 从资源加载图标
HICON LoadIconFromResource(HINSTANCE hInstance, LPCWSTR resName, int size)
{
    return (HICON)LoadImage(hInstance, resName, IMAGE_ICON, size, size, 0);
}

// 创建纯色图标
HICON CreateColorIcon(COLORREF color, int size)
{
    HDC hdc = GetDC(NULL);
    HDC hdcMem = CreateCompatibleDC(hdc);

    HBITMAP hBitmap = CreateCompatibleBitmap(hdc, size, size);
    HBITMAP hOldBitmap = (HBITMAP)SelectObject(hdcMem, hBitmap);

    // 绘制纯色图标
    HBRUSH hBrush = CreateSolidBrush(color);
    RECT rc = { 0, 0, size, size };
    FillRect(hdcMem, &rc, hBrush);

    // 创建图标
    ICONINFO iconInfo;
    iconInfo.fIcon = TRUE;
    iconInfo.xHotspot = 0;
    iconInfo.yHotspot = 0;
    iconInfo.hbmMask = hBitmap;
    iconInfo.hbmColor = hBitmap;

    HICON hIcon = CreateIconIndirect(&iconInfo);

    // 清理
    DeleteObject(hBrush);
    SelectObject(hdcMem, hOldBitmap);
    DeleteObject(hBitmap);
    DeleteDC(hdcMem);
    ReleaseDC(NULL, hdc);

    return hIcon;
}