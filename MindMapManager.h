#pragma once
class MindNode;      // forward declaration
#include "MindMapData.h"
#include "MINDTREE.h"
#include <map>
#include <set>
#include <vector>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cwctype>

#include "history_system.h"

class MindMapManager {
public:
    static MindMapManager& Instance() {
        static MindMapManager instance;
        return instance;
    }

    MindNode* GetRoot() { return root; }
    const std::vector<MindNode*>& GetFreeNodes() const { return freeNodes; }

    // Multi-selection and Frames
    void ClearSelection() { selectedNodes.clear(); }
    void AddToSelection(MindNode* node) { if (node) selectedNodes.insert(node); }
    const std::set<MindNode*>& GetSelectedNodes() const { return selectedNodes; }
    bool IsNodeSelected(MindNode* node) const { return selectedNodes.find(node) != selectedNodes.end(); }

    void AddFrame(const std::vector<int>& nodeIds, const std::wstring& style) {
        frames.push_back({ nodeIds, style });
    }
    const std::vector<MindFrame>& GetFrames() const { return frames; }

    // Collect all nodes that contain a non-empty note (for navigation panel)
    std::vector<MindNode*> GetAllNodesWithNote() {
        std::vector<MindNode*> result;
        std::vector<MindNode*> allNodes;
        FlattenTree(root, allNodes);
        for (auto fn : freeNodes) {
            FlattenTree(fn, allNodes);
        }

        for (MindNode* n : allNodes) {
            if (n && !n->note.empty()) {
                result.push_back(n);
            }
        }
        std::sort(result.begin(), result.end(), [](MindNode* a, MindNode* b) {
            return a && b ? a->id < b->id : a < b;
        });
        return result;
    }

    // Collect all nodes that contain a non-empty label (for navigation panel)
    std::vector<MindNode*> GetAllNodesWithLabel() {
        std::vector<MindNode*> result;
        std::vector<MindNode*> allNodes;
        FlattenTree(root, allNodes);
        for (auto fn : freeNodes) {
            FlattenTree(fn, allNodes);
        }

        for (MindNode* n : allNodes) {
            if (n && !n->label.empty()) {
                result.push_back(n);
            }
        }
        std::sort(result.begin(), result.end(), [](MindNode* a, MindNode* b) {
            return a && b ? a->id < b->id : a < b;
        });
        return result;
    }

    // Add a top-level free node (no parent). UI: insert at root level in TreeView.
    MindNode* AddFreeNode(const std::wstring& text = L"Free Topic") {
        if (!global::G_TREEVIEW) return nullptr;
        MindNode* newNode = new MindNode(nextId++, text, nullptr);
        freeNodes.push_back(newNode);

        TVINSERTSTRUCT tvis{};
        tvis.hParent = TVI_ROOT;
        tvis.hInsertAfter = TVI_LAST;
        tvis.item.mask = TVIF_TEXT | TVIF_PARAM;
        tvis.item.pszText = (LPWSTR)newNode->text.c_str();
        tvis.item.lParam = (LPARAM)newNode->id;
        newNode->hTreeItem = TreeView_InsertItem(global::G_TREEVIEW, &tvis);
        if (newNode->hTreeItem) {
            nodeMap[newNode->hTreeItem] = newNode;
            TreeView_SelectItem(global::G_TREEVIEW, newNode->hTreeItem);
        }
        return newNode;
    }

    // Initialize with a root node
    void CreateNewMap(std::wstring rootText) {
        if (root) delete root;
        for (auto fn : freeNodes) delete fn;
        freeNodes.clear();
        nodeMap.clear();
        nextId = 1;
        
        root = new MindNode(nextId++, rootText);
        nodeMap[root->hTreeItem] = root; // Note: hTreeItem is null here, need to set after UI creation
    }

