// Common helpers for the DFDRepairMod plugins (header-only).
//
// The rule: no address in the game's DLLs is hard-coded; each is found at load time - by byte patterns of the code
// (Image::find) and by the RTTI names of classes (Image::vtableOf). Every write checks the original bytes / the
// start of the function it replaces. Anything not found or not matching -> nothing is patched, and the log says why.
#pragma once

#include <windows.h>

#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <string>
#include <vector>

namespace dfd {

// ---------------------------------------------------------------- the log

class Log {
public:
    // The file <this DLL's folder>\<name>.log, overwritten at every game start.
    void open(HMODULE self, const wchar_t* name) {
        wchar_t path[MAX_PATH];
        DWORD n = GetModuleFileNameW(self, path, MAX_PATH);
        while (n && path[n - 1] != L'\\') --n;
        path[n] = 0;
        wcsncat(path, name, MAX_PATH - wcslen(path) - 1);
        wcsncat(path, L".log", MAX_PATH - wcslen(path) - 1);
        f_ = _wfopen(path, L"w");
    }
    // Write a printf-style line with an HH:MM:SS time stamp. Flushed to disk at once, so the log survives a game
    // crash. If the file did not open - silently does nothing.
    void operator()(const char* fmt, ...) {
        if (!f_) return;
        time_t t = time(nullptr);
        tm lt{};
        localtime_s(&lt, &t);
        fprintf(f_, "%02d:%02d:%02d ", lt.tm_hour, lt.tm_min, lt.tm_sec);
        va_list ap;
        va_start(ap, fmt);
        vfprintf(f_, fmt, ap);
        va_end(ap);
        fputc('\n', f_);
        fflush(f_);
    }

private:
    FILE* f_ = nullptr;
};

// ---------------------------------------------------------------- the module

// A loaded PE image: its sections and searching them.
class Image {
public:
    // base - the loaded module's address (the same as its HMODULE): the PE headers are read from it.
    explicit Image(uintptr_t base) : base_(base) {
        auto dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
        nt_ = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
    }
    uintptr_t base() const { return base_; }
    // RVA - an address's offset from the module's start; addresses are written this way in the log and in
    // engine_notes.md.
    uint32_t rva(uintptr_t a) const { return static_cast<uint32_t>(a - base_); }
    uint32_t timeDateStamp() const { return nt_->FileHeader.TimeDateStamp; }
    uint32_t sizeOfImage() const { return nt_->OptionalHeader.SizeOfImage; }

    struct Range {
        uintptr_t begin = 0, end = 0;
    };
    // A section's bounds in memory (".text" - code, ".rdata" - constants and vtables, ".data" - data).
    // An empty range if there is no such section.
    Range section(const char* name) const {
        auto s = IMAGE_FIRST_SECTION(nt_);
        for (int i = 0; i < nt_->FileHeader.NumberOfSections; ++i, ++s)
            if (strncmp(reinterpret_cast<const char*>(s->Name), name, 8) == 0)
                return {base_ + s->VirtualAddress, base_ + s->VirtualAddress + s->Misc.VirtualSize};
        return {};
    }

    // A pattern "48 8B ?? 05": bytes in hex, ?? - any byte. Returns every match in the section.
    std::vector<uintptr_t> find(const char* pattern, const char* sect = ".text") const {
        std::vector<int> pat = parse(pattern);
        std::vector<uintptr_t> hits;
        Range r = section(sect);
        if (pat.empty() || r.end - r.begin < pat.size()) return hits;
        for (uintptr_t p = r.begin; p + pat.size() <= r.end; ++p)
            if (matchesAt(p, pat)) hits.push_back(p);
        return hits;
    }
    // Do the bytes at `at` match the pattern (the same format as find's)? This is how the start of a function is
    // checked before it is replaced, and an instruction at a known offset.
    static bool matches(uintptr_t at, const char* pattern) { return matchesAt(at, parse(pattern)); }

    // A string (with its terminating zero) in a section: the address of its first occurrence, or 0.
    uintptr_t findString(const char* s, const char* sect = ".rdata") const {
        Range r = section(sect);
        size_t n = strlen(s) + 1;
        for (uintptr_t p = r.begin; p + n <= r.end; ++p)
            if (memcmp(reinterpret_cast<const void*>(p), s, n) == 0) return p;
        return 0;
    }

