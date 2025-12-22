//控件便捷函数头文件
#ifndef BUTTON_H
#define BUTTON_H

#include"borderless_button.h"
#include"MINDTREE.h"

HWND CreateIconButton(HWND hWndParent, HINSTANCE hInstance, int ID, int x, int y, int Width, int Height, int IconID);
HWND CreateText(HWND hWndParent, HINSTANCE hInstance, int ID, int x, int y, int Width, int Height, LPCWSTR Text);
HWND CreateButton(HWND hWndParent, HINSTANCE hInstance, int ID, int x, int y, int Width, int Height, LPCWSTR Text);
HWND CreateContentTree(HWND hWndParent, HINSTANCE hInstance, int ID, int x, int y, int Width, int Height);
HWND CreateDropList(HWND Paarent, HINSTANCE hInstance, int ID, int x, int y, int Width, int Height, LPCWSTR* Items, int ItemCount);
HWND CreateEditBox(HWND Parent, HINSTANCE hInstance, int ID, int x, int y, int Width, int Height, LPCWSTR Text);
HWND CreateCheckBox(HWND hWndParent, HINSTANCE hInstance, int ID, int x, int y, int width, int height, LPCWSTR text);
HWND CreateNoborderButton(HWND hWndParent, HINSTANCE hInstance, int x, int y, int width, int height, HICON hIcon, int ID, const BORDERLESS_BUTTON_CONFIG& config = global::defconfig);

#endif

