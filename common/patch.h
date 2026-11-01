// Общие помощники для плагинов DFDRepairMod (header-only).
//
// Правило: адреса в DLL игры не зашиваются, а ищутся при загрузке - по байтовым шаблонам кода
// (Image::find) и по RTTI-именам классов (Image::vtableOf). Каждая правка проверяет исходные байты /
// начало подменяемой функции. Не нашлось или не совпало хоть что-то -> не патчим ничего, пишем в лог.
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

// ---------------------------------------------------------------- лог

class Log {
public:
    // Файл <папка этой DLL>\<name>.log, перезаписывается при каждом запуске игры.
    void open(HMODULE self, const wchar_t* name) {
        wchar_t path[MAX_PATH];
        DWORD n = GetModuleFileNameW(self, path, MAX_PATH);
        while (n && path[n - 1] != L'\\') --n;
        path[n] = 0;
        wcsncat(path, name, MAX_PATH - wcslen(path) - 1);
        wcsncat(path, L".log", MAX_PATH - wcslen(path) - 1);
        f_ = _wfopen(path, L"w");
    }
    // Записать строку в стиле printf с отметкой времени ЧЧ:ММ:СС. Сразу сбрасывается на диск, чтобы лог
    // уцелел при падении игры. Если файл не открылся - молча ничего не делает.
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

// ---------------------------------------------------------------- модуль

// Загруженный PE-образ: секции и поиск по ним.
class Image {
public:
    // base - адрес загруженного модуля (то же, что HMODULE): по нему читаются заголовки PE.
    explicit Image(uintptr_t base) : base_(base) {
        auto dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
        nt_ = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
    }
    uintptr_t base() const { return base_; }
    // RVA - смещение адреса от начала модуля; в таком виде адреса пишутся в лог и в engine_notes.md.
    uint32_t rva(uintptr_t a) const { return static_cast<uint32_t>(a - base_); }
    uint32_t timeDateStamp() const { return nt_->FileHeader.TimeDateStamp; }
    uint32_t sizeOfImage() const { return nt_->OptionalHeader.SizeOfImage; }

    struct Range {
        uintptr_t begin = 0, end = 0;
    };
    // Границы секции (".text" - код, ".rdata" - константы и vtable, ".data" - данные) в памяти.
    // Пустой диапазон, если такой секции нет.
    Range section(const char* name) const {
        auto s = IMAGE_FIRST_SECTION(nt_);
        for (int i = 0; i < nt_->FileHeader.NumberOfSections; ++i, ++s)
            if (strncmp(reinterpret_cast<const char*>(s->Name), name, 8) == 0)
                return {base_ + s->VirtualAddress, base_ + s->VirtualAddress + s->Misc.VirtualSize};
        return {};
    }

    // Шаблон "48 8B ?? 05": байты в hex, ?? - любой байт. Возвращает все совпадения в секции.
    std::vector<uintptr_t> find(const char* pattern, const char* sect = ".text") const {
        std::vector<int> pat = parse(pattern);
        std::vector<uintptr_t> hits;
        Range r = section(sect);
        if (pat.empty() || r.end - r.begin < pat.size()) return hits;
        for (uintptr_t p = r.begin; p + pat.size() <= r.end; ++p)
            if (matchesAt(p, pat)) hits.push_back(p);
        return hits;
    }
    // Совпадают ли байты по адресу at с шаблоном (тот же формат, что у find). Так проверяется начало
    // функции перед подменой и инструкции по известному смещению.
    static bool matches(uintptr_t at, const char* pattern) { return matchesAt(at, parse(pattern)); }

    // Строка (с завершающим нулём) в секции: адрес первого вхождения или 0.
    uintptr_t findString(const char* s, const char* sect = ".rdata") const {
        Range r = section(sect);
        size_t n = strlen(s) + 1;
        for (uintptr_t p = r.begin; p + n <= r.end; ++p)
            if (memcmp(reinterpret_cast<const void*>(p), s, n) == 0) return p;
        return 0;
    }

    // Адрес, на который ссылается rip-относительный операнд: disp32 по смещению dispAt, длина инструкции len.
    static uintptr_t ripTarget(uintptr_t insn, size_t dispAt, size_t len) {
        return insn + len + *reinterpret_cast<const int32_t*>(insn + dispAt);
    }

