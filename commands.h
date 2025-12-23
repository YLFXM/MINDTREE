#pragma once
class MindNode;      // forward declaration
#include "framework.h"
#include <string>

// Command handlers for menu actions. Implementations in commands.cpp
void HandleSubtopic(HWND hWnd);
void HandleWorkmate(HWND hWnd);
void HandleParent(HWND hWnd);
void HandleFreeTopic(HWND hWnd);
void HandleLabel(HWND hWnd);
void HandleAnnotation(HWND hWnd);
void HandleSummary(HWND hWnd);
void HandleLink(HWND hWnd);
void HandleFrame(HWND hWnd);
void HandleNote(HWND hWnd);
void HandleOutlinkPage(HWND hWnd);
void HandleOutlinkFile(HWND hWnd);
void HandleOutlinkTopic(HWND hWnd);
void HandleNewPaint(HWND hWnd);
void HandleNewPaintWithTopic(HWND hWnd);
void HandleLarger(HWND hWnd);
void HandleSmaller(HWND hWnd);
void HandleAdapt(HWND hWnd);
void HandleLineOnly(HWND hWnd);
void HandleStructure(HWND hWnd); // Added declaration

// Link-mode helpers (used by canvas for hit-test completion and ESC cancel)
bool LinkModeIsActive();
MindNode* LinkModeGetSource();
void LinkModeStart(MindNode* sourceNode);
void LinkModeCancel();
bool LinkModeTryComplete(MindNode* targetNode);

// Helper prompt: modal input box (returns empty if cancelled)
std::wstring PromptForString(HWND hWnd, const std::wstring& title, const std::wstring& initial = L"");
