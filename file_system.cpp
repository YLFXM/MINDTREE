#include "file_system.h"
#include "MINDTREE.h"
#include "MindMapManager.h"
#include "history_system.h"
#include <commdlg.h>
#include <fstream>

void FileOpen(HWND hWnd) {
    OPENFILENAME ofn;       
    WCHAR szFile[260];       

    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hWnd;
    ofn.lpstrFile = szFile;
    ofn.lpstrFile[0] = '\0';
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = L"MindTree Files\0*.mt\0All Files\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = NULL;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

    if (GetOpenFileName(&ofn) == TRUE) {
        global::currentFilePath = szFile;
        
        if (MindMapManager::Instance().LoadFromFile(global::currentFilePath)) {
            // Ensure history is initialized (combined v2 load already sets it; legacy load resets it).
            if (HistorySystem::Instance().GetSnapshots().empty()) {
                HistorySystem::Instance().ResetToSnapshot(MindMapManager::Instance().CreateSnapshot());
            }
            std::wstring title = L"MINDTREE - " + global::currentFilePath;
            SetWindowText(hWnd, title.c_str());
            
            // Update Tab Title if using tabs
            if (global::FILES) {
                int cur = TabCtrl_GetCurSel(global::FILES);
                std::wstring fileName = szFile;
                size_t lastSlash = fileName.find_last_of(L"\\/");
                if (lastSlash != std::wstring::npos) fileName = fileName.substr(lastSlash + 1);
                
                TCITEM tie;
                tie.mask = TCIF_TEXT;
                tie.pszText = (LPWSTR)fileName.c_str();
                TabCtrl_SetItem(global::FILES, cur, &tie);
            }
        } else {
            MessageBox(hWnd, L"Failed to open file.", L"Error", MB_OK | MB_ICONERROR);
        }
    }
}

void FileSave(HWND hWnd) {
    if (global::currentFilePath.empty()) {
        FileSaveAs(hWnd);
    } else {
        if (MindMapManager::Instance().SaveToFile(global::currentFilePath)) {
            // Success
        } else {
            MessageBox(hWnd, L"Failed to save file.", L"Error", MB_OK | MB_ICONERROR);
        }
    }
}

void FileSaveAs(HWND hWnd) {
    OPENFILENAME ofn;       
    WCHAR szFile[260];       
    
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hWnd;
    ofn.lpstrFile = szFile;
    ofn.lpstrFile[0] = '\0';
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = L"MindTree Files\0*.mt\0All Files\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrDefExt = L"mt";
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT;

    if (GetSaveFileName(&ofn) == TRUE) {
        global::currentFilePath = szFile;
        FileSave(hWnd);
        
        std::wstring title = L"MINDTREE - " + global::currentFilePath;
        SetWindowText(hWnd, title.c_str());
    }
}

void FileNew(HWND hWnd) {
    global::currentFilePath = L"";
    SetWindowText(hWnd, L"MINDTREE - Untitled");
    MindMapManager::Instance().CreateRootUI();
    HistorySystem::Instance().ResetToSnapshot(MindMapManager::Instance().CreateSnapshot());
}

void FileClose(HWND hWnd) {
    global::currentFilePath = L"";
    SetWindowText(hWnd, L"MINDTREE");
    // Clear UI and Data
    if (global::G_TREEVIEW) TreeView_DeleteAllItems(global::G_TREEVIEW);
    MindMapManager::Instance().CreateNewMap(L""); // Or just clear it
    // Actually CreateRootUI resets it to a default state, which is usually what "Close" -> "New" does, 
    // but "Close" might mean showing nothing. For now let's just reset to root.
    MindMapManager::Instance().CreateRootUI();
    HistorySystem::Instance().ResetToSnapshot(MindMapManager::Instance().CreateSnapshot());
}