    // Add a sub-node to the currently selected node in TreeView
    void AddSubNodeToSelected(const std::wstring& text = L"Sub Topic") {
        if (!global::G_TREEVIEW) return;
        
        HTREEITEM hSelected = TreeView_GetSelection(global::G_TREEVIEW);
        if (!hSelected) {
            // Ensure we at least have a root and default-select it for convenience
            if (!root) {
                CreateRootUI();
            }
            HTREEITEM hRoot = (root && root->hTreeItem) ? root->hTreeItem : TreeView_GetRoot(global::G_TREEVIEW);
            if (hRoot) {
                TreeView_SelectItem(global::G_TREEVIEW, hRoot);
                SetFocus(global::G_TREEVIEW);
                hSelected = hRoot;
            } else {
                MessageBox(NULL, L"Please select a parent node", L"Add Node", MB_OK);
                return;
            }
        }

        MindNode* parentNode = GetNodeFromHandle(hSelected);
        if (parentNode) {
            MindNode* newNode = new MindNode(nextId++, text, parentNode);
            parentNode->children.push_back(newNode);

            // Update UI
            TVINSERTSTRUCT tvis;
            tvis.hParent = hSelected;
            tvis.hInsertAfter = TVI_LAST;
            tvis.item.mask = TVIF_TEXT | TVIF_PARAM;
            tvis.item.pszText = (LPWSTR)text.c_str();
            tvis.item.lParam = (LPARAM)newNode->id; // Store ID in lParam
            
            newNode->hTreeItem = TreeView_InsertItem(global::G_TREEVIEW, &tvis);
            nodeMap[newNode->hTreeItem] = newNode;
            
            TreeView_Expand(global::G_TREEVIEW, hSelected, TVE_EXPAND);
            TreeView_SelectItem(global::G_TREEVIEW, newNode->hTreeItem);
            SetFocus(global::G_TREEVIEW);
        }
    }

    // Add a sibling node
    void AddSiblingNodeToSelected(const std::wstring& text = L"Sibling Topic") {
        if (!global::G_TREEVIEW) return;

        HTREEITEM hSelected = TreeView_GetSelection(global::G_TREEVIEW);
        if (!hSelected) {
            MessageBox(NULL, L"Please select a node first", L"Add Node", MB_OK);
            return;
        }

        MindNode* selectedNode = GetNodeFromHandle(hSelected);
        if (selectedNode && selectedNode->parent) {
            MindNode* parentNode = selectedNode->parent;
            MindNode* newNode = new MindNode(nextId++, text, parentNode);
            parentNode->children.push_back(newNode);

            // Update UI
            TVINSERTSTRUCT tvis;
            tvis.hParent = TreeView_GetParent(global::G_TREEVIEW, hSelected);
            tvis.hInsertAfter = hSelected;
            tvis.item.mask = TVIF_TEXT | TVIF_PARAM;
            tvis.item.pszText = (LPWSTR)text.c_str();
            tvis.item.lParam = (LPARAM)newNode->id;

            newNode->hTreeItem = TreeView_InsertItem(global::G_TREEVIEW, &tvis);
            nodeMap[newNode->hTreeItem] = newNode;

            TreeView_SelectItem(global::G_TREEVIEW, newNode->hTreeItem);
            SetFocus(global::G_TREEVIEW);
        } else if (selectedNode == root) {
             MessageBox(NULL, L"Cannot add sibling to Root", L"Add Node", MB_OK);
        }
    }

    // Add Parent (Insert between current and its parent)
    void AddParentNodeToSelected(const std::wstring& text = L"Parent Topic") {
        if (!global::G_TREEVIEW) return;

        HTREEITEM hSelected = TreeView_GetSelection(global::G_TREEVIEW);
        if (!hSelected) {
            MessageBox(NULL, L"Please select a node first", L"Add Parent", MB_OK);
            return;
        }

        MindNode* selectedNode = GetNodeFromHandle(hSelected);
        if (!selectedNode) return;

        MindNode* parentNode = selectedNode->parent;
        if (!parentNode) {
            MessageBox(NULL, L"Cannot insert parent for root node", L"Add Parent", MB_OK);
            return;
        }

        // Create new node that will become the parent of the selected node
        MindNode* newNode = new MindNode(nextId++, text, parentNode);

        // Replace selectedNode in parentNode->children with newNode
        auto it = std::find(parentNode->children.begin(), parentNode->children.end(), selectedNode);
        if (it != parentNode->children.end()) {
            *it = newNode;
        } else {
            parentNode->children.push_back(newNode);
        }

        // Reparent selectedNode under newNode
        newNode->children.push_back(selectedNode);
        selectedNode->parent = newNode;

        // Update UI: remove subtree UI mappings for the moved subtree
        RemoveNodeFromMapRecursive(selectedNode);

        // Delete UI item(s) for the selected subtree
        TreeView_DeleteItem(global::G_TREEVIEW, hSelected);

        // Insert newNode UI under the original parent handle
        HTREEITEM hParentItem = parentNode->hTreeItem;
        RecursivelyCreateNodeUI(newNode, hParentItem);

        // Select the newly inserted parent node
        if (newNode->hTreeItem) {
            TreeView_SelectItem(global::G_TREEVIEW, newNode->hTreeItem);
        }
    }

