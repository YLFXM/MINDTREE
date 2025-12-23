//ȫ�ֱ���������ͷ�ļ�
#pragma once

#include "resource.h"
#include <string>
#define MAX_LOADSTRING 100

#include"borderless_button.h"

namespace global {
    extern std::wstring currentFilePath;

	// ȫ�ֱ���:
	extern HINSTANCE hInst;                                // ��ǰʵ��
	extern WCHAR szTitle[MAX_LOADSTRING];                  // �������ı�
	extern WCHAR szWindowClass[MAX_LOADSTRING];            // ����������
	extern HWND HOME;										//�����ھ��
	extern HWND TOOLS;                                     //���������ھ��
	extern WCHAR ToolsClass[MAX_LOADSTRING];
	extern HWND GUIDE;                                     //������崰�ھ��
	extern WCHAR GuideClass[MAX_LOADSTRING];
	extern HWND STYLE;                                     //�Ҳ���崰�ھ��
	extern WCHAR StyleClass[MAX_LOADSTRING];
	extern HWND EXPLAIN;                                   //��ݼ�˵�����ھ��
	extern HWND FILES;										//�ļ����������ھ��
	extern HWND CLOTH;										//�ղر��������ھ��
	//extern HWND g_canvasWnd;

	extern BORDERLESS_BUTTON_CONFIG  defconfig;

	// �ؼ�ȫ�ֱ���
	extern HWND SUB, PARENT, MATE, RETURN, CLOSETOOLS, RIGHT, OPENTOOLS;//�������ؼ�
	extern RECT button;//��ťλ��
	extern RECT tools;//����������
	extern HWND G_TAB, G_CLOSE, G_TREEVIEW, G_SEARCH, G_CURRENT;//�������ؼ�
	extern RECT guide;//����������
	extern HWND S_TAB, S_CLOSE, S_CURRENT, W_STYLE, W_PAINT;//�༭���ؼ�
	extern RECT style;//�༭������

    // Zoom controls
    extern HWND ZOOM_DEC;
    extern HWND ZOOM_INC;
    extern HWND ZOOM_LABEL;
}