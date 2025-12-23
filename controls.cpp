//创建控件便捷函数源文件

#include "framework.h"
#include "controls.h"
#include "borderless_button.h"
#include "MINDTREE.h"

HWND CreateIconButton(HWND hWndParent, HINSTANCE hInstance, int ID, int x, int y, int Width, int Height, int IconID) {
	HWND button = CreateWindow(
		WC_BUTTON,  // 按钮类名
		L"",      // 按钮文本
		WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON | BS_ICON,  // 按钮样式
		x,         // x 坐标
		y,         // y 坐标
		Width,        // 宽度
		Height,        // 高度
		hWndParent,     // 父窗口句柄
		(HMENU)ID,       // 按钮 ID
		hInstance,
		NULL);      // 不使用额外参数
	SendMessage(button, BM_SETIMAGE, IMAGE_ICON, (LPARAM)LoadIcon(hInstance, MAKEINTRESOURCE(IconID)));
	return button;
}

HWND CreateText(HWND hWndParent, HINSTANCE hInstance, int ID, int x, int y, int width, int height, LPCWSTR text) {
	HWND textControl = CreateWindow(
		L"STATIC",  // 静态文本类名
		text,       // 静态文本内容
		WS_VISIBLE | WS_CHILD | SS_LEFT,  // 静态文本样式
		x,          // x 坐标
		y,          // y 坐标
		width,      // 宽度
		height,     // 高度
		hWndParent, // 父窗口句柄
		(HMENU)ID,  // 控件 ID
		hInstance,
		NULL);      // 不使用额外参数
	return textControl;
}

HWND CreateButton(HWND hWndParent, HINSTANCE hInstance, int ID, int x, int y, int width, int height, LPCWSTR text) {
	HWND button = CreateWindow(
		WC_BUTTON,  // 按钮类名
		text,       // 按钮文本
		WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,  // 按钮样式
		x,          // x 坐标
		y,          // y 坐标
		width,      // 宽度
		height,     // 高度
		hWndParent, // 父窗口句柄
		(HMENU)ID,  // 按钮 ID
		hInstance,
		NULL);      // 不使用额外参数
	return button;
}

HWND CreateContentTree(HWND hWndParent, HINSTANCE hInstance, int ID, int x, int y, int width, int height) {
	HWND treeView = CreateWindowEx(
		0,
		WC_TREEVIEW,  // 树视图类名
		L"",          // 树视图标题
		WS_VISIBLE | WS_CHILD | WS_BORDER | TVS_HASLINES | TVS_LINESATROOT | TVS_HASBUTTONS,  // 树视图样式
		x,            // x 坐标
		y,            // y 坐标
		width,        // 宽度
		height,       // 高度
		hWndParent,   // 父窗口句柄
		(HMENU)ID,    // 控件 ID
		hInstance,
		NULL);        // 不使用额外参数
	return treeView;
}

HWND CreateDropList(HWND Paarent, HINSTANCE hInstance, int ID, int x, int y, int Width, int Height, LPCWSTR* Items, int ItemCount) {
	HWND dropList = CreateWindowEx(
		WS_EX_CLIENTEDGE,
		WC_COMBOBOX,  // 下拉列表类名
		L"",          // 下拉列表标题
		WS_VISIBLE | WS_CHILD | CBS_DROPDOWNLIST | WS_VSCROLL,  // 下拉列表样式
		x,            // x 坐标
		y,            // y 坐标
		Width,        // 宽度
		Height,       // 高度
		Paarent,      // 父窗口句柄
		(HMENU)ID,    // 控件 ID
		hInstance,
		NULL);        // 不使用额外参数
	for (int i = 0; i < ItemCount; ++i) {
		SendMessage(dropList, CB_ADDSTRING, 0, (LPARAM)Items[i]);
	}
	if (ItemCount > 0) {
		SendMessage(dropList, CB_SETCURSEL, 0, 0); // 设置默认选中第一个项
	}
	return dropList;
}

HWND CreateEditBox(HWND Parent, HINSTANCE hInstance, int ID, int x, int y, int Width, int Height, LPCWSTR Text) {
	HWND editBox = CreateWindowEx(
		WS_EX_CLIENTEDGE,
		WC_EDIT,      // 编辑框类名
		Text,         // 编辑框初始文本
		WS_VISIBLE | WS_CHILD | WS_BORDER | ES_LEFT | ES_AUTOHSCROLL,  // 编辑框样式
		x,            // x 坐标
		y,            // y 坐标
		Width,        // 宽度
		Height,       // 高度
		Parent,       // 父窗口句柄
		(HMENU)ID,    // 控件 ID
		hInstance,
		NULL);        // 不使用额外参数
	return editBox;
}

// 创建复选框
HWND CreateCheckBox(HWND hWndParent, HINSTANCE hInstance, int ID, int x, int y, int width, int height, LPCWSTR text) {
	HWND checkBox = CreateWindow(
		WC_BUTTON,     // 按钮类名
		text,          // 复选框文本
		WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX,  // 复选框样式
		x,             // x 坐标
		y,             // y 坐标
		width,         // 宽度
		height,        // 高度
		hWndParent,    // 父窗口句柄
		(HMENU)ID,     // 控件 ID
		hInstance,
		NULL);
	return checkBox;
}

// 修改：定义改为接受 const 引用，移除默认参数并使用局部副本传递给 CreateBorderlessButton
HWND CreateNoborderButton(HWND hParent, HINSTANCE hInstance, int x, int y, int width, int height, HICON hIcon, int id, const BORDERLESS_BUTTON_CONFIG& config) {
	BORDERLESS_BUTTON_CONFIG cfg = config; // 局部副本
	cfg.hIcon = hIcon;
	return CreateBorderlessButton(
		hParent,
		hInstance,
		x,
		y,
		width,
		height,
		NULL,
		hIcon,
		id,
		cfg
	);
}