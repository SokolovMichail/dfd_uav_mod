// Tests of the game-agnostic core of the Spawn Framework (plugins/spawn/core): the rules of a vehicle that keeps one
// squad of its own while deployed, and reading the framework's columns from a vehicle table.
//
// The core talks to the game only through SpawnPort, so here the port is a plain test object: vehicles are entries
// in a map, squads are numbers that live until killed, and the port writes down what it was asked to do.
//
// This program is built without Windows headers (tests/CMakeLists.txt): if the core ever starts depending on Windows
// or on logic.dll, it stops compiling here.

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "third_party/doctest.h"

#include <map>
#include <set>
#include <string>
#include <vector>

#include "../plugins/spawn/core/spawner.h"

using namespace spawn::core;

namespace {

// ================================================================ test port

struct TestVehicle {
    const SpawnConfig* config = nullptr;
    int kits = 0;  // rounds of the kit ammo; -1 - does not carry it
};

class TestPort final : public SpawnPort {
public:
    std::map<Vehicle*, TestVehicle> vehicles;
    std::set<SquadId> living;
    std::vector<std::string> spawned, events;
    std::vector<SquadId> killed;
    std::vector<Vehicle*> packed;  // pack orders given
    SquadId next = 100;
    bool spawnFails = false;

    const SpawnConfig* config(Vehicle* v) override {
        auto it = vehicles.find(v);
        return it == vehicles.end() ? nullptr : it->second.config;
    }
    bool spendKit(Vehicle* v, const std::string&) override {
        int& k = vehicles[v].kits;
        if (k <= 0) return false;
        --k;
        return true;
    }
    SquadId spawn(Vehicle*, const std::string& squad) override {
        if (spawnFails) return 0;
        spawned.push_back(squad);
        living.insert(++next);
        return next;
    }
    bool alive(Vehicle*, SquadId s) override { return living.count(s) != 0; }
    void kill(Vehicle*, SquadId s) override {
        living.erase(s);
        killed.push_back(s);
    }
    void pack(Vehicle* v) override { packed.push_back(v); }
    void event(const char* what, Vehicle*, SquadId) override { events.push_back(what); }
    std::map<Vehicle*, Saved> saves;  // what a loaded game kept of a vehicle
    Saved saved(Vehicle* v) override {
        auto it = saves.find(v);
        return it == saves.end() ? Saved{} : it->second;
    }
};

Vehicle* handle(int n) { return reinterpret_cast<Vehicle*>(static_cast<uintptr_t>(n) * 16); }

const SpawnConfig kUav{"Res_patchwork_uav", "uav_kit", 60.0f};
const SpawnConfig kFree{"Lgn_swarm", "", 30.0f};

// A port with one UAV van (5 kits) at handle(1).
struct Fixture {
    TestPort port;
    Spawner spawner{port};
    Vehicle* van = handle(1);
    Fixture() { port.vehicles[van] = {&kUav, 5}; }
    int kits() { return port.vehicles[van].kits; }
    SquadId squad() { return spawner.state(van) ? spawner.state(van)->squad : 0; }
};

}  // namespace

// ================================================================ the rules

TEST_CASE("deploying spawns the squad at once and spends a kit") {
    Fixture f;
    f.spawner.onDeployed(f.van);
    CHECK(f.port.spawned == std::vector<std::string>{"Res_patchwork_uav"});
    CHECK(f.kits() == 4);
    CHECK(f.squad() != 0);
}

TEST_CASE("one squad at a time: ticks while it lives spawn nothing") {
    Fixture f;
    f.spawner.onDeployed(f.van);
    for (int i = 0; i < 100; ++i) f.spawner.onTick(f.van, 1.0f);
    CHECK(f.port.spawned.size() == 1);
    CHECK(f.kits() == 4);
}

TEST_CASE("packing up kills the squad and starts the cooldown") {
    Fixture f;
    f.spawner.onDeployed(f.van);
    SquadId s = f.squad();
    f.spawner.onPacked(f.van);
    CHECK(f.port.killed == std::vector<SquadId>{s});
    CHECK(f.squad() == 0);
    CHECK(f.spawner.state(f.van)->cooldown == doctest::Approx(60));
}