    // Delete node but keep children (reparent them to grandparent)
    void DeleteNodeKeepChildren() {
        if (!global::G_TREEVIEW) return;
        HTREEITEM hSelected = TreeView_GetSelection(global::G_TREEVIEW);
        if (!hSelected) return;

        MindNode* targetNode = GetNodeFromHandle(hSelected);
        if (!targetNode) {
            MessageBox(NULL, L"Error: Node not found in data structure.", L"Delete Node", MB_OK);
            return;
        }

        if (targetNode == root) {
            MessageBox(NULL, L"Cannot delete Root node", L"Delete Node", MB_OK);
            return;
        }

        MindNode* parentNode = targetNode->parent;
        if (!parentNode) return; // Should not happen if not root

        // 1. Update Data Structure
        // Remove target from parent's children
        auto it = std::find(parentNode->children.begin(), parentNode->children.end(), targetNode);
        if (it != parentNode->children.end()) {
            parentNode->children.erase(it);
        }

        // Move target's children to parent
        std::vector<MindNode*> childrenToMove = targetNode->children;
        
        // DEBUG: Check if children exist
        if (childrenToMove.empty()) {
            MessageBox(NULL, L"Debug: No children to move found in data structure.", L"Debug", MB_OK);
        } else {
            std::wstring msg = L"Debug: Moving " + std::to_wstring(childrenToMove.size()) + L" children.";
            MessageBox(NULL, msg.c_str(), L"Debug", MB_OK);
        }

        for (MindNode* child : childrenToMove) {
            child->parent = parentNode;
            parentNode->children.push_back(child);
        }
        
        // Clear target's children so they are not deleted when target is deleted
        targetNode->children.clear();
        
        // Remove from map
        nodeMap.erase(targetNode->hTreeItem);
        delete targetNode;

        // 2. Update UI
        // Store parent handle before deleting target
        HTREEITEM hParentItem = parentNode->hTreeItem;
        
        if (!hParentItem) {
             MessageBox(NULL, L"Debug: Parent handle is NULL!", L"Debug", MB_OK);
        }

        // Delete target from UI
        if (!TreeView_DeleteItem(global::G_TREEVIEW, hSelected)) {
             MessageBox(NULL, L"Debug: Failed to delete item from UI", L"Debug", MB_OK);
        }

        // Recreate children UI
        for (MindNode* child : childrenToMove) {
            RecursivelyCreateNodeUI(child, hParentItem);
        }
        
        // Select parent
        TreeView_SelectItem(global::G_TREEVIEW, hParentItem);
    }

    // Delete node and all its children
    void DeleteNodeAndChildren() {
        if (!global::G_TREEVIEW) return;
        HTREEITEM hSelected = TreeView_GetSelection(global::G_TREEVIEW);
        if (!hSelected) return;

        MindNode* targetNode = GetNodeFromHandle(hSelected);
        if (!targetNode) return;

        if (targetNode == root) {
            MessageBox(NULL, L"Cannot delete Root node", L"Delete Node", MB_OK);
            return;
        }

        MindNode* parentNode = targetNode->parent;
        if (parentNode) {
            // Remove from parent's children list
            auto it = std::find(parentNode->children.begin(), parentNode->children.end(), targetNode);
            if (it != parentNode->children.end()) {
                parentNode->children.erase(it);
            }
        }

        // Clean up map
        RemoveNodeFromMapRecursive(targetNode);

        // Delete from UI (this also deletes children items in UI)
        TreeView_DeleteItem(global::G_TREEVIEW, hSelected);

        // Delete data (destructor recursively deletes children)
        delete targetNode; 
    }

    void RemoveNodeFromMapRecursive(MindNode* node) {
        if (!node) return;
        nodeMap.erase(node->hTreeItem);
        for (auto child : node->children) {
            RemoveNodeFromMapRecursive(child);
        }
    }

    void RecursivelyCreateNodeUI(MindNode* node, HTREEITEM hParent) {
        TVINSERTSTRUCT tvis;
        tvis.hParent = hParent;
        tvis.hInsertAfter = TVI_LAST;
        tvis.item.mask = TVIF_TEXT | TVIF_PARAM;
        tvis.item.pszText = (LPWSTR)node->text.c_str();
        tvis.item.lParam = (LPARAM)node->id;
        
        node->hTreeItem = TreeView_InsertItem(global::G_TREEVIEW, &tvis);
        
        if (!node->hTreeItem) {
             MessageBox(NULL, L"Debug: Failed to insert child item", L"Debug", MB_OK);
        }

        nodeMap[node->hTreeItem] = node;

        for (MindNode* child : node->children) {
            RecursivelyCreateNodeUI(child, node->hTreeItem);
        }
        
        // Expand if it has children
        if (!node->children.empty()) {
            TreeView_Expand(global::G_TREEVIEW, node->hTreeItem, TVE_EXPAND);
        }
    }