    // The address a rip-relative operand refers to: disp32 at offset dispAt, the instruction `len` bytes long.
    static uintptr_t ripTarget(uintptr_t insn, size_t dispAt, size_t len) {
        return insn + len + *reinterpret_cast<const int32_t*>(insn + dispAt);
    }

    // A class's primary vtable by RTTI: ".?AV<cls>@@" -> TypeDescriptor -> CompleteObjectLocator
    // (offset 0) -> the slot before the vtable. cls - the full name with its namespace, e.g.
    // "AmmoRechargerFacet@Leonardo".
    // This finds the vtable's address in any build of the game, with no byte patterns. 0 - the class was not found.
    // Steps: 1) the type's name string in .data (the TypeDescriptor starts 0x10 bytes before it);
    // 2) in .rdata - the CompleteObjectLocator with this TypeDescriptor and offset 0 (the primary vtable);
    // 3) in .rdata - a pointer to that COL: the vtable starts right after it.
    uintptr_t vtableOf(const char* cls) const {
        std::string name = std::string(".?AV") + cls + "@@";
        Range data = section(".data"), rdata = section(".rdata");
        uintptr_t str = 0;
        for (uintptr_t p = data.begin; p + name.size() + 1 <= data.end; ++p)
            if (memcmp(reinterpret_cast<const void*>(p), name.c_str(), name.size() + 1) == 0) {
                str = p;
                break;
            }
        if (!str) return 0;
        uint32_t td = rva(str - 0x10);
        for (uintptr_t p = rdata.begin; p + 24 <= rdata.end; p += 4) {
            auto col = reinterpret_cast<const uint32_t*>(p);  // sig, offset, cdOffset, pTD, pCHD, pSelf
            if (col[0] == 1 && col[1] == 0 && col[3] == td && col[5] == rva(p)) {
                uint64_t ref = p;
                for (uintptr_t q = rdata.begin; q + 8 <= rdata.end; q += 8)
                    if (*reinterpret_cast<const uint64_t*>(q) == ref) return q + 8;
            }
        }
        return 0;
    }

private:
    // Parse a pattern's text "48 8B ?? 05" into a list of bytes; -1 - any byte (?? or ?).
    static std::vector<int> parse(const char* s) {
        std::vector<int> out;
        while (*s) {
            if (*s == ' ') {
                ++s;
            } else if (s[0] == '?') {
                out.push_back(-1);
                s += (s[1] == '?') ? 2 : 1;
            } else {
                out.push_back(static_cast<int>(strtoul(std::string(s, 2).c_str(), nullptr, 16)));
                s += 2;
            }
        }
        return out;
    }
    // Do the bytes at p match the parsed pattern (positions of -1 are not compared)?
    static bool matchesAt(uintptr_t p, const std::vector<int>& pat) {
        auto b = reinterpret_cast<const uint8_t*>(p);
        for (size_t i = 0; i < pat.size(); ++i)
            if (pat[i] >= 0 && b[i] != pat[i]) return false;
        return true;
    }

    uintptr_t base_;
    const IMAGE_NT_HEADERS64* nt_;
};

// ---------------------------------------------------------------- patches

// A write to protected memory (code or .rdata), the protection restored after it.
inline bool writeMem(void* dst, const void* src, size_t n) {
    DWORD old;
    if (!VirtualProtect(dst, n, PAGE_EXECUTE_READWRITE, &old)) return false;
    memcpy(dst, src, n);
    VirtualProtect(dst, n, old, &old);
    FlushInstructionCache(GetCurrentProcess(), dst, n);
    return true;
}

// A set of writes made all together, or not at all.
class PatchSet {
public:
    explicit PatchSet(const Image& img) : img_(img) {}

    // Replace bytes at an address: orig must match, or the whole set is cancelled.
    void bytes(const char* what, uintptr_t at, std::vector<uint8_t> orig, std::vector<uint8_t> repl) {
        items_.push_back({what, at, std::move(orig), std::move(repl), nullptr, nullptr, nullptr});
    }

    // Replace a vtable slot. The function now in the slot must start with the pattern prologue (a check that it is
    // the same function, with the same field layout). *saved gets the original pointer.
    template <class Fn>
    void vslot(const char* what, uintptr_t vtable, int slot, const char* prologue, Fn hook, Fn* saved) {
        uintptr_t at = vtable ? vtable + 8u * slot : 0;
        uintptr_t cur = at ? *reinterpret_cast<uintptr_t*>(at) : 0, repl = reinterpret_cast<uintptr_t>(hook);
        std::vector<uint8_t> o(8), r(8);
        memcpy(o.data(), &cur, 8);
        memcpy(r.data(), &repl, 8);
        items_.push_back({what, at, std::move(o), std::move(r), reinterpret_cast<void**>(saved),
                          reinterpret_cast<void*>(cur), prologue});
    }