TEST_CASE("the cooldown runs while packed up; redeploying after it spawns again") {
    Fixture f;
    f.spawner.onDeployed(f.van);
    f.spawner.onPacked(f.van);
    f.spawner.onTick(f.van, 59.0f);
    f.spawner.onDeployed(f.van);
    CHECK(f.port.spawned.size() == 1);  // 1 s left
    f.spawner.onTick(f.van, 1.0f);
    CHECK(f.port.spawned.size() == 2);
    CHECK(f.kits() == 3);
}

TEST_CASE("the cooldown left is there while it runs, deployed or not") {
    Fixture f;
    CHECK(f.spawner.cooldownLeft(f.van) == 0);  // never deployed
    f.spawner.onDeployed(f.van);
    CHECK(f.spawner.cooldownLeft(f.van) == 0);  // squad out
    f.spawner.onPacked(f.van);
    CHECK(f.spawner.cooldownLeft(f.van) == doctest::Approx(60));
    f.spawner.onTick(f.van, 30.0f);
    f.spawner.onDeployed(f.van);  // deployed while waiting
    CHECK(f.spawner.cooldownLeft(f.van) == doctest::Approx(30));
    f.spawner.onTick(f.van, 30.0f);
    CHECK(f.spawner.cooldownLeft(f.van) == 0);
    CHECK(f.port.spawned.size() == 2);  // launched by itself when the wait was over
    CHECK(f.spawner.cooldownLeft(handle(9)) == 0);  // not ours
}

TEST_CASE("a squad lost in battle starts the cooldown at once") {
    Fixture f;
    f.spawner.onDeployed(f.van);
    f.port.living.clear();
    f.spawner.onTick(f.van, 0.1f);  // lost: pack-up ordered
    CHECK(f.spawner.cooldownLeft(f.van) == doctest::Approx(60));
}

TEST_CASE("a squad lost in battle packs its vehicle up and starts the cooldown") {
    Fixture f;
    f.spawner.onDeployed(f.van);
    f.port.living.clear();  // shot down
    f.spawner.onTick(f.van, 0.1f);
    CHECK(f.squad() == 0);
    CHECK(f.port.packed == std::vector<Vehicle*>{f.van});
    CHECK(f.spawner.state(f.van)->cooldown == doctest::Approx(60));
    // until the game ends F4 mode the vehicle spawns nothing, even after the cooldown
    f.spawner.onTick(f.van, 100.0f);
    CHECK(f.port.spawned.size() == 1);
    CHECK(f.port.packed.size() == 1);  // asked once
    f.spawner.onPacked(f.van);         // the game ends F4 mode: nothing left to kill
    CHECK(f.port.killed.empty());
    f.spawner.onDeployed(f.van);
    CHECK(f.port.spawned.size() == 2);
}

TEST_CASE("deployed again before the cooldown is over, the vehicle spawns when it is") {
    Fixture f;
    f.spawner.onDeployed(f.van);
    f.port.living.clear();
    f.spawner.onTick(f.van, 0.1f);  // lost: packs up
    f.spawner.onPacked(f.van);
    f.spawner.onTick(f.van, 20.0f);
    f.spawner.onDeployed(f.van);
    CHECK(f.port.spawned.size() == 1);
    f.spawner.onTick(f.van, 39.9f);
    CHECK(f.port.spawned.size() == 1);
    f.spawner.onTick(f.van, 0.2f);
    CHECK(f.port.spawned.size() == 2);
}

TEST_CASE("no kit - no squad, and no spawn until a kit turns up") {
    Fixture f;
    f.port.vehicles[f.van].kits = 0;
    f.spawner.onDeployed(f.van);
    f.spawner.onTick(f.van, 1.0f);
    CHECK(f.port.spawned.empty());
    CHECK(f.port.events.back() == std::string("no kit left"));
    f.port.vehicles[f.van].kits = 1;  // resupplied while deployed
    f.spawner.onTick(f.van, 0.1f);
    CHECK(f.port.spawned.size() == 1);
    CHECK(f.kits() == 0);
}

