// The Spawn Framework's part in game.dll: the vehicle's F4 button shows the cooldown.
//
// The command panel builds each ability's button with a class picked by the ability's id (game.dll 0x34cc0). Only
// TimerAbilityWindow - the buttons of SpawnSmoke, Rush, Hunting... - draws an ability's timer: while it runs, the
// button is greyed out (0x2b0e80(button, true)), a progress bar of its layout ("progress_bar") shows the part still
// to wait ([bar+0x1a8], then 0x2c5400(bar)), and the button is not ready (slot 33), so it cannot be pressed. F4 (Pack,
// ability 7) is a TwiceAbilityWindow - two buttons, "unpack" ([+0x130]) and "pack" ([+0x238]) - which does none of
// that. The logic.dll part (logic_dll/port.h) keeps a vehicle's cooldown in F4's own timer; here TwiceAbilityWindow's
// update (slot 7) and "ready" (slot 33) do for F4 what TimerAbilityWindow does for its ability:
//   * timer running: both buttons greyed out, the progress bar shown with the part still to wait, not ready;
//   * timer over: the buttons as before, the bar hidden -
//   * except on a vehicle of the framework: there a bar stays, with nothing to wait, so F4 shows the bar's picture
//     instead of the game's - "progress_bar" on the deploy button, "progress_bar_2" on the pack-up one. The
//     pictures and the buttons' tooltips are the vehicle's own - its row's spawn_icon* and spawn_tooltip*, or the
//     framework's defaults (core/look.h makes the choice) - set on the widgets whenever another vehicle's F4 is
//     shown; the tooltip keys take the place of order_unpack and order_pack. Every other vehicle's F4 stays as the
//     game draws it.
// The progress bars come with the Spawn Framework mod's scripts/ui/AbilityButtons/pack.txt; without them F4 only
// greys out.
//
// A window: +0x120 its ability id, +0x128 its layout (children: a vector at +0x40, a child's name - std::string at
// +0x18), +0x138 the unit (reference block); a TwiceAbilityWindow's +0x240 - which button it shows (0 unpack, 1
// pack, 0x211e70). A widget: +0x5b shown, vfunc +0x48 show(bool), +0x80 its tooltip's key (std::string, the
// layout's "tip"; the tooltip is built from it each time it shows, 0x2cfa80).

#include <cstring>
#include <unordered_set>

#include "../core/look.h"
#include "../logic_dll/engine.h"
#include "../logic_dll/plugin.h"

