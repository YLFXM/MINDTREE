#pragma once
#include "framework.h"

void EditUndo(HWND hWnd);
void EditRedo(HWND hWnd);
void EditCut(HWND hWnd);
void EditCopy(HWND hWnd);
void EditPaste(HWND hWnd);
void EditDeleteSub(HWND hWnd);
void EditDelete(HWND hWnd);
void EditSelectAll(HWND hWnd);
void EditSearch(HWND hWnd);

// Search/Replace Support
extern UINT uFindReplaceMsg;
void InitSearchReplace();
void ProcessSearchReplace(HWND hWnd, LPARAM lParam);