TEST_CASE("five kits make five squads, then nothing") {
    Fixture f;
    for (int i = 0; i < 7; ++i) {
        f.spawner.onDeployed(f.van);
        f.spawner.onPacked(f.van);
        f.spawner.onTick(f.van, 60.0f);
    }
    CHECK(f.port.spawned.size() == 5);
    CHECK(f.kits() == 0);
}

TEST_CASE("a vehicle without spawn_ammo spawns without kits") {
    TestPort port;
    Spawner spawner(port);
    Vehicle* v = handle(2);
    port.vehicles[v] = {&kFree, -1};
    spawner.onDeployed(v);
    CHECK(port.spawned == std::vector<std::string>{"Lgn_swarm"});
}

TEST_CASE("vehicles that are not ours are ignored") {
    Fixture f;
    Vehicle* other = handle(3);
    f.spawner.onDeployed(other);
    f.spawner.onTick(other, 1.0f);
    f.spawner.onPacked(other);
    CHECK(f.port.spawned.empty());
    CHECK(f.spawner.state(other) == nullptr);
}

TEST_CASE("a spawn that makes nothing waits the cooldown instead of spending kit after kit") {
    Fixture f;
    f.port.spawnFails = true;
    f.spawner.onDeployed(f.van);
    for (int i = 0; i < 10; ++i) f.spawner.onTick(f.van, 0.1f);
    CHECK(f.kits() == 4);
    CHECK(f.spawner.state(f.van)->cooldown > 0);
}

TEST_CASE("packing up after the squad died in battle kills nothing and keeps one cooldown") {
    Fixture f;
    f.spawner.onDeployed(f.van);
    f.port.living.clear();
    f.spawner.onTick(f.van, 10.0f);  // noticed: cooldown 60, pack-up ordered
    f.spawner.onPacked(f.van);
    CHECK(f.port.killed.empty());
    CHECK(f.spawner.state(f.van)->cooldown == doctest::Approx(60));
}

TEST_CASE("packing up on purpose orders nothing: the player's own pack-up is already under way") {
    Fixture f;
    f.spawner.onDeployed(f.van);
    f.spawner.onPacked(f.van);
    CHECK(f.port.packed.empty());
}

TEST_CASE("a destroyed vehicle's squad dies and the vehicle is forgotten") {
    Fixture f;
    f.spawner.onDeployed(f.van);
    SquadId s = f.squad();
    f.spawner.onRemoved(f.van);
    CHECK(f.port.killed == std::vector<SquadId>{s});
    CHECK(f.spawner.state(f.van) == nullptr);
    f.spawner.onTick(f.van, 100.0f);  // the handle reused by something else: nothing happens
    CHECK(f.port.spawned.size() == 1);
}

TEST_CASE("each vehicle has its own squad and cooldown") {
    Fixture f;
    Vehicle* second = handle(4);
    f.port.vehicles[second] = {&kUav, 5};
    f.spawner.onDeployed(f.van);
    f.spawner.onDeployed(second);
    CHECK(f.port.spawned.size() == 2);
    f.spawner.onPacked(f.van);
    CHECK(f.port.killed.size() == 1);
    CHECK(f.spawner.state(second)->squad != 0);
}

// ================================================================ saved games

TEST_CASE("a loaded vehicle takes back its saved squad instead of spawning a second one") {
    Fixture f;
    f.port.living.insert(40304);
    f.port.saves[f.van] = {40304, 0};
    f.spawner.onDeployed(f.van);
    CHECK(f.port.spawned.empty());
    CHECK(f.kits() == 5);
    CHECK(f.squad() == 40304);
    f.spawner.onPacked(f.van);  // and it is that squad that dies with the vehicle
    CHECK(f.port.killed == std::vector<SquadId>{40304});
}

TEST_CASE("a saved squad that is dead is not taken back: the vehicle spawns a new one") {
    Fixture f;
    f.port.saves[f.van] = {40304, 0};
    f.spawner.onDeployed(f.van);
    CHECK(f.port.spawned.size() == 1);
    CHECK(f.squad() != 40304);
}

