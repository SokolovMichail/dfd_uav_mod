// What vehicles spawn: the framework's columns (core/config.h) of the vehicle tables of the enabled mods.
//
// The game ignores columns it does not know, and they never reach its parsed vehicle descriptions; the vanilla
// tables do not have them. So instead of the game's table storage, the plugin reads the mods' own files:
//   - the mods enabled in the game's MODS menu: Documents\My Games\Terminator Dark Fate\mods\mods.json, in its order
//     ([{"enabled": true, "folder": "..."}]; the folder is the one in <game>\mods, any case);
//   - in each, every cars.xml / tanks.xml (basis\scripts\species, and the DLCs' dlc\<DLC>\basis\...), and every
//     cars.<x>.patch.xml / tanks.<x>.patch.xml (DefianceLoader's table merge, same columns);
//   - a vehicle in several mods takes the row of the last one in the list.
// Read once, on first use (the first F4 of any vehicle) - on the game thread, not under the loader lock.

#include <shlobj.h>

#include <algorithm>
#include <cstdio>
#include <cwctype>

#include "plugin.h"

namespace spawn {

namespace {

std::wstring g_gameDir;

std::string readFile(const std::wstring& path) {
    std::string s;
    FILE* f = _wfopen(path.c_str(), L"rb");
    if (!f) return s;
    char buf[65536];
    size_t n;
    while ((n = fread(buf, 1, sizeof buf, f)) > 0) s.append(buf, n);
    fclose(f);
    return s;
}

std::string utf8(const std::wstring& w) {
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string s(n > 0 ? n - 1 : 0, '\0');
    if (n > 1) WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, s.data(), n, nullptr, nullptr);
    return s;
}
std::wstring wide(const std::string& s) {
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    std::wstring w(n > 0 ? n - 1 : 0, L'\0');
    if (n > 1) MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, w.data(), n);
    return w;
}

// The enabled mods' folders, in the order of mods.json. A minimal reader for the file the game writes: objects
// with "enabled" and "folder", nothing nested.
std::vector<std::string> enabledMods(const std::wstring& modsJson) {
    std::string j = readFile(modsJson);
    std::vector<std::string> out;
    size_t pos = 0;
    while ((pos = j.find('{', pos)) != std::string::npos) {
        size_t end = j.find('}', pos);
        if (end == std::string::npos) break;
        std::string obj = j.substr(pos, end - pos);
        pos = end + 1;
        size_t f = obj.find("\"folder\"");
        size_t e = obj.find("\"enabled\"");
        if (f == std::string::npos || e == std::string::npos) continue;
        size_t colon = obj.find(':', e);
        if (colon == std::string::npos || obj.find("true", colon) != obj.find_first_not_of(" \t\r\n", colon + 1))
            continue;
        size_t q1 = obj.find('"', obj.find(':', f) + 1), q2 = q1 == std::string::npos ? q1 : obj.find('"', q1 + 1);
        if (q2 != std::string::npos) out.push_back(obj.substr(q1 + 1, q2 - q1 - 1));
    }
    return out;
}

// Is this a vehicle table of ours to read: cars.xml, tanks.xml, cars.<x>.patch.xml, tanks.<x>.patch.xml?
bool vehicleTable(std::wstring name) {
    std::transform(name.begin(), name.end(), name.begin(), [](wchar_t c) { return std::towlower(c); });
    for (const wchar_t* t : {L"cars", L"tanks"}) {
        std::wstring base = t;
        if (name == base + L".xml") return true;
        if (name.rfind(base + L".", 0) == 0 && name.size() > base.size() + 10 &&
            name.compare(name.size() - 10, 10, L".patch.xml") == 0)
            return true;
    }
    return false;
}

// Every vehicle table under dir, recursively, in name order (so the result does not depend on the disk).
void findTables(const std::wstring& dir, std::vector<std::wstring>& out) {
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW((dir + L"\\*").c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    std::vector<std::wstring> dirs, files;
    do {
        std::wstring n = fd.cFileName;
        if (n == L"." || n == L"..") continue;
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            dirs.push_back(n);
        else if (vehicleTable(n))
            files.push_back(n);
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    std::sort(dirs.begin(), dirs.end());
    std::sort(files.begin(), files.end());
    for (auto& f : files) out.push_back(dir + L"\\" + f);
    for (auto& d : dirs) findTables(dir + L"\\" + d, out);
}

// The user's Documents folder may be moved (OneDrive), so it is asked from the shell. shell32 and ole32 are loaded
// here, on first use, rather than imported: the DLL itself is loaded under the Windows loader lock.
std::wstring modsJsonPath() {
    using GetFolderFn = HRESULT(WINAPI*)(REFKNOWNFOLDERID, DWORD, HANDLE, PWSTR*);
    using FreeFn = void(WINAPI*)(LPVOID);
    HMODULE shell = LoadLibraryW(L"shell32.dll"), ole = LoadLibraryW(L"ole32.dll");
    auto getFolder = shell ? reinterpret_cast<GetFolderFn>(GetProcAddress(shell, "SHGetKnownFolderPath")) : nullptr;
    auto free = ole ? reinterpret_cast<FreeFn>(GetProcAddress(ole, "CoTaskMemFree")) : nullptr;
    PWSTR docs = nullptr;
    std::wstring path;
    if (getFolder && getFolder(FOLDERID_Documents, 0, nullptr, &docs) == S_OK)
        path = std::wstring(docs) + L"\\My Games\\Terminator Dark Fate\\mods\\mods.json";
    if (docs && free) free(docs);
    return path;
}

core::SpawnTable load() {
    core::SpawnTable t;
    std::wstring json = modsJsonPath();
    auto mods = enabledMods(json);
    g_log("tables: %zu enabled mod(s) in %s", mods.size(), utf8(json).c_str());
    for (const auto& m : mods) {
        std::vector<std::wstring> files;
        findTables(g_gameDir + L"\\mods\\" + wide(m), files);
        for (const auto& f : files) {
            core::SpawnTable rows;
            int n = core::readSpawnColumns(readFile(f), rows);
            if (!n) continue;
            for (auto& [name, c] : rows) {
                auto old = t.find(name);
                if (old != t.end() && (old->second.squad != c.squad || old->second.ammo != c.ammo ||
                                       old->second.cooldown != c.cooldown))
                    g_log("tables: '%s' is in more than one mod - the one from %s wins", name.c_str(), m.c_str());
                t[name] = c;
            }
            g_log("tables: %d spawning vehicle(s) in %s", n, utf8(f.substr(g_gameDir.size() + 1)).c_str());
        }
    }
    for (auto& [name, c] : t)
        g_log("  %s -> %s (kit %s, cooldown %.0f s)", name.c_str(), c.squad.c_str(),
              c.ammo.empty() ? "none" : c.ammo.c_str(), c.cooldown);
    if (t.empty()) g_log("tables: no spawning vehicles - is a mod made on the Spawn Framework enabled in MODS?");
    return t;
}

}  // namespace

void setGameDir(const std::wstring& gameDir) { g_gameDir = gameDir; }

std::wstring gameDirOf(HMODULE self) {
    wchar_t path[MAX_PATH];
    DWORD n = GetModuleFileNameW(self, path, MAX_PATH);
    std::wstring p(path, n);
    for (int i = 0; i < 3; ++i) {  // the DLL's name, plugins\, bin\ (or DefianceLoader\)
        size_t slash = p.find_last_of(L'\\');
        p.resize(slash == std::wstring::npos ? 0 : slash);
    }
    return p;
}

const core::SpawnTable& spawnTable() {
    static const core::SpawnTable t = load();
    return t;
}

}  // namespace spawn
