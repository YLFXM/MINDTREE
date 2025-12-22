#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
#include <functional>
#include <utility>

// Operation-log history implemented as an append-only list of full document snapshots.
// - Unlimited undo/redo (bounded only by memory/disk)
// - Persisted on Save via HistorySystem::SaveCombinedFile / LoadCombinedFile
//
// NOTE: This module intentionally does not depend on MindMapManager.
class HistorySystem {
public:
    static HistorySystem& Instance();

    void Clear();

    // Replace all history with a single snapshot and set cursor to 0.
    void ResetToSnapshot(const std::wstring& snapshot);

    // Commit a new "current" snapshot.
    // If the cursor is not at the end, truncates the redo tail.
    void CommitSnapshotIfChanged(const std::wstring& snapshot);

    bool CanUndo() const;
    bool CanRedo() const;

    // Performs undo/redo and calls restore(snapshot) if movement happens.
    bool Undo(const std::function<void(const std::wstring&)>& restore);
    bool Redo(const std::function<void(const std::wstring&)>& restore);

    const std::wstring& GetCurrentSnapshot() const;
    std::size_t GetCursor() const;
    const std::vector<std::wstring>& GetSnapshots() const;

    // Replace internal history state (used by Load).
    // If provided state is invalid, falls back to ResetToSnapshot(fallbackSnapshot).
    void SetStateOrReset(std::vector<std::wstring> snapshots, std::size_t cursor, const std::wstring& fallbackSnapshot);

    // RAII helper to record an operation: captures "before", then on destruction captures "after"
    // and commits if changed.
    class RecordScope {
    public:
        template <class CaptureFn>
        RecordScope(CaptureFn&& captureFn)
            : capture(std::forward<CaptureFn>(captureFn)) {
            if (capture) {
                before = capture();
                if (HistorySystem::Instance().GetSnapshots().empty()) {
                    HistorySystem::Instance().ResetToSnapshot(before);
                }
            }
        }

        ~RecordScope() {
            if (!capture) return;
            std::wstring after = capture();
            HistorySystem::Instance().CommitSnapshotIfChanged(after);
        }

        RecordScope(const RecordScope&) = delete;
        RecordScope& operator=(const RecordScope&) = delete;

    private:
        std::function<std::wstring()> capture;
        std::wstring before;
    };

    // Persist/restore: combined file = {header, current-map, snapshots[], cursor}
    // Uses UTF-8 for strings.
    static bool SaveCombinedFile(const std::wstring& filePath,
        const std::wstring& currentMapSnapshot,
        const std::vector<std::wstring>& snapshots,
        std::size_t cursor);

    static bool LoadCombinedFile(const std::wstring& filePath,
        std::wstring& outCurrentMapSnapshot,
        std::vector<std::wstring>& outSnapshots,
        std::size_t& outCursor);

    static bool LooksLikeCombinedFile(const std::wstring& filePath);

private:
    HistorySystem() = default;

    std::vector<std::wstring> snapshots;
    std::size_t cursor = 0;
};