    MindNode* GetNodeFromHandle(HTREEITEM hItem) {
        // Try map first
        if (nodeMap.find(hItem) != nodeMap.end()) {
            return nodeMap[hItem];
        }
        
        // Fallback: use lParam (ID) to find node
        if (global::G_TREEVIEW && hItem) {
            TVITEM tvi;
            memset(&tvi, 0, sizeof(tvi)); // Ensure zero initialization
            tvi.hItem = hItem;
            tvi.mask = TVIF_PARAM;
            
            if (TreeView_GetItem(global::G_TREEVIEW, &tvi)) {
                int id = (int)tvi.lParam;
                
                // Skip special marker IDs (like -1 for root nodes in custom views)
                if (id <= 0) {
                    return nullptr;
                }
                
                MindNode* node = FindNodeById(root, id);
                if (!node) {
                    for (auto fn : freeNodes) {
                        node = FindNodeById(fn, id);
                        if (node) break;
                    }
                }
                
                if (node) {
                    // Repair map without overriding existing tree handle if already set
                    nodeMap[hItem] = node;
                    if (!node->hTreeItem) {
                        node->hTreeItem = hItem;
                    }
                    return node;
                }
            }
        }
        
        return nullptr;
    }

    MindNode* FindNodeById(MindNode* current, int id) {
        if (!current) return nullptr;
        if (current->id == id) return current;
        for (auto child : current->children) {
            MindNode* found = FindNodeById(child, id);
            if (found) return found;
        }
        return nullptr;
    }

    void RegisterNode(HTREEITEM hItem, MindNode* node) {
        node->hTreeItem = hItem;
        nodeMap[hItem] = node;
    }

    void CreateRootUI() {
        if (!global::G_TREEVIEW) return;
        TreeView_DeleteAllItems(global::G_TREEVIEW);
        
        CreateNewMap(L"Central Topic");
        
        // Use RecursivelyCreateNodeUI for root as well, passing TVI_ROOT
        RecursivelyCreateNodeUI(root, TVI_ROOT);
        if (root && root->hTreeItem) {
            TreeView_SelectItem(global::G_TREEVIEW, root->hTreeItem);
            SetFocus(global::G_TREEVIEW);
        }

        // New document boundary: initialize operation-log history.
        HistorySystem::Instance().ResetToSnapshot(CreateSnapshot());
    }

    void ClearHandlesRecursive(MindNode* node) {
        if (!node) return;
        node->hTreeItem = NULL;
        for (auto child : node->children) {
            ClearHandlesRecursive(child);
        }
    }

    void ClearUI() {
        if (global::G_TREEVIEW) TreeView_DeleteAllItems(global::G_TREEVIEW);
        nodeMap.clear();
        if (root) ClearHandlesRecursive(root);
        for (auto fn : freeNodes) ClearHandlesRecursive(fn);
    }

    void RebuildTreeUI() {
        if (!global::G_TREEVIEW || !root) return;
        
        // Clear UI
        TreeView_DeleteAllItems(global::G_TREEVIEW);
        
        // Clear Map (handles will be invalid)
        nodeMap.clear();
        
        // Rebuild UI from existing root
        RecursivelyCreateNodeUI(root, TVI_ROOT);
        
        // Rebuild UI for free nodes
        for (auto fn : freeNodes) {
            RecursivelyCreateNodeUI(fn, TVI_ROOT);
        }
        
        if (root->hTreeItem) {
            TreeView_SelectItem(global::G_TREEVIEW, root->hTreeItem);
            SetFocus(global::G_TREEVIEW);
        }
    }

    bool SaveToFile(const std::wstring& filePath) {
        // Always save as the combined v2 (map + operation history + cursor).
        // This keeps unlimited undo/redo across app restarts.
        const HistorySystem& hs = HistorySystem::Instance();
        const std::wstring current = hs.GetSnapshots().empty() ? CreateSnapshot() : hs.GetCurrentSnapshot();
        return HistorySystem::SaveCombinedFile(filePath, current, hs.GetSnapshots(), hs.GetCursor());
    }

