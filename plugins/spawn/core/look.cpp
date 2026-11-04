// How a vehicle's F4 button looks. Overview - look.h.

#include "look.h"

namespace spawn::core {

namespace {

std::string orDefault(const std::string& value, const char* fallback) { return value.empty() ? fallback : value; }

// A tooltip key of the row: empty - the default; too long to write in place - the default too, noted in `tooLong`.
std::string tipOf(const std::string& key, const char* fallback, std::string& tooLong) {
    if (key.empty()) return fallback;
    if (key.size() <= kMaxTip) return key;
    if (tooLong.empty()) tooLong = key;
    return fallback;
}

}  // namespace

Look lookOf(const SpawnConfig* c) {
    Look l;
    if (!c) {
        l.tip = kGameTip;
        l.tipRecall = kGameTipRecall;
        return l;
    }
    l.icon = orDefault(c->icon, kDefaultIcon);
    l.iconCooldown = orDefault(c->iconCooldown, kDefaultIconCooldown);
    l.iconRecall = orDefault(c->iconRecall, kDefaultIconRecall);
    l.tip = tipOf(c->tooltip, kDefaultTip, l.tooLong);
    l.tipRecall = tipOf(c->tooltipRecall, kDefaultTipRecall, l.tooLong);
    return l;
}

bool isFrameworkTip(const SpawnTable& table, const std::string& key) {
    if (key == kDefaultTip || key == kDefaultTipRecall) return true;
    for (const auto& [name, c] : table)
        if (c.tooltip == key || c.tooltipRecall == key) return true;
    return false;
}

}  // namespace spawn::core
