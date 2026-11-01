// The Spawn Framework's part in game.dll: the vehicle's F4 button shows the cooldown.
//
// The command panel builds each ability's button with a class picked by the ability's id (game.dll 0x34cc0). Only
// TimerAbilityWindow - the buttons of SpawnSmoke, Rush, Hunting... - draws an ability's timer: while it runs, the
// button is greyed out (0x2b0e80(button, true)), a progress bar of its layout ("progress_bar") shows the part still
// to wait ([bar+0x1a8], then 0x2c5400(bar)), and the button is not ready (slot 33), so it cannot be pressed. F4 (Pack,
// ability 7) is a TwiceAbilityWindow - two buttons, "unpack" ([+0x130]) and "pack" ([+0x238]) - which does none of
// that. The logic.dll part (plugin.cpp) keeps a vehicle's cooldown in F4's own timer; here TwiceAbilityWindow's
// update (slot 7) and "ready" (slot 33) do for F4 what TimerAbilityWindow does for its ability:
//   * timer running: both buttons greyed out, the progress bar shown with the part still to wait, not ready;
//   * timer over: the buttons as before, the bar hidden -
//   * except on a vehicle of the framework ready to deploy: there the bar stays, with nothing to wait, so F4 shows
//     the bar's picture (the drones) instead of the unpack box. Its pack-up button and every other vehicle's F4 stay
//     as the game draws them.
// The progress bar comes with the mod's scripts/ui/AbilityButtons/pack.txt; without it F4 only greys out.
//
// A window: +0x120 its ability id, +0x128 its layout (children: a vector at +0x40, a child's name - std::string at
// +0x18), +0x138 the unit (reference block); a TwiceAbilityWindow's +0x240 - which button it shows (0 unpack, 1
// pack, 0x211e70). A widget: +0x5b shown, vfunc +0x48 show(bool).

#include <cstring>
#include <unordered_set>

#include "../logic_dll/plugin.h"

