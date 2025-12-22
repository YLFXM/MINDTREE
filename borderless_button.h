#ifndef BORDERLESS_BUTTON_H
#define BORDERLESS_BUTTON_H

#include <windows.h>
#include <commctrl.h>

// 按钮样式标志
#define BB_STYLE_NORMAL        0x0000
#define BB_STYLE_FLAT          0x0001
#define BB_STYLE_ROUNDED       0x0002
#define BB_STYLE_SHADOW        0x0004
#define BB_STYLE_TOOLBAR       0x0008

// 自定义消息
#define BB_SETICON   (WM_APP + 1)
#define BB_SETCOLORS (WM_APP + 2)

// 图标对齐方式
#define BB_ALIGN_LEFT          0x0010
#define BB_ALIGN_RIGHT         0x0020
#define BB_ALIGN_TOP           0x0040
#define BB_ALIGN_BOTTOM        0x0080
#define BB_ALIGN_CENTER        0x0100

// 按钮状态
typedef enum {
    BBS_NORMAL = 0,
    BBS_HOVER,
    BBS_PRESSED,
    BBS_DISABLED,
    BBS_FOCUSED
} BUTTON_STATE;

// 按钮回调函数类型
typedef void (*BUTTON_CLICK_CALLBACK)(HWND hButton, void* userData);
typedef void (*BUTTON_DRAW_CALLBACK)(HWND hButton, HDC hdc, RECT* rc, BUTTON_STATE state, void* userData);

// 按钮配置结构
typedef struct {
    HICON hIcon;                    // 图标句柄
    HICON hIconHover;               // 悬停时图标
    HICON hIconPressed;             // 按下时图标
    HICON hIconDisabled;            // 禁用时图标

    COLORREF bgColorNormal;         // 正常背景色
    COLORREF bgColorHover;          // 悬停背景色
    COLORREF bgColorPressed;        // 按下背景色
    COLORREF bgColorDisabled;       // 禁用背景色

    COLORREF borderColorNormal;     // 正常边框色
    COLORREF borderColorHover;      // 悬停边框色
    COLORREF borderColorPressed;    // 按下边框色

    int borderWidth;                // 边框宽度（0表示无边框）
    int cornerRadius;               // 圆角半径
    int iconTextSpacing;            // 图标和文本间距
    int iconSize;                   // 图标大小

    UINT style;                     // 样式标志
    UINT iconAlignment;             // 图标对齐方式

    BOOL showText;                  // 是否显示文本
    WCHAR text[256];                // 按钮文本

    BUTTON_CLICK_CALLBACK onClickCallback;  // 点击回调
    BUTTON_DRAW_CALLBACK customDrawCallback; // 自定义绘制回调
    void* userData;                 // 用户数据
} BORDERLESS_BUTTON_CONFIG;

// 按钮实例数据
typedef struct {
    HWND hwnd;
    HWND hParent;

    BUTTON_STATE state;
    BORDERLESS_BUTTON_CONFIG config;

    BOOL tracking;                  // 是否正在跟踪鼠标
    BOOL hasFocus;                  // 是否有键盘焦点

    // 动态颜色（如果未设置则使用默认值）
    COLORREF bgColor;
    COLORREF borderColor;
    COLORREF textColor;
} BORDERLESS_BUTTON;

// 公共函数声明
BOOL RegisterBorderlessButtonClass(HINSTANCE hInstance);
void UnregisterBorderlessButtonClass(HINSTANCE hInstance);

HWND CreateBorderlessButton(
    HWND hParent,
    HINSTANCE hInstance,
    int x, int y,
    int width, int height,
    LPCWSTR text,
    HICON hIcon,
    int id,
    BORDERLESS_BUTTON_CONFIG config
);

void BorderlessButtonSetIcon(HWND hButton, HICON hIcon, BUTTON_STATE state);
void BorderlessButtonSetText(HWND hButton, LPCWSTR text);
void BorderlessButtonSetColors(HWND hButton, COLORREF bgColor, COLORREF borderColor, BUTTON_STATE state);
void BorderlessButtonSetStyle(HWND hButton, UINT style);
void BorderlessButtonSetState(HWND hButton, BUTTON_STATE state);
void BorderlessButtonSetCallback(HWND hButton, BUTTON_CLICK_CALLBACK callback, void* userData);
void BorderlessButtonSetCustomDraw(HWND hButton, BUTTON_DRAW_CALLBACK callback, void* userData);

HICON BorderlessButtonGetIcon(HWND hButton, BUTTON_STATE state);
LPCWSTR BorderlessButtonGetText(HWND hButton);
BUTTON_STATE BorderlessButtonGetState(HWND hButton);

// 工具函数
HICON LoadIconFromFile(LPCWSTR filename, int size);
HICON LoadIconFromResource(HINSTANCE hInstance, LPCWSTR resName, int size);
HICON CreateColorIcon(COLORREF color, int size);

#endif // BORDERLESS_BUTTON_H#pragma once