    void SaveNodeRecursive(std::wofstream& file, MindNode* node) {
        int parentId = (node->parent) ? node->parent->id : 0;
        // Format: ID <tab> ParentID <tab> Text
        file << node->id << L"\t" << parentId << L"\t" << node->text << std::endl;
        for (auto child : node->children) {
            SaveNodeRecursive(file, child);
        }
    }

    bool LoadFromFile(const std::wstring& filePath) {
        // Prefer combined v2 file (map snapshot + operation history)
        if (HistorySystem::LooksLikeCombinedFile(filePath)) {
            std::wstring current;
            std::vector<std::wstring> snaps;
            std::size_t cur = 0;
            if (!HistorySystem::LoadCombinedFile(filePath, current, snaps, cur)) {
                return false;
            }

            RestoreSnapshot(current);
            HistorySystem::Instance().SetStateOrReset(std::move(snaps), cur, current);

            // Sync to PageInfo
            if (currentPageIndex >= 0 && currentPageIndex < pages.size()) {
                pages[currentPageIndex].root = root;
                pages[currentPageIndex].freeNodes = freeNodes;
                pages[currentPageIndex].nextId = nextId;
                pages[currentPageIndex].filePath = filePath;
                size_t lastSlash = filePath.find_last_of(L"\\/");
                pages[currentPageIndex].title = (lastSlash != std::wstring::npos) ? filePath.substr(lastSlash + 1) : filePath;
            }
            return true;
        }

        // Legacy v1 loader (id, parentId, text) for backward compatibility
        std::wifstream file(filePath);
        file.imbue(std::locale(""));
        if (!file.is_open()) return false;

        // Clear existing data
        if (root) delete root;
        for (auto fn : freeNodes) delete fn;
        freeNodes.clear();
        root = nullptr;
        nodeMap.clear();

        // Clear UI
        if (global::G_TREEVIEW) TreeView_DeleteAllItems(global::G_TREEVIEW);

        std::wstring line;
        std::map<int, MindNode*> idMap;
        int maxId = 0;

        try {
            while (std::getline(file, line)) {
                if (line.empty()) continue;
                std::wstringstream ss(line);
                std::wstring segment;
                std::vector<std::wstring> parts;

                while(std::getline(ss, segment, L'\t')) {
                    parts.push_back(segment);
                }

                if (parts.size() >= 3) {
                    int id = std::stoi(parts[0]);
                    int parentId = std::stoi(parts[1]);
                    std::wstring text = parts[2];

                    if (id > maxId) maxId = id;

                    MindNode* newNode = new MindNode(id, text);
                    idMap[id] = newNode;

                    if (parentId == 0) {
                        if (!root) root = newNode;
                        else freeNodes.push_back(newNode);
                    } else {
                        if (idMap.find(parentId) != idMap.end()) {
                            MindNode* parent = idMap[parentId];
                            newNode->parent = parent;
                            parent->children.push_back(newNode);
                        }
                    }
                }
            }
        } catch (...) {
            MessageBox(NULL, L"Error parsing file", L"Error", MB_OK);
            if (root) delete root;
            for (auto fn : freeNodes) delete fn;
            freeNodes.clear();
            root = nullptr;
            nodeMap.clear();
        }

        nextId = maxId + 1;
        if (!root) {
            CreateNewMap(L"Central Topic");
        }

        if (root && global::G_TREEVIEW) {
            RecursivelyCreateNodeUI(root, TVI_ROOT);
            for (auto fn : freeNodes) {
                RecursivelyCreateNodeUI(fn, TVI_ROOT);
            }
        }

        // Reset operation-log history for legacy files.
        HistorySystem::Instance().ResetToSnapshot(CreateSnapshot());

        if (currentPageIndex >= 0 && currentPageIndex < pages.size()) {
            pages[currentPageIndex].root = root;
            pages[currentPageIndex].freeNodes = freeNodes;
            pages[currentPageIndex].nextId = nextId;
            pages[currentPageIndex].filePath = filePath;
            size_t lastSlash = filePath.find_last_of(L"\\/");
            pages[currentPageIndex].title = (lastSlash != std::wstring::npos) ? filePath.substr(lastSlash + 1) : filePath;
        }

        return true;
    }

    // Snapshot for Undo/Redo
    std::wstring CreateSnapshot() {
        std::wstringstream ss;
        if (root) {
            SaveNodeRecursiveToStream(ss, root);
        }
        for (auto fn : freeNodes) {
            SaveNodeRecursiveToStream(ss, fn);
        }
        return ss.str();
    }