    // Apply the set. First checks EVERY write: its address found, the original bytes in place, the functions to be
    // replaced starting as expected. A single mismatch - writes nothing and returns false (the reason is in the
    // log). Otherwise makes every write, fills *saved with the original pointers and returns true.
    bool apply(Log& log) {
        for (auto& it : items_) {
            if (!it.at) {
                log("not found: %s - nothing patched", it.what);
                return false;
            }
            bool ok = memcmp(reinterpret_cast<const void*>(it.at), it.orig.data(), it.orig.size()) == 0;
            if (ok && it.prologue)
                ok = it.savedValue && Image::matches(reinterpret_cast<uintptr_t>(it.savedValue), it.prologue);
            if (!ok) {
                log("verify FAILED: %s at RVA %08X - nothing patched", it.what, img_.rva(it.at));
                return false;
            }
        }
        for (auto& it : items_) {
            if (it.saved) *it.saved = it.savedValue;
            if (!writeMem(reinterpret_cast<void*>(it.at), it.repl.data(), it.repl.size())) {
                log("write FAILED: %s at RVA %08X (error %lu)", it.what, img_.rva(it.at), GetLastError());
                return false;  // the writes before it stay; in practice VirtualProtect does not fail
            }
            log("patched: %s (RVA %08X)", it.what, img_.rva(it.at));
        }
        return true;
    }

private:
    struct Item {
        const char* what;
        uintptr_t at;
        std::vector<uint8_t> orig, repl;
        void** saved;
        void* savedValue;
        const char* prologue;
    };
    const Image& img_;
    std::vector<Item> items_;
};

// ---------------------------------------------------------------- waiting for a DLL to load

// Calls cb(base) exactly once, as soon as the DLL is in the process: at once if it is already loaded, otherwise
// from the loader's notification (LdrRegisterDllNotification) - after it is mapped and its imports bound, but
// BEFORE its DllMain and any of its code. The callback runs under the loader lock: no LoadLibrary and no waiting
// for other threads in it.
class DllWatcher {
public:
    using Callback = void (*)(uintptr_t base);

    // Start waiting for the DLL named dllName (no path, any case). Already loaded - cb is called at once, from
    // here. false - could not subscribe to the Windows loader's notifications.
    bool watch(const wchar_t* dllName, Callback cb) {
        name_ = dllName;
        cb_ = cb;
        if (HMODULE h = GetModuleHandleW(dllName)) {
            fire(reinterpret_cast<uintptr_t>(h));
            return true;
        }
        auto reg = reinterpret_cast<LONG(NTAPI*)(ULONG, void*, void*, void**)>(
            GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "LdrRegisterDllNotification"));
        return reg && reg(0, reinterpret_cast<void*>(&notify), this, &cookie_) == 0;
    }

private:
    struct UnicodeString {
        USHORT Length, MaximumLength;
        PWSTR Buffer;
    };
    struct NotificationData {  // LDR_DLL_LOADED_NOTIFICATION_DATA
        ULONG Flags;
        const UnicodeString* FullDllName;
        const UnicodeString* BaseDllName;
        void* DllBase;
        ULONG SizeOfImage;
    };

    // The Windows loader's notification of any DLL loaded or unloaded. If it is ours that was loaded (reason 1) -
    // call the callback. ctx - our DllWatcher.
    static void NTAPI notify(ULONG reason, const NotificationData* data, void* ctx) {
        auto self = static_cast<DllWatcher*>(ctx);
        if (reason != 1 /* LOADED */ || self->fired_) return;
        const UnicodeString* n = data->BaseDllName;
        size_t len = wcslen(self->name_);
        if (n->Length / 2 == len && _wcsnicmp(n->Buffer, self->name_, len) == 0)
            self->fire(reinterpret_cast<uintptr_t>(data->DllBase));
    }

    // Call the callback exactly once in the object's lifetime.
    void fire(uintptr_t base) {
        if (fired_) return;
        fired_ = true;
        cb_(base);
    }

    const wchar_t* name_ = nullptr;
    Callback cb_ = nullptr;
    void* cookie_ = nullptr;
    bool fired_ = false;
};

}  // namespace dfd
