#include "history_system.h"

#include <windows.h>

#include <algorithm>
#include <cstring>

namespace {
    constexpr char kMagic[8] = { 'M','T','R','E','E','2','\r','\n' };
    constexpr std::uint32_t kVersion = 2;

    bool WriteAll(HANDLE hFile, const void* data, DWORD bytes) {
        const BYTE* p = static_cast<const BYTE*>(data);
        DWORD written = 0;
        while (written < bytes) {
            DWORD w = 0;
            if (!WriteFile(hFile, p + written, bytes - written, &w, nullptr)) {
                return false;
            }
            if (w == 0) return false;
            written += w;
        }
        return true;
    }

    bool ReadAll(HANDLE hFile, void* data, DWORD bytes) {
        BYTE* p = static_cast<BYTE*>(data);
        DWORD read = 0;
        while (read < bytes) {
            DWORD r = 0;
            if (!ReadFile(hFile, p + read, bytes - read, &r, nullptr)) {
                return false;
            }
            if (r == 0) return false;
            read += r;
        }
        return true;
    }

    bool ReadSome(HANDLE hFile, void* data, DWORD bytes, DWORD& outRead) {
        outRead = 0;
        return !!ReadFile(hFile, data, bytes, &outRead, nullptr);
    }

    bool WriteU32(HANDLE hFile, std::uint32_t v) {
        return WriteAll(hFile, &v, sizeof(v));
    }

    bool ReadU32(HANDLE hFile, std::uint32_t& v) {
        return ReadAll(hFile, &v, sizeof(v));
    }

    std::string WideToUtf8(const std::wstring& w) {
        if (w.empty()) return {};
        int bytes = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), nullptr, 0, nullptr, nullptr);
        if (bytes <= 0) return {};
        std::string out;
        out.resize(bytes);
        WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), &out[0], bytes, nullptr, nullptr);
        return out;
    }

    std::wstring Utf8ToWide(const std::string& s) {
        if (s.empty()) return {};
        int chars = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
        if (chars <= 0) return {};
        std::wstring out;
        out.resize(chars);
        MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), &out[0], chars);
        return out;
    }

    bool WriteBlob(HANDLE hFile, const std::wstring& w) {
        std::string utf8 = WideToUtf8(w);
        if (!WriteU32(hFile, (std::uint32_t)utf8.size())) return false;
        if (!utf8.empty()) {
            if (!WriteAll(hFile, utf8.data(), (DWORD)utf8.size())) return false;
        }
        return true;
    }

    bool ReadBlob(HANDLE hFile, std::wstring& out) {
        std::uint32_t len = 0;
        if (!ReadU32(hFile, len)) return false;
        if (len == 0) {
            out.clear();
            return true;
        }
        std::string buf;
        buf.resize(len);
        if (!ReadAll(hFile, &buf[0], len)) return false;
        out = Utf8ToWide(buf);
        return true;
    }
}

HistorySystem& HistorySystem::Instance() {
    static HistorySystem inst;
    return inst;
}

void HistorySystem::Clear() {
    snapshots.clear();
    cursor = 0;
}

void HistorySystem::ResetToSnapshot(const std::wstring& snapshot) {
    snapshots.clear();
    snapshots.push_back(snapshot);
    cursor = 0;
}

void HistorySystem::CommitSnapshotIfChanged(const std::wstring& snapshot) {
    if (snapshots.empty()) {
        ResetToSnapshot(snapshot);
        return;
    }

    if (snapshots[cursor] == snapshot) {
        return;
    }

    // Truncate redo tail if needed
    if (cursor + 1 < snapshots.size()) {
        snapshots.erase(snapshots.begin() + (cursor + 1), snapshots.end());
    }

    snapshots.push_back(snapshot);
    cursor = snapshots.size() - 1;
}

bool HistorySystem::CanUndo() const {
    return !snapshots.empty() && cursor > 0;
}

bool HistorySystem::CanRedo() const {
    return !snapshots.empty() && (cursor + 1) < snapshots.size();
}