    // Первичная vtable класса по RTTI: ".?AV<cls>@@" -> TypeDescriptor -> CompleteObjectLocator
    // (offset 0) -> слот перед vtable. cls - полное имя с пространством имён, напр. "AmmoRechargerFacet@Leonardo".
    // Так адрес vtable находится в любой сборке игры без шаблонов. 0 - класс не найден.
    // Шаги: 1) строка-имя типа в .data (TypeDescriptor начинается за 0x10 байт до неё);
    // 2) в .rdata - CompleteObjectLocator с этим TypeDescriptor и offset 0 (первичная vtable);
    // 3) в .rdata - указатель на этот COL: vtable начинается сразу за ним.
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
    // Разобрать текст шаблона "48 8B ?? 05" в список байтов; -1 - любой байт (?? или ?).
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
    // Совпадают ли байты по адресу p с разобранным шаблоном (позиции -1 не сравниваются).
    static bool matchesAt(uintptr_t p, const std::vector<int>& pat) {
        auto b = reinterpret_cast<const uint8_t*>(p);
        for (size_t i = 0; i < pat.size(); ++i)
            if (pat[i] >= 0 && b[i] != pat[i]) return false;
        return true;
    }

    uintptr_t base_;
    const IMAGE_NT_HEADERS64* nt_;
};

// ---------------------------------------------------------------- патчи

// Запись в защищённую память (код или .rdata) с восстановлением прав.
inline bool writeMem(void* dst, const void* src, size_t n) {
    DWORD old;
    if (!VirtualProtect(dst, n, PAGE_EXECUTE_READWRITE, &old)) return false;
    memcpy(dst, src, n);
    VirtualProtect(dst, n, old, &old);
    FlushInstructionCache(GetCurrentProcess(), dst, n);
    return true;
}

// Набор правок, который применяется целиком или не применяется вовсе.
class PatchSet {
public:
    explicit PatchSet(const Image& img) : img_(img) {}

    // Замена байтов по адресу: orig должен совпасть, иначе весь набор отменяется.
    void bytes(const char* what, uintptr_t at, std::vector<uint8_t> orig, std::vector<uint8_t> repl) {
        items_.push_back({what, at, std::move(orig), std::move(repl), nullptr, nullptr, nullptr});
    }

    // Подмена слота vtable. Функция в слоте должна начинаться с шаблона prologue (проверка, что
    // это та же функция и раскладка полей та же). *saved получит исходный указатель.
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

    // Применить набор. Сначала проверяет ВСЕ правки: адрес найден, исходные байты на месте, у подменяемых
    // функций ожидаемое начало. Хоть одно несовпадение - не пишет ничего и возвращает false (причина в
    // логе). Иначе пишет все правки, заполняет *saved исходными указателями и возвращает true.
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
                return false;  // предыдущие правки остаются; на практике VirtualProtect не падает
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

// ---------------------------------------------------------------- ожидание загрузки DLL

// Вызывает cb(base) ровно один раз, как только нужная DLL окажется в процессе:
// сразу, если она уже загружена, иначе из уведомления загрузчика (LdrRegisterDllNotification) -
// после отображения и связывания импорта, но ДО её DllMain и до любого её кода.
// Колбэк выполняется под loader lock: никаких LoadLibrary и ожиданий других потоков внутри.
class DllWatcher {
public:
    using Callback = void (*)(uintptr_t base);

    // Начать ожидание DLL с именем dllName (без пути, регистр не важен). Уже загружена - cb вызывается
    // сразу, отсюда. false - не удалось подписаться на уведомления загрузчика Windows.
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

    // Уведомление загрузчика Windows о загрузке/выгрузке любой DLL. Если загружена (reason 1) нужная -
    // вызвать колбэк. ctx - наш объект DllWatcher.
    static void NTAPI notify(ULONG reason, const NotificationData* data, void* ctx) {
        auto self = static_cast<DllWatcher*>(ctx);
        if (reason != 1 /* LOADED */ || self->fired_) return;
        const UnicodeString* n = data->BaseDllName;
        size_t len = wcslen(self->name_);
        if (n->Length / 2 == len && _wcsnicmp(n->Buffer, self->name_, len) == 0)
            self->fire(reinterpret_cast<uintptr_t>(data->DllBase));
    }

    // Вызвать колбэк ровно один раз за время жизни объекта.
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
