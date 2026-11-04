// Which tables the Spawn Framework reads. Overview - mods.h.

#include "mods.h"

#include <algorithm>
#include <cctype>

namespace spawn::core {

std::vector<std::string> enabledMods(const std::string& j) {
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
        // "enabled": the first thing after its colon must be true
        size_t colon = obj.find(':', e);
        if (colon == std::string::npos || obj.find("true", colon) != obj.find_first_not_of(" \t\r\n", colon + 1))
            continue;
        // "folder": the string after its colon
        size_t fColon = obj.find(':', f);
        size_t q1 = fColon == std::string::npos ? fColon : obj.find('"', fColon + 1);
        size_t q2 = q1 == std::string::npos ? q1 : obj.find('"', q1 + 1);
        if (q2 != std::string::npos) out.push_back(obj.substr(q1 + 1, q2 - q1 - 1));
    }
    return out;
}

bool isVehicleTable(const std::string& fileName) {
    std::string name = fileName;
    std::transform(name.begin(), name.end(), name.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    const std::string patch = ".patch.xml";
    for (const std::string base : {"cars", "tanks"}) {
        if (name == base + ".xml") return true;
        // <base>.<x>.patch.xml, <x> not empty
        if (name.rfind(base + ".", 0) == 0 && name.size() > base.size() + 1 + patch.size() &&
            name.compare(name.size() - patch.size(), patch.size(), patch) == 0)
            return true;
    }
    return false;
}

std::vector<std::string> mergeRows(SpawnTable& into, const SpawnTable& rows) {
    std::vector<std::string> clashes;
    for (const auto& [name, c] : rows) {
        auto old = into.find(name);
        if (old != into.end() && !(old->second == c)) clashes.push_back(name);
        into[name] = c;
    }
    return clashes;
}

}  // namespace spawn::core