bool HistorySystem::Undo(const std::function<void(const std::wstring&)>& restore) {
    if (!CanUndo()) return false;
    cursor--;
    restore(snapshots[cursor]);
    return true;
}

bool HistorySystem::Redo(const std::function<void(const std::wstring&)>& restore) {
    if (!CanRedo()) return false;
    cursor++;
    restore(snapshots[cursor]);
    return true;
}

const std::wstring& HistorySystem::GetCurrentSnapshot() const {
    static const std::wstring empty;
    if (snapshots.empty()) return empty;
    return snapshots[cursor];
}

std::size_t HistorySystem::GetCursor() const {
    return cursor;
}

const std::vector<std::wstring>& HistorySystem::GetSnapshots() const {
    return snapshots;
}

void HistorySystem::SetStateOrReset(std::vector<std::wstring> newSnapshots, std::size_t newCursor, const std::wstring& fallbackSnapshot) {
    if (newSnapshots.empty()) {
        ResetToSnapshot(fallbackSnapshot);
        return;
    }
    if (newCursor >= newSnapshots.size()) {
        ResetToSnapshot(fallbackSnapshot);
        return;
    }
    snapshots = std::move(newSnapshots);
    cursor = newCursor;
}

bool HistorySystem::LooksLikeCombinedFile(const std::wstring& filePath) {
    HANDLE hFile = CreateFileW(filePath.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return false;

    char buf[8] = {};
    DWORD r = 0;
    bool ok = ReadSome(hFile, buf, sizeof(buf), r);
    CloseHandle(hFile);

    if (!ok || r != sizeof(buf)) return false;
    return memcmp(buf, kMagic, sizeof(kMagic)) == 0;
}

bool HistorySystem::SaveCombinedFile(const std::wstring& filePath,
    const std::wstring& currentMapSnapshot,
    const std::vector<std::wstring>& snapshots,
    std::size_t cursor) {

    HANDLE hFile = CreateFileW(filePath.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return false;

    bool ok = true;

    ok = ok && WriteAll(hFile, kMagic, sizeof(kMagic));
    ok = ok && WriteU32(hFile, kVersion);

    // Write cursor + count up front
    ok = ok && WriteU32(hFile, (std::uint32_t)cursor);
    ok = ok && WriteU32(hFile, (std::uint32_t)snapshots.size());

    // Write current map blob
    ok = ok && WriteBlob(hFile, currentMapSnapshot);

    // Write snapshots
    for (const auto& s : snapshots) {
        ok = ok && WriteBlob(hFile, s);
        if (!ok) break;
    }

    CloseHandle(hFile);
    return ok;
}

bool HistorySystem::LoadCombinedFile(const std::wstring& filePath,
    std::wstring& outCurrentMapSnapshot,
    std::vector<std::wstring>& outSnapshots,
    std::size_t& outCursor) {

    HANDLE hFile = CreateFileW(filePath.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return false;

    bool ok = true;

    char magic[8];
    ok = ok && ReadAll(hFile, magic, sizeof(magic));
    if (!ok || memcmp(magic, kMagic, sizeof(kMagic)) != 0) {
        CloseHandle(hFile);
        return false;
    }

    std::uint32_t version = 0;
    ok = ok && ReadU32(hFile, version);
    if (!ok || version != kVersion) {
        CloseHandle(hFile);
        return false;
    }

    std::uint32_t cursor = 0;
    std::uint32_t count = 0;
    ok = ok && ReadU32(hFile, cursor);
    ok = ok && ReadU32(hFile, count);

    std::wstring current;
    ok = ok && ReadBlob(hFile, current);

    std::vector<std::wstring> snaps;
    snaps.reserve(count);
    for (std::uint32_t i = 0; i < count; ++i) {
        std::wstring s;
        ok = ok && ReadBlob(hFile, s);
        if (!ok) break;
        snaps.push_back(std::move(s));
    }

    CloseHandle(hFile);
    if (!ok) return false;

    outCurrentMapSnapshot = std::move(current);
    outSnapshots = std::move(snaps);
    outCursor = (std::size_t)cursor;
    return true;
}