namespace spawn {

namespace {

using namespace engine;

constexpr size_t kWinAbility = 0x120, kWinLayout = 0x128, kWinButton = 0x130, kWinButton2 = 0x238, kWinUnit = 0x138;
constexpr size_t kWinShowing = 0x240;  // 0 - the unpack (deploy) button is shown, 1 - the pack-up one
constexpr size_t kLayoutChildren = 0x40, kChildName = 0x18, kWidgetShown = 0x5b, kWidgetShow = 0x48;
constexpr size_t kBarValue = 0x1a8, kWidgetTip = 0x80;

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
    void* unit = objectOf(field(window, kWinUnit));
    if (!unit) return false;
    if (unitOut) *unitOut = unit;
    void* ai = aiOf(unit);
    if (!ai) return false;
    left = abilityTimer(ai, kAbilityPack);
    length = abilityLength(ai, kAbilityPack);
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

// ---- each vehicle's own look (core/look.h), put on the widgets

// A tooltip key of a vehicle's row too long to write in place: logged once.
void tellTooLong(const core::Look& look) {
    static std::unordered_set<std::string> told;
    if (!look.tooLong.empty() && told.insert(look.tooLong).second)
        g_log("tooltip key '%s' is longer than %zu characters - the default is shown", look.tooLong.c_str(),
              core::kMaxTip);
}

// Put `want` into a button's tooltip key - only over the game's own key or one of the framework's, and in the
// string itself (at most 15 characters: no memory of the game's to allocate or free).
void setTip(void* button, const char* gameKey, const std::string& want) {
    if (!button || want.size() > core::kMaxTip) return;
    auto tip = reinterpret_cast<MsvcString*>(static_cast<char*>(button) + kWidgetTip);
    if (tip->cap != 15 || tip->size > 15 || want == tip->buf) return;
    if (strcmp(tip->buf, gameKey) != 0 && !core::isFrameworkTip(spawnTable(), tip->buf)) return;
    memcpy(tip->buf, want.c_str(), want.size() + 1);
    tip->size = want.size();
}

// A picture widget's picture: the game's setter (0x2d6210, found in the layout loader) takes the path the widget
// keeps at +0x60 - "ui/pictures/" + the layout's name without its extension (0x2a9970) - and loads the texture.
using PictureFn = void (*)(void* widget, const MsvcString* path);
PictureFn g_setPicture;
constexpr size_t kWidgetPicture = 0x60;
constexpr size_t kBarProgressPicture = 0x1a0;  // a progress bar's child picture, the part still to wait (0x2c5590)

void setPicture(void* widget, const std::string& name) {
    if (!widget || !g_setPicture) return;
    std::string path = "ui/pictures/" + name.substr(0, name.rfind('.'));
    auto now = reinterpret_cast<const MsvcString*>(static_cast<char*>(widget) + kWidgetPicture);
    if (now->size == path.size() && memcmp(now->c_str(), path.data(), path.size()) == 0) return;
    MsvcString s = msvc(path);
    g_setPicture(widget, &s);
}

// The vehicle's pictures on F4's two bars: the deploy one - its icon, filled with its cooldown picture; the
// pack-up one - its pack-up picture (also under the bar's fill, which never shows: there is nothing to wait).
void setPictures(void* bar, void* bar2, const core::Look& look) {
    if (bar) {
        setPicture(bar, look.icon);
        setPicture(field(bar, kBarProgressPicture), look.iconCooldown);
    }
    if (bar2) {
        setPicture(bar2, look.iconRecall);
        setPicture(field(bar2, kBarProgressPicture), look.iconRecall);
    }
}

// TwiceAbilityWindow slot 7 - every frame: after the game has set the buttons, F4 shows the cooldown - and on a
// vehicle of the framework, its own pictures and tooltips.
void hookUpdate(void* window) {
    g_origUpdate(window);
    guarded("panel", [&] {
        float left = 0, length = 0;
        void* unit = nullptr;
        if (!packTimer(window, left, length, &unit)) return;
        const core::SpawnConfig* c = spawnConfigOf(unit);
        core::Look look = core::lookOf(c);
        tellTooLong(look);
        setTip(field(window, kWinButton), core::kGameTip, look.tip);
        setTip(field(window, kWinButton2), core::kGameTipRecall, look.tipRecall);
        void* bar = child(window, "progress_bar");
        void* bar2 = child(window, "progress_bar_2");
        if (c) setPictures(bar, bar2, look);
        if (left > 0 && length > 0) {
            greyButtons(window, true);
            g_greyed.insert(window);
            if (bar) showBar(bar, left / length);
            if (bar2) show(bar2, false);
            return;
        }
        if (g_greyed.erase(window)) greyButtons(window, false);
        bool deployButton = *reinterpret_cast<int*>(static_cast<char*>(window) + kWinShowing) == 0;
        if (bar) {
            if (c && deployButton)
                showBar(bar, 0);
            else
                show(bar, false);
        }
        if (bar2) {
            if (c && !deployButton)
                showBar(bar2, 0);
            else
                show(bar2, false);
        }
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
// The layout loader's "image" (0x2a9ef0): "the path of the layout's name (0x2a9970), set it on the widget" - the
// setter's call at +20.
constexpr const char* kSigPictureCall = "48 8D 55 EF 48 8D 4D 97 E8 ?? ?? ?? ?? 90 48 8B D0 48 8B CF E8 ?? ?? ?? ?? 90";
constexpr size_t kPictureCall = 20;
constexpr const char* kProloguePicture = "48 89 5C 24 10 48 89 74 24 18 48 89 7C 24 20 55 41 54 41 55 41 56 41 57";

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
    auto pictures = img.find(kSigPictureCall);
    if (pictures.size() == 1) {
        g_setPicture = reinterpret_cast<PictureFn>(Image::ripTarget(pictures[0] + kPictureCall, 1, 5));
        if (!Image::matches(reinterpret_cast<uintptr_t>(g_setPicture), kProloguePicture)) g_setPicture = nullptr;
    }
    g_log("panel: picture setter %08X (%zu calls)%s",
          g_setPicture ? img.rva(reinterpret_cast<uintptr_t>(g_setPicture)) : 0, pictures.size(),
          g_setPicture ? "" : " - not found: F4 shows the layout's pictures on every framework vehicle");
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