namespace spawn {

namespace {

template <class R, class... A>
R vcall(void* obj, size_t offset, A... args) {
    auto vtbl = *reinterpret_cast<void***>(obj);
    return reinterpret_cast<R (*)(void*, A...)>(vtbl[offset / 8])(obj, args...);
}
void* field(void* obj, size_t off) { return *reinterpret_cast<void**>(static_cast<char*>(obj) + off); }

struct MsvcString {
    union {
        char buf[16];
        const char* ptr;
    };
    size_t size, cap;
    const char* c_str() const { return cap > 15 ? ptr : buf; }
};

constexpr int kAbilityPack = 7;
constexpr size_t kWinAbility = 0x120, kWinLayout = 0x128, kWinButton = 0x130, kWinButton2 = 0x238, kWinUnit = 0x138;
constexpr size_t kWinShowing = 0x240;  // 0 - the unpack (deploy) button is shown, 1 - the pack-up one
constexpr size_t kLayoutChildren = 0x40, kChildName = 0x18, kWidgetShown = 0x5b, kWidgetShow = 0x48;
constexpr size_t kBarValue = 0x1a8;
constexpr size_t kGetAbilityTimer = 0xf0, kGetAbilityLength = 0xf8;  // AiFacet slots 30 / 31

using WindowFn = void (*)(void* window);
using ReadyFn = bool (*)(void* window);
using GreyFn = void (*)(void* button, bool grey);
using RefreshFn = void (*)(void* bar);
WindowFn g_origUpdate;
ReadyFn g_origReady;
GreyFn g_grey;
RefreshFn g_refresh;

std::unordered_set<void*> g_greyed;  // windows whose buttons we greyed out (only compared, never followed)

// F4's timer and its length for the window's unit (and the unit); false - not F4, or no unit / AI.
bool packTimer(void* window, float& left, float& length, void** unitOut = nullptr) {
    if (*reinterpret_cast<int*>(static_cast<char*>(window) + kWinAbility) != kAbilityPack) return false;
    void* rb = field(window, kWinUnit);
    void* unit = rb ? field(rb, 0x10) : nullptr;
    if (!unit) return false;
    if (unitOut) *unitOut = unit;
    void* ai = field(vcall<void*>(unit, 0xb0), 0x28);
    if (!ai) return false;
    left = vcall<float>(ai, kGetAbilityTimer, kAbilityPack);
    length = vcall<float>(ai, kGetAbilityLength, kAbilityPack);
    return true;
}

// The child of the window's layout called `name`; nullptr - none.
void* child(void* window, const char* name) {
    void* layout = field(window, kWinLayout);
    if (!layout) return nullptr;
    auto begin = static_cast<void**>(field(layout, kLayoutChildren));
    auto end = static_cast<void**>(field(layout, kLayoutChildren + 8));
    for (void** c = begin; c && c != end; ++c)
        if (*c && strcmp(reinterpret_cast<MsvcString*>(static_cast<char*>(*c) + kChildName)->c_str(), name) == 0)
            return *c;
    return nullptr;
}

void show(void* widget, bool on) {
    bool shown = *(static_cast<uint8_t*>(widget) + kWidgetShown) != 0;
    if (shown != on) vcall<void>(widget, kWidgetShow, on);
}

void greyButtons(void* window, bool grey) {
    for (size_t at : {kWinButton, kWinButton2})
        if (void* b = field(window, at)) g_grey(b, grey);
}

// Show the bar with `part` of it (0..1) still to wait.
void showBar(void* bar, float part) {
    show(bar, true);
    *reinterpret_cast<float*>(static_cast<char*>(bar) + kBarValue) = part < 1 ? part : 1;
    g_refresh(bar);
}

// TwiceAbilityWindow slot 7 - every frame: after the game has set the buttons, F4 shows the cooldown - and on a
// vehicle of the framework ready to deploy, the drones.
void hookUpdate(void* window) {
    g_origUpdate(window);
    guarded("panel", [&] {
        float left = 0, length = 0;
        void* unit = nullptr;
        if (!packTimer(window, left, length, &unit)) return;
        void* bar = child(window, "progress_bar");
        if (left > 0 && length > 0) {
            greyButtons(window, true);
            g_greyed.insert(window);
            if (bar) showBar(bar, left / length);
            return;
        }
        if (g_greyed.erase(window)) greyButtons(window, false);
        if (!bar) return;
        bool deployButton = *reinterpret_cast<int*>(static_cast<char*>(window) + kWinShowing) == 0;
        if (deployButton && spawnsSquads(unit))
            showBar(bar, 0);
        else
            show(bar, false);
    });
}

// TwiceAbilityWindow slot 33 - may the button be pressed: not while F4's timer runs.
bool hookReady(void* window) {
    bool ready = g_origReady(window);
    if (!ready) return ready;
    guarded("panel ready", [&] {
        float left = 0, length = 0;
        if (packTimer(window, left, length) && left > 0) ready = false;
    });
    return ready;
}

constexpr int kSlotUpdate = 7, kSlotReady = 33;
constexpr const char* kPrologueTwiceUpdate = "40 57 48 83 EC 20 48 8B F9 48 8B 89 38 01 00 00";
constexpr const char* kPrologueReady = "B0 01 C3";  // the base class's: always ready
// TimerAbilityWindow slot 7 (0x20e270): reads the ability's timer and length, greys its button out (call at +0x86)
// and refreshes its progress bar (call at +0xe5).
constexpr const char* kTimerUpdate =
    "48 89 5C 24 08 57 48 83 EC 50 48 8B D9 48 8B 89 38 01 00 00 48 85 C9 74 09 48 8B 49 10 48 85 C9 75 02 33 C9 48 "
    "8B 01 FF 90 B0 00 00 00 48 8B 78 28 48 85 FF 0F 84 E3 00 00 00 48 8B 07 48 8B CF 8B 93 20 01 00 00 0F 29 74 24 "
    "40 0F 29 7C 24 30 44 0F 29 44 24 20 FF 90 F0 00 00 00 48 8B 07 48 8B CF 8B 93 20 01 00 00 0F 28 F0 FF 90 F8 00 "
    "00 00 48 8B 8B 30 01 00 00 0F 57 FF 0F 2F F7 44 0F 28 C0 76 68 B2 01 E8 ?? ?? ?? ?? 48 8B 8B 38 02 00 00 48 85 "
    "C9 74 0E 80 79 5B 00 75 08 48 8B 01 B2 01 FF 50 48 48 8B 8B 38 02 00 00 F3 41 0F 5E F0 F3 0F 10 81 A8 01 00 00 "
    "F3 0F 5C C6 0F 54 05 ?? ?? ?? ?? 0F 2F 05 ?? ?? ?? ?? 72 40 0F 2F F7 72 0B F3 0F 5D 35 ?? ?? ?? ?? 0F 28 FE F3 "
    "0F 11 B9 A8 01 00 00 E8 ?? ?? ?? ??";
constexpr size_t kGreyCall = 0x86, kRefreshCall = 0xe5;
constexpr const char* kPrologueGrey = "40 53 48 83 EC 20 48 8B D9 88 91 84 02 00 00";
constexpr const char* kPrologueRefresh = "48 89 5C 24 18 48 89 74 24 20 55 57 41 56 48 8D";

}  // namespace

bool installPanel(const Image& img, Patcher& p) {
    g_log("game.dll at %p (TimeDateStamp %08X)", reinterpret_cast<void*>(img.base()), img.timeDateStamp());
    uintptr_t twice = img.vtableOf("TwiceAbilityWindow@Leonardo");
    uintptr_t timer = img.vtableOf("TimerAbilityWindow@Leonardo");
    if (timer) {
        uintptr_t update = *reinterpret_cast<uintptr_t*>(timer + 8 * kSlotUpdate);
        if (Image::matches(update, kTimerUpdate)) {
            g_grey = reinterpret_cast<GreyFn>(Image::ripTarget(update + kGreyCall, 1, 5));
            g_refresh = reinterpret_cast<RefreshFn>(Image::ripTarget(update + kRefreshCall, 1, 5));
            if (!Image::matches(reinterpret_cast<uintptr_t>(g_grey), kPrologueGrey)) g_grey = nullptr;
            if (!Image::matches(reinterpret_cast<uintptr_t>(g_refresh), kPrologueRefresh)) g_refresh = nullptr;
        }
    }
    g_log("panel: TwiceAbilityWindow %08X TimerAbilityWindow %08X, grey %08X refresh %08X", twice ? img.rva(twice) : 0,
          timer ? img.rva(timer) : 0, g_grey ? img.rva(reinterpret_cast<uintptr_t>(g_grey)) : 0,
          g_refresh ? img.rva(reinterpret_cast<uintptr_t>(g_refresh)) : 0);
    if (!twice || !g_grey || !g_refresh) {
        g_log("this game.dll build is not supported - F4 does not show the cooldown (spawning works)");
        return false;
    }
    p.vslot("TwiceAbilityWindow update (F4 cooldown)", twice, kSlotUpdate, kPrologueTwiceUpdate, &hookUpdate,
            &g_origUpdate);
    p.vslot("TwiceAbilityWindow ready (F4 cooldown)", twice, kSlotReady, kPrologueReady, &hookReady, &g_origReady);
    return p.apply();
}

}  // namespace spawn