TEST_CASE("a loaded vehicle keeps its saved cooldown") {
    Fixture f;
    f.port.saves[f.van] = {0, 42.0f};
    f.spawner.onDeployed(f.van);
    CHECK(f.port.spawned.empty());
    CHECK(f.spawner.cooldownLeft(f.van) == doctest::Approx(42));
    f.spawner.onTick(f.van, 42.0f);
    CHECK(f.port.spawned.size() == 1);
}

TEST_CASE("the saved game is read only when the rules first see a vehicle") {
    Fixture f;
    f.spawner.onDeployed(f.van);
    f.spawner.onPacked(f.van);
    f.spawner.onTick(f.van, 60.0f);
    f.port.living.insert(40304);
    f.port.saves[f.van] = {40304, 30.0f};  // what the game shows now is the rules' own
    f.spawner.onDeployed(f.van);
    CHECK(f.port.spawned.size() == 2);
    CHECK(f.squad() != 40304);
}

TEST_CASE("a map change forgets every vehicle and kills nothing") {
    Fixture f;
    f.spawner.onDeployed(f.van);
    f.spawner.onPacked(f.van);
    f.spawner.reset();
    CHECK(f.spawner.state(f.van) == nullptr);
    CHECK(f.spawner.cooldownLeft(f.van) == 0);
    CHECK(f.port.killed.size() == 1);  // the pack-up's only
}

// ================================================================ the table columns

namespace {

// A SpreadsheetML table as the game's are written: a title row above the header, sparse cells, a comment row.
const char* kTable = R"(<?xml version="1.0"?>
<Workbook xmlns="urn:schemas-microsoft-com:office:spreadsheet" xmlns:ss="urn:schemas-microsoft-com:office:spreadsheet">
 <Worksheet ss:Name="cars">
  <Table ss:ExpandedColumnCount="6">
   <Row><Cell><Data ss:Type="String">vehicles</Data></Cell></Row>
   <Row ss:Index="3"><Cell><Data ss:Type="String">sysname</Data></Cell><Cell><Data ss:Type="String">mass</Data></Cell><Cell ss:Index="4"><Data ss:Type="String">spawn_squad</Data></Cell><Cell><Data ss:Type="String">spawn_ammo</Data></Cell><Cell><Data ss:Type="String">spawn_cooldown</Data></Cell></Row>
   <Row><Cell><Data ss:Type="String">Res_van</Data></Cell><Cell><Data ss:Type="Number">2500</Data></Cell></Row>
   <Row><Cell><Data ss:Type="String">Res_patchwork_van</Data></Cell><Cell ss:Index="4"><Data ss:Type="String">Res_patchwork_uav</Data></Cell><Cell><Data ss:Type="String">uav_kit</Data></Cell><Cell><Data ss:Type="Number">45</Data></Cell></Row>
   <Row><Cell><Data ss:Type="String">Int_recon_van</Data></Cell><Cell ss:Index="4"><Data ss:Type="String"><Font>Int_recon_uav</Font></Data></Cell></Row>
   <Row><Cell><Data ss:Type="String">#Res_old_van</Data></Cell><Cell ss:Index="4"><Data ss:Type="String">Lgn_swarm</Data></Cell></Row>
   <Row/>
   <Row><Cell ss:Index="1"/><Cell><Data ss:Type="String">no sysname</Data></Cell></Row>
  </Table>
 </Worksheet>
</Workbook>
)";

}  // namespace