    void RestoreSnapshot(const std::wstring& data) {
        // Clear existing
        if (root) delete root;
        for (auto fn : freeNodes) delete fn;
        freeNodes.clear();
        root = nullptr;
        nodeMap.clear();
        if (global::G_TREEVIEW) TreeView_DeleteAllItems(global::G_TREEVIEW);

        std::wstringstream ss(data);
        std::wstring line;
        std::map<int, MindNode*> idMap;
        int maxId = 0;

        auto Unescape = [](const std::wstring& in) -> std::wstring {
            std::wstring out;
            out.reserve(in.size());
            for (size_t i = 0; i < in.size(); ++i) {
                wchar_t c = in[i];
                if (c == L'\\' && i + 1 < in.size()) {
                    wchar_t n = in[i + 1];
                    if (n == L'\\') { out.push_back(L'\\'); i++; continue; }
                    if (n == L't') { out.push_back(L'\t'); i++; continue; }
                    if (n == L'n') { out.push_back(L'\n'); i++; continue; }
                    if (n == L'r') { out.push_back(L'\r'); i++; continue; }
                }
                out.push_back(c);
            }
            return out;
        };

        try {
            while (std::getline(ss, line)) {
                if (line.empty()) continue;
                std::wstringstream lss(line);
                std::wstring segment;
                std::vector<std::wstring> parts;
                while(std::getline(lss, segment, L'\t')) {
                    parts.push_back(segment);
                }

                // v2 snapshot supports extra fields; v1 has 3 fields.
                if (parts.size() >= 3) {
                    int id = std::stoi(parts[0]);
                    int parentId = std::stoi(parts[1]);
                    std::wstring text = Unescape(parts[2]);
                    if (id > maxId) maxId = id;

                    MindNode* newNode = new MindNode(id, text);

                    if (parts.size() >= 4) newNode->label = Unescape(parts[3]);
                    if (parts.size() >= 5) newNode->note = Unescape(parts[4]);
                    if (parts.size() >= 6) newNode->outlinkFile = Unescape(parts[5]);
                    if (parts.size() >= 7) newNode->outlinkPage = Unescape(parts[6]);
                    if (parts.size() >= 8) newNode->outlinkTopicId = std::stoi(parts[7]);
                    if (parts.size() >= 9) newNode->paintPath = Unescape(parts[8]);
                    if (parts.size() >= 10) newNode->summary = Unescape(parts[9]);
                    if (parts.size() >= 11) newNode->frameStyle = Unescape(parts[10]);
                    if (parts.size() >= 12) newNode->outlinkURL = Unescape(parts[11]);
                    if (parts.size() >= 13) newNode->annotation = Unescape(parts[12]);
                    if (parts.size() >= 14) {
                        std::wstring ids = parts[13];
                        size_t start = 0;
                        while (start < ids.size()) {
                            size_t comma = ids.find(L',', start);
                            std::wstring part = (comma == std::wstring::npos) ? ids.substr(start) : ids.substr(start, comma - start);
                            if (!part.empty()) {
                                newNode->linkedNodeIds.push_back(std::stoi(part));
                            }
                            if (comma == std::wstring::npos) break;
                            start = comma + 1;
                        }
                    }

                    idMap[id] = newNode;

                    if (parentId == 0) {
                        if (!root) root = newNode;
                        else freeNodes.push_back(newNode);
                    } else {
                        if (idMap.find(parentId) != idMap.end()) {
                            MindNode* parent = idMap[parentId];
                            newNode->parent = parent;
                            parent->children.push_back(newNode);
                        }
                    }
                }
            }
        } catch (...) {
            // Silent fail
        }
        nextId = maxId + 1;
        if (!root) CreateNewMap(L"Central Topic");
        if (root && global::G_TREEVIEW) {
            RecursivelyCreateNodeUI(root, TVI_ROOT);
            for (auto fn : freeNodes) {
                RecursivelyCreateNodeUI(fn, TVI_ROOT);
            }
        }
    }

