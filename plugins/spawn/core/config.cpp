// Reading the Spawn Framework's columns from a vehicle table. Overview - config.h.
//
// SpreadsheetML as the game's tables use it: <Table> of <Row>s of <Cell>s; a cell may skip ahead with
// ss:Index="n" (1-based), its value is the text of its <Data> (which may hold formatting tags); rows and cells may be
// self-closing. The header is the first row that has a "sysname" cell; rows whose sysname starts with '#' are
// comments, as in the game.

#include "config.h"

#include <algorithm>
#include <cstdlib>
#include <vector>

namespace spawn::core {

namespace {

// The text between > and < with XML entities decoded and tags dropped.
std::string textOf(const std::string& x, size_t begin, size_t end) {
    std::string out;
    for (size_t i = begin; i < end; ++i) {
        char c = x[i];
        if (c == '<') {  // a formatting tag inside <Data>
            size_t close = x.find('>', i);
            if (close == std::string::npos || close >= end) break;
            i = close;
            continue;
        }
        if (c == '&') {
            size_t semi = x.find(';', i);
            if (semi != std::string::npos && semi < end) {
                std::string e = x.substr(i + 1, semi - i - 1);
                const char* rep = e == "lt" ? "<" : e == "gt" ? ">" : e == "amp" ? "&" : e == "quot" ? "\"" : e == "apos" ? "'"
                                  : e == "#10" ? "\n" : e == "#13" ? "\r" : nullptr;
                if (rep) {
                    out += rep;
                    i = semi;
                    continue;
                }
            }
        }
        out += c;
    }
    return out;
}

// The value of attribute `name` ("ss:Index") inside a start tag x[begin, end), or "".
std::string attr(const std::string& x, size_t begin, size_t end, const char* name) {
    std::string key = std::string(name) + "=\"";
    size_t at = x.find(key, begin);
    if (at == std::string::npos || at >= end) return {};
    at += key.size();
    size_t close = x.find('"', at);
    return close == std::string::npos || close > end ? std::string() : x.substr(at, close - at);
}

// The cells of the row in x[begin, end), placed by ss:Index (missing cells are "").
std::vector<std::string> cellsOf(const std::string& x, size_t begin, size_t end) {
    std::vector<std::string> cells;
    size_t pos = begin, index = 0;
    for (;;) {
        size_t c = x.find("<Cell", pos);
        if (c == std::string::npos || c >= end) break;
        size_t tagEnd = x.find('>', c);
        if (tagEnd == std::string::npos || tagEnd >= end) break;
        std::string idx = attr(x, c, tagEnd, "ss:Index");
        if (!idx.empty()) index = static_cast<size_t>(std::max(1L, std::strtol(idx.c_str(), nullptr, 10))) - 1;
        std::string value;
        if (x[tagEnd - 1] == '/') {
            pos = tagEnd + 1;
        } else {
            size_t cellEnd = x.find("</Cell>", tagEnd);
            if (cellEnd == std::string::npos || cellEnd > end) break;
            size_t d = x.find("<Data", tagEnd);
            if (d != std::string::npos && d < cellEnd) {
                size_t dv = x.find('>', d);
                size_t de = x.find("</Data>", dv);
                if (dv != std::string::npos && de != std::string::npos && de <= cellEnd) value = textOf(x, dv + 1, de);
            }
            pos = cellEnd + 7;
        }
        if (cells.size() <= index) cells.resize(index + 1);
        cells[index] = value;
        ++index;
    }
    return cells;
}

int column(const std::vector<std::string>& header, const char* name) {
    for (size_t i = 0; i < header.size(); ++i)
        if (header[i] == name) return static_cast<int>(i);
    return -1;
}

std::string cell(const std::vector<std::string>& row, int col) {
    return col >= 0 && static_cast<size_t>(col) < row.size() ? row[col] : std::string();
}

}  // namespace

int readSpawnColumns(const std::string& x, SpawnTable& into) {
    size_t table = x.find("<Table"), tableEnd = x.find("</Table>");
    if (table == std::string::npos || tableEnd == std::string::npos) return 0;
    std::vector<std::string> header;
    int sysname = -1, squad = -1, ammo = -1, cooldown = -1, added = 0;
    int icon = -1, iconCooldown = -1, iconRecall = -1, tooltip = -1, tooltipRecall = -1;
    size_t pos = table;
    for (;;) {
        size_t r = x.find("<Row", pos);
        if (r == std::string::npos || r >= tableEnd) break;
        size_t tagEnd = x.find('>', r);
        if (tagEnd == std::string::npos) break;
        if (x[tagEnd - 1] == '/') {  // <Row/>: an empty row
            pos = tagEnd + 1;
            continue;
        }
        size_t rowEnd = x.find("</Row>", tagEnd);
        if (rowEnd == std::string::npos || rowEnd > tableEnd) break;
        pos = rowEnd + 6;
        std::vector<std::string> cells = cellsOf(x, tagEnd + 1, rowEnd);
        if (sysname < 0) {
            sysname = column(cells, "sysname");
            if (sysname < 0) continue;  // rows above the header
            header = cells;
            squad = column(header, "spawn_squad");
            ammo = column(header, "spawn_ammo");
            cooldown = column(header, "spawn_cooldown");
            icon = column(header, "spawn_icon");
            iconCooldown = column(header, "spawn_icon_cooldown");
            iconRecall = column(header, "spawn_icon_recall");
            tooltip = column(header, "spawn_tooltip");
            tooltipRecall = column(header, "spawn_tooltip_recall");
            if (squad < 0) return 0;  // not a table of ours
            continue;
        }
        std::string name = cell(cells, sysname), s = cell(cells, squad);
        if (name.empty() || name[0] == '#' || s.empty()) continue;
        SpawnConfig c;
        c.squad = s;
        c.ammo = cell(cells, ammo);
        c.icon = cell(cells, icon);
        c.iconCooldown = cell(cells, iconCooldown);
        c.iconRecall = cell(cells, iconRecall);
        c.tooltip = cell(cells, tooltip);
        c.tooltipRecall = cell(cells, tooltipRecall);
        std::string cd = cell(cells, cooldown);
        if (!cd.empty()) {
            char* stop = nullptr;
            float v = std::strtof(cd.c_str(), &stop);
            if (stop != cd.c_str() && v >= 0) c.cooldown = v;
        }
        into[name] = c;
        ++added;
    }
    return added;
}

}  // namespace spawn::core