TEST_CASE("the framework's columns are read from a vehicle table") {
    SpawnTable t;
    CHECK(readSpawnColumns(kTable, t) == 2);
    REQUIRE(t.count("Res_patchwork_van") == 1);
    CHECK(t["Res_patchwork_van"].squad == "Res_patchwork_uav");
    CHECK(t["Res_patchwork_van"].ammo == "uav_kit");
    CHECK(t["Res_patchwork_van"].cooldown == doctest::Approx(45));
    REQUIRE(t.count("Int_recon_van") == 1);
    CHECK(t["Int_recon_van"].squad == "Int_recon_uav");  // formatting tags inside <Data> are dropped
    CHECK(t["Int_recon_van"].ammo.empty());
    CHECK(t["Int_recon_van"].cooldown == doctest::Approx(kDefaultCooldown));
    CHECK(t.count("Res_van") == 0);       // no spawn_squad
    CHECK(t.count("#Res_old_van") == 0);  // a comment row
    CHECK(t["Res_patchwork_van"].icon.empty());  // no look columns: the defaults
    CHECK(t["Res_patchwork_van"].tooltip.empty());
}

TEST_CASE("each vehicle's own F4 pictures and tooltips are read; empty cells leave the defaults") {
    const char* table = R"(<Workbook><Worksheet><Table>
   <Row><Cell><Data>sysname</Data></Cell><Cell><Data>spawn_squad</Data></Cell><Cell><Data>spawn_icon</Data></Cell><Cell><Data>spawn_icon_cooldown</Data></Cell><Cell><Data>spawn_icon_recall</Data></Cell><Cell><Data>spawn_tooltip</Data></Cell><Cell><Data>spawn_tooltip_recall</Data></Cell></Row>
   <Row><Cell><Data>Res_patchwork_van</Data></Cell><Cell><Data>Res_patchwork_uav</Data></Cell><Cell><Data>main\buttons2\pwuav_ready.dds</Data></Cell><Cell><Data>main\buttons2\pwuav_cooldown.dds</Data></Cell><Cell><Data>main\buttons2\pwuav_recall.dds</Data></Cell><Cell><Data>pwuav_deploy</Data></Cell><Cell><Data>pwuav_recall</Data></Cell></Row>
   <Row><Cell><Data>Mortar_truck</Data></Cell><Cell><Data>Res_mortar_team</Data></Cell><Cell ss:Index="6"><Data>mortar_deploy</Data></Cell></Row>
  </Table></Worksheet></Workbook>)";
    SpawnTable t;
    CHECK(readSpawnColumns(table, t) == 2);
    const SpawnConfig& uav = t["Res_patchwork_van"];
    CHECK(uav.icon == "main\\buttons2\\pwuav_ready.dds");
    CHECK(uav.iconCooldown == "main\\buttons2\\pwuav_cooldown.dds");
    CHECK(uav.iconRecall == "main\\buttons2\\pwuav_recall.dds");
    CHECK(uav.tooltip == "pwuav_deploy");
    CHECK(uav.tooltipRecall == "pwuav_recall");
    const SpawnConfig& mortar = t["Mortar_truck"];
    CHECK(mortar.icon.empty());
    CHECK(mortar.iconRecall.empty());
    CHECK(mortar.tooltip == "mortar_deploy");
    CHECK(mortar.tooltipRecall.empty());
}

TEST_CASE("a table without the framework's columns adds nothing") {
    SpawnTable t;
    CHECK(readSpawnColumns("<Table><Row><Cell><Data>sysname</Data></Cell><Cell><Data>mass</Data></Cell></Row>"
                           "<Row><Cell><Data>Res_van</Data></Cell></Row></Table>",
                           t) == 0);
    CHECK(t.empty());
    CHECK(readSpawnColumns("not a table", t) == 0);
}

TEST_CASE("a later table replaces a vehicle's row; entities are decoded") {
    SpawnTable t;
    const char* a = "<Table><Row><Cell><Data>sysname</Data></Cell><Cell><Data>spawn_squad</Data></Cell></Row>"
                    "<Row><Cell><Data>Van</Data></Cell><Cell><Data>A</Data></Cell></Row></Table>";
    const char* b = "<Table><Row><Cell><Data>sysname</Data></Cell><Cell><Data>spawn_squad</Data></Cell></Row>"
                    "<Row><Cell><Data>Van</Data></Cell><Cell><Data>B&amp;C</Data></Cell></Row></Table>";
    readSpawnColumns(a, t);
    readSpawnColumns(b, t);
    CHECK(t["Van"].squad == "B&C");
}