    void SaveNodeRecursiveToStream(std::wstringstream& ss, MindNode* node) {
        auto Escape = [](const std::wstring& in) -> std::wstring {
            std::wstring out;
            out.reserve(in.size());
            for (wchar_t c : in) {
                switch (c) {
                case L'\\': out += L"\\\\"; break;
                case L'\t': out += L"\\t"; break;
                case L'\n': out += L"\\n"; break;
                case L'\r': out += L"\\r"; break;
                default: out.push_back(c); break;
                }
            }
            return out;
        };

        int parentId = (node->parent) ? node->parent->id : 0;

        ss << node->id << L"\t" << parentId
           << L"\t" << Escape(node->text)
           << L"\t" << Escape(node->label)
           << L"\t" << Escape(node->note)
           << L"\t" << Escape(node->outlinkFile)
           << L"\t" << Escape(node->outlinkPage)
           << L"\t" << node->outlinkTopicId
           << L"\t" << Escape(node->paintPath)
           << L"\t" << Escape(node->summary)
           << L"\t" << Escape(node->frameStyle)
           << L"\t" << Escape(node->outlinkURL)
           << L"\t" << Escape(node->annotation)
           << L"\t";

        for (size_t i = 0; i < node->linkedNodeIds.size(); ++i) {
            ss << node->linkedNodeIds[i];
            if (i + 1 < node->linkedNodeIds.size()) ss << L",";
        }
        ss << std::endl;

        for (auto child : node->children) {
            SaveNodeRecursiveToStream(ss, child);
        }
    }

    // Page Management
    struct PageInfo {
        MindNode* root;
        std::vector<MindNode*> freeNodes;
        std::wstring title;
        std::wstring filePath;
        int nextId;
    };

    void AddNewPage() {
        // Sync current
        if (currentPageIndex >= 0 && currentPageIndex < pages.size()) {
            pages[currentPageIndex].root = root;
            pages[currentPageIndex].freeNodes = freeNodes;
            pages[currentPageIndex].nextId = nextId;
        }
        
        PageInfo page;
        page.nextId = 1;
        page.root = new MindNode(page.nextId++, L"Central Topic");
        page.title = L"Page " + std::to_wstring(pages.size() + 1);
        page.filePath = L"";
        pages.push_back(page);
        
        SwitchToPage(pages.size() - 1);
    }

    void SwitchToPage(int index) {
        if (index < 0 || index >= pages.size()) return;
        
        // Sync current before switching
        if (currentPageIndex >= 0 && currentPageIndex < pages.size()) {
            pages[currentPageIndex].root = root;
            pages[currentPageIndex].freeNodes = freeNodes;
            pages[currentPageIndex].nextId = nextId;
        }
        
        currentPageIndex = index;
        root = pages[index].root;
        freeNodes = pages[index].freeNodes;
        nextId = pages[index].nextId;
        
        // Update UI
        if (global::G_TREEVIEW) {
            TreeView_DeleteAllItems(global::G_TREEVIEW);
            nodeMap.clear();
            if (root) {
                RecursivelyCreateNodeUI(root, TVI_ROOT);
            }
            for (auto fn : freeNodes) {
                RecursivelyCreateNodeUI(fn, TVI_ROOT);
            }
        }
        
        // Update Global File Path
        global::currentFilePath = pages[index].filePath;
        
        // Update Window Title
        std::wstring title = L"MINDTREE";
        if (!global::currentFilePath.empty()) {
            title += L" - " + global::currentFilePath;
        } else {
            title += L" - " + pages[index].title;
        }
        if (global::HOME) SetWindowText(global::HOME, title.c_str());

        // Reset operation history when changing pages (history is per active document/page).
        HistorySystem::Instance().ResetToSnapshot(CreateSnapshot());
    }
    
    int GetPageCount() const { return pages.size(); }
    
    std::wstring GetPageTitle(int index) {
        if (index >= 0 && index < pages.size()) {
            return pages[index].title;
        }
        return L"";
    }

    void UpdateCurrentPageTitle(const std::wstring& title) {
        if (currentPageIndex >= 0 && currentPageIndex < pages.size()) {
            pages[currentPageIndex].title = title;
        }
    }
    
    void UpdateCurrentPageFilePath(const std::wstring& path) {
        if (currentPageIndex >= 0 && currentPageIndex < pages.size()) {
            pages[currentPageIndex].filePath = path;
        }
    }

