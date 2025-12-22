// 简单画布测试桩接口（用于 commands.cpp 的占位实现）
#pragma once
#include <windows.h>
#include <string>
#include "MindMapData.h"

// Zoom management
void Canvas_ZoomBy(int deltaPercent);
void Canvas_SetZoom(int percent);
int  Canvas_GetZoom();

// Fit / invalidate
void Canvas_FitToView();
void Canvas_Invalidate();

// Branch filter (only show subtree rooted at hRoot)
void Canvas_SetFilterRoot(HTREEITEM hRoot);
void Canvas_ClearFilter();

// Paint file helpers
void Canvas_LoadPaintForNode(MindNode* node);
void Canvas_ShowPaintForNode(MindNode* node);