    // Search and Replace
    MindNode* FindNextNode(const std::wstring& text, MindNode* startNode, bool matchCase = false) {
        if (!root) return nullptr;
        
        // Flatten tree to list for easier linear search
        std::vector<MindNode*> allNodes;
        FlattenTree(root, allNodes);
        for (auto fn : freeNodes) {
            FlattenTree(fn, allNodes);
        }
        
        // Find start index
        size_t startIndex = 0;
        if (startNode) {
            for (size_t i = 0; i < allNodes.size(); ++i) {
                if (allNodes[i] == startNode) {
                    startIndex = i + 1; // Start from next
                    break;
                }
            }
        }
        
        // Search from start to end
        for (size_t i = startIndex; i < allNodes.size(); ++i) {
            if (ContainsText(allNodes[i]->text, text, matchCase)) {
                return allNodes[i];
            }
        }
        
        // Wrap around: Search from 0 to startIndex
        for (size_t i = 0; i < startIndex; ++i) {
            if (ContainsText(allNodes[i]->text, text, matchCase)) {
                return allNodes[i];
            }
        }
        
        return nullptr;
    }
    
    int ReplaceAll(const std::wstring& findText, const std::wstring& replaceText, bool matchCase = false) {
        if (!root) return 0;
        std::vector<MindNode*> allNodes;
        FlattenTree(root, allNodes);
        for (auto fn : freeNodes) {
            FlattenTree(fn, allNodes);
        }
        
        int count = 0;
        for (MindNode* node : allNodes) {
            if (node->text == findText) { // Exact match for replace all usually? Or substring? 
                // Standard Replace All usually replaces substrings.
                // Let's do substring replace.
                size_t pos = 0;
                bool modified = false;
                std::wstring newStr = node->text;
                
                // Simple replace loop
                while ((pos = (matchCase ? newStr.find(findText, pos) : FindCaseInsensitive(newStr, findText, pos))) != std::wstring::npos) {
                    newStr.replace(pos, findText.length(), replaceText);
                    pos += replaceText.length();
                    modified = true;
                }
                
                if (modified) {
                    node->text = newStr;
                    // Update UI
                    TVITEM tvi;
                    tvi.hItem = node->hTreeItem;
                    tvi.mask = TVIF_TEXT;
                    tvi.pszText = (LPWSTR)node->text.c_str();
                    TreeView_SetItem(global::G_TREEVIEW, &tvi);
                    count++;
                }
            } else if (ContainsText(node->text, findText, matchCase)) {
                 // Substring match logic
                size_t pos = 0;
                bool modified = false;
                std::wstring newStr = node->text;
                
                while (true) {
                    size_t foundPos = matchCase ? newStr.find(findText, pos) : FindCaseInsensitive(newStr, findText, pos);
                    if (foundPos == std::wstring::npos) break;
                    
                    newStr.replace(foundPos, findText.length(), replaceText);
                    pos = foundPos + replaceText.length();
                    modified = true;
                }
                
                if (modified) {
                    node->text = newStr;
                    TVITEM tvi;
                    tvi.hItem = node->hTreeItem;
                    tvi.mask = TVIF_TEXT;
                    tvi.pszText = (LPWSTR)node->text.c_str();
                    TreeView_SetItem(global::G_TREEVIEW, &tvi);
                    count++;
                }
            }
        }
        return count;
    }

private:
    void FlattenTree(MindNode* node, std::vector<MindNode*>& list) {
        if (!node) return;
        list.push_back(node);
        for (auto child : node->children) {
            FlattenTree(child, list);
        }
    }
    
    bool ContainsText(const std::wstring& source, const std::wstring& find, bool matchCase) {
        if (matchCase) {
            return source.find(find) != std::wstring::npos;
        } else {
            return FindCaseInsensitive(source, find, 0) != std::wstring::npos;
        }
    }
    
    size_t FindCaseInsensitive(const std::wstring& source, const std::wstring& find, size_t pos) {
        auto it = std::search(
            source.begin() + pos, source.end(),
            find.begin(), find.end(),
            [](wchar_t ch1, wchar_t ch2) {
                return towupper(ch1) == towupper(ch2);
            }
        );
        if (it != source.end()) return it - source.begin();
        return std::wstring::npos;
    }

    MindMapManager() : currentPageIndex(0) {
        PageInfo page;
        page.nextId = 1;
        page.root = new MindNode(page.nextId++, L"Central Topic");
        page.title = L"Page 1";
        page.filePath = L"";
        pages.push_back(page);
        
        root = pages[0].root;
        nextId = pages[0].nextId;
    }
    
    std::vector<PageInfo> pages;
    int currentPageIndex;

    MindNode* root;
    std::vector<MindNode*> freeNodes;
    int nextId;
    std::map<HTREEITEM, MindNode*> nodeMap;

    std::set<MindNode*> selectedNodes;
    std::vector<MindFrame> frames;
};
