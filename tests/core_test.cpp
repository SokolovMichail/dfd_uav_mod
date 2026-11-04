// Tests of the game-agnostic core of the Spawn Framework (plugins/spawn/core): the rules of a vehicle that keeps one
// squad of its own while deployed, reading the framework's columns from a vehicle table, which tables are read and
// how the mods' rows come together, and the choice of F4's pictures and tooltips.
//
// The core talks to the game only through SpawnPort, so here the port is a plain test object: vehicles are entries
// in a map, squads are numbers that live until killed, and the port writes down what it was asked to do.
//
// The core sources are built here with Windows headers switched off (tests/CMakeLists.txt, spawn_core_checked): if the
// core ever starts depending on Windows or on logic.dll, the tests stop compiling.

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "third_party/doctest.h"

#include <map>
#include <set>
#include <string>
#include <vector>

#include "../plugins/spawn/core/look.h"
#include "../plugins/spawn/core/mods.h"
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

SpawnConfig config(const char* squad, const char* ammo, float cooldown) {
    SpawnConfig c;
    c.squad = squad;
    c.ammo = ammo;
    c.cooldown = cooldown;
    return c;
}

const SpawnConfig kUav = config("Res_patchwork_uav", "uav_kit", 60.0f);
const SpawnConfig kFree = config("Lgn_swarm", "", 30.0f);

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

// ================================================================ untethered vehicles

namespace {

SpawnConfig troops(int count, bool tethered = false) {
    SpawnConfig c = config("Res_militia", "uav_kit", 40.0f);
    c.tethered = tethered;
    c.count = count;
    return c;
}

}  // namespace

TEST_CASE("an untethered vehicle launches spawn_count squads, a kit each, and the cooldown starts at once") {
    Fixture f;
    SpawnConfig c = troops(3);
    f.port.vehicles[f.van] = {&c, 5};
    f.spawner.onDeployed(f.van);
    CHECK(f.port.spawned.size() == 3);
    CHECK(f.kits() == 2);
    CHECK(f.squad() == 0);  // none of them is the vehicle's
    CHECK(f.spawner.cooldownLeft(f.van) == doctest::Approx(40));
}

TEST_CASE("an untethered vehicle launches as many squads as it has kits for") {
    Fixture f;
    SpawnConfig c = troops(3);
    f.port.vehicles[f.van] = {&c, 2};
    f.spawner.onDeployed(f.van);
    CHECK(f.port.spawned.size() == 2);
    CHECK(f.spawner.cooldownLeft(f.van) == doctest::Approx(40));
    f.port.vehicles[f.van].kits = 0;
    f.spawner.onPacked(f.van);
    f.spawner.onTick(f.van, 40.0f);
    f.spawner.onDeployed(f.van);  // no kit: nothing, and no cooldown
    CHECK(f.port.spawned.size() == 2);
    CHECK(f.spawner.cooldownLeft(f.van) == 0);
}

TEST_CASE("an untethered vehicle's squads are on their own: pack-up, loss and destruction change nothing") {
    Fixture f;
    SpawnConfig c = troops(2);
    f.port.vehicles[f.van] = {&c, 5};
    f.spawner.onDeployed(f.van);
    f.port.living.clear();  // both lost in battle
    for (int i = 0; i < 5; ++i) f.spawner.onTick(f.van, 1.0f);
    CHECK(f.port.packed.size() == 1);  // the launch's own pack-up, nothing for the loss
    f.spawner.onPacked(f.van);
    f.spawner.onRemoved(f.van);
    CHECK(f.port.killed.empty());
}

TEST_CASE("an untethered vehicle packs up by itself after its launch - on the next tick, not in the deploy") {
    Fixture f;
    SpawnConfig c = troops(1);
    f.port.vehicles[f.van] = {&c, 5};
    f.spawner.onDeployed(f.van);
    CHECK(f.port.packed.empty());  // the deploy runs inside F4 mode's own first tick
    f.spawner.onTick(f.van, 0.1f);
    CHECK(f.port.packed == std::vector<Vehicle*>{f.van});
    f.spawner.onTick(f.van, 0.1f);
    CHECK(f.port.packed.size() == 1);  // ordered once
    f.spawner.onPacked(f.van);
    f.spawner.onTick(f.van, 40.0f);
    f.spawner.onDeployed(f.van);  // deployed again after the cooldown: the next launch
    CHECK(f.port.spawned.size() == 2);
}

TEST_CASE("an untethered vehicle packs up even when there was nothing to launch") {
    Fixture f;
    SpawnConfig c = troops(1);
    f.port.vehicles[f.van] = {&c, 0};
    f.spawner.onDeployed(f.van);
    f.spawner.onTick(f.van, 0.1f);
    CHECK(f.port.spawned.empty());
    CHECK(f.port.packed.size() == 1);
}

TEST_CASE("a player's pack-up before the next tick leaves nothing to order") {
    Fixture f;
    SpawnConfig c = troops(1);
    f.port.vehicles[f.van] = {&c, 5};
    f.spawner.onDeployed(f.van);
    f.spawner.onPacked(f.van);
    f.spawner.onTick(f.van, 0.1f);
    CHECK(f.port.packed.empty());
}

TEST_CASE("a tethered vehicle keeps one squad, whatever its spawn_count") {
    Fixture f;
    SpawnConfig c = troops(4, true);
    f.port.vehicles[f.van] = {&c, 5};
    f.spawner.onDeployed(f.van);
    CHECK(f.port.spawned.size() == 1);
    CHECK(f.kits() == 4);
}

TEST_CASE("an untethered vehicle does not take a squad back from a saved game") {
    Fixture f;
    SpawnConfig c = troops(1);
    f.port.vehicles[f.van] = {&c, 5};
    f.port.living.insert(40304);
    f.port.saves[f.van] = {40304, 12.0f};
    f.spawner.onDeployed(f.van);
    CHECK(f.squad() == 0);
    CHECK(f.spawner.cooldownLeft(f.van) == doctest::Approx(12));  // its cooldown it does
    CHECK(f.port.spawned.empty());
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
   <Row><Cell><Data ss:Type="String">Res_patchwork_uav_carrier</Data></Cell><Cell ss:Index="4"><Data ss:Type="String">Res_patchwork_uav</Data></Cell><Cell><Data ss:Type="String">uav_kit</Data></Cell><Cell><Data ss:Type="Number">45</Data></Cell></Row>
   <Row><Cell><Data ss:Type="String">Int_uav_carrier</Data></Cell><Cell ss:Index="4"><Data ss:Type="String"><Font>Int_recon_uav</Font></Data></Cell></Row>
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
    REQUIRE(t.count("Res_patchwork_uav_carrier") == 1);
    CHECK(t["Res_patchwork_uav_carrier"].squad == "Res_patchwork_uav");
    CHECK(t["Res_patchwork_uav_carrier"].ammo == "uav_kit");
    CHECK(t["Res_patchwork_uav_carrier"].cooldown == doctest::Approx(45));
    REQUIRE(t.count("Int_uav_carrier") == 1);
    CHECK(t["Int_uav_carrier"].squad == "Int_recon_uav");  // formatting tags inside <Data> are dropped
    CHECK(t["Int_uav_carrier"].ammo.empty());
    CHECK(t["Int_uav_carrier"].cooldown == doctest::Approx(kDefaultCooldown));
    CHECK(t.count("Res_van") == 0);       // no spawn_squad
    CHECK(t.count("#Res_old_van") == 0);  // a comment row
    CHECK(t["Res_patchwork_uav_carrier"].icon.empty());  // no look columns: the defaults
    CHECK(t["Res_patchwork_uav_carrier"].tooltip.empty());
}

TEST_CASE("each vehicle's own F4 pictures and tooltips are read; empty cells leave the defaults") {
    const char* table = R"(<Workbook><Worksheet><Table>
   <Row><Cell><Data>sysname</Data></Cell><Cell><Data>spawn_squad</Data></Cell><Cell><Data>spawn_icon</Data></Cell><Cell><Data>spawn_icon_cooldown</Data></Cell><Cell><Data>spawn_icon_recall</Data></Cell><Cell><Data>spawn_tooltip</Data></Cell><Cell><Data>spawn_tooltip_recall</Data></Cell></Row>
   <Row><Cell><Data>Res_patchwork_uav_carrier</Data></Cell><Cell><Data>Res_patchwork_uav</Data></Cell><Cell><Data>main\buttons2\uavc_ready.dds</Data></Cell><Cell><Data>main\buttons2\uavc_cooldown.dds</Data></Cell><Cell><Data>main\buttons2\uavc_recall.dds</Data></Cell><Cell><Data>uavc_deploy</Data></Cell><Cell><Data>uavc_recall</Data></Cell></Row>
   <Row><Cell><Data>Mortar_truck</Data></Cell><Cell><Data>Res_mortar_team</Data></Cell><Cell ss:Index="6"><Data>mortar_deploy</Data></Cell></Row>
  </Table></Worksheet></Workbook>)";
    SpawnTable t;
    CHECK(readSpawnColumns(table, t) == 2);
    const SpawnConfig& uav = t["Res_patchwork_uav_carrier"];
    CHECK(uav.icon == "main\\buttons2\\uavc_ready.dds");
    CHECK(uav.iconCooldown == "main\\buttons2\\uavc_cooldown.dds");
    CHECK(uav.iconRecall == "main\\buttons2\\uavc_recall.dds");
    CHECK(uav.tooltip == "uavc_deploy");
    CHECK(uav.tooltipRecall == "uavc_recall");
    const SpawnConfig& mortar = t["Mortar_truck"];
    CHECK(mortar.icon.empty());
    CHECK(mortar.iconRecall.empty());
    CHECK(mortar.tooltip == "mortar_deploy");
    CHECK(mortar.tooltipRecall.empty());
}

TEST_CASE("spawn_tethered and spawn_count: any no is untethered; a bad count is 1; a big one is capped") {
    const char* table = R"(<Workbook><Worksheet><Table>
   <Row><Cell><Data>sysname</Data></Cell><Cell><Data>spawn_squad</Data></Cell><Cell><Data>spawn_tethered</Data></Cell><Cell><Data>spawn_count</Data></Cell></Row>
   <Row><Cell><Data>A</Data></Cell><Cell><Data>s</Data></Cell><Cell><Data>False</Data></Cell><Cell><Data>3</Data></Cell></Row>
   <Row><Cell><Data>B</Data></Cell><Cell><Data>s</Data></Cell><Cell><Data> no </Data></Cell><Cell><Data>0</Data></Cell></Row>
   <Row><Cell><Data>C</Data></Cell><Cell><Data>s</Data></Cell><Cell><Data>0</Data></Cell><Cell><Data>500</Data></Cell></Row>
   <Row><Cell><Data>D</Data></Cell><Cell><Data>s</Data></Cell><Cell><Data>true</Data></Cell><Cell><Data>two</Data></Cell></Row>
   <Row><Cell><Data>E</Data></Cell><Cell><Data>s</Data></Cell></Row>
  </Table></Worksheet></Workbook>)";
    SpawnTable t;
    CHECK(readSpawnColumns(table, t) == 5);
    CHECK(!t["A"].tethered);
    CHECK(t["A"].count == 3);
    CHECK(t["A"].unitsPerDeploy() == 3);
    CHECK(!t["B"].tethered);
    CHECK(t["B"].count == 1);
    CHECK(!t["C"].tethered);
    CHECK(t["C"].count == kMaxCount);
    CHECK(t["D"].tethered);
    CHECK(t["D"].count == 1);
    CHECK(t["E"].tethered);  // no cells: tethered, one
    CHECK(t["E"].unitsPerDeploy() == 1);
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

// ---------------------------------------------------------------- which tables, and the mods' rows (mods.h)

TEST_CASE("the enabled mods are read from mods.json in its order") {
    const char* json = R"([
        {"enabled": true, "folder": "Spawn_Framework"},
        {"folder": "HEMTT_Spares", "enabled": false},
        {"enabled":true,"folder":"UAV_Carriers"},
        {"enabled": true},
        {"folder": "No_Flag"}
    ])";
    CHECK(enabledMods(json) == std::vector<std::string>{"Spawn_Framework", "UAV_Carriers"});
    CHECK(enabledMods("").empty());
    CHECK(enabledMods("not json").empty());
}

TEST_CASE("vehicle tables: cars and tanks, and their table-merge patches, any case") {
    CHECK(isVehicleTable("cars.xml"));
    CHECK(isVehicleTable("Tanks.XML"));
    CHECK(isVehicleTable("cars.uav_carriers.patch.xml"));
    CHECK(isVehicleTable("TANKS.Mod.Patch.Xml"));
    CHECK_FALSE(isVehicleTable("cars..patch.xml"));
    CHECK_FALSE(isVehicleTable("humans.xml"));
    CHECK_FALSE(isVehicleTable("cars.xml.bak"));
    CHECK_FALSE(isVehicleTable("supercars.xml"));
}

TEST_CASE("merging rows: the later row wins, and only a different row is a clash") {
    SpawnConfig a;
    a.squad = "A";
    SpawnConfig b = a;
    b.icon = "other.dds";  // any column counts, not only the squad, kit and cooldown
    SpawnTable all = {{"Van", a}};
    CHECK(mergeRows(all, {{"Van", a}}).empty());
    CHECK(mergeRows(all, {{"Van", b}, {"Truck", a}}) == std::vector<std::string>{"Van"});
    CHECK(all["Van"].icon == b.icon);
    CHECK(all.size() == 2);
}

// ---------------------------------------------------------------- F4's look (look.h)

TEST_CASE("F4's look: the row's pictures and tooltips, the defaults for empty cells, the game's for other vehicles") {
    Look other = lookOf(nullptr);
    CHECK(other.tip == kGameTip);
    CHECK(other.tipRecall == kGameTipRecall);
    CHECK(other.icon.empty());

    SpawnConfig c;
    c.squad = "S";
    Look plain = lookOf(&c);
    CHECK(plain.icon == kDefaultIcon);
    CHECK(plain.iconCooldown == kDefaultIconCooldown);
    CHECK(plain.iconRecall == kDefaultIconRecall);
    CHECK(plain.tip == kDefaultTip);
    CHECK(plain.tipRecall == kDefaultTipRecall);
    CHECK(plain.tooLong.empty());

    c.icon = "a.dds";
    c.iconRecall = "b.dds";
    c.tooltip = "uavc_deploy";
    Look own = lookOf(&c);
    CHECK(own.icon == "a.dds");
    CHECK(own.iconCooldown == kDefaultIconCooldown);
    CHECK(own.iconRecall == "b.dds");
    CHECK(own.tip == "uavc_deploy");
    CHECK(own.tipRecall == kDefaultTipRecall);
}

TEST_CASE("a tooltip key longer than the game's string holds in place shows the default, and is reported") {
    SpawnConfig c;
    c.squad = "S";
    c.tooltip = std::string(kMaxTip, 'k');
    c.tooltipRecall = std::string(kMaxTip + 1, 'r');
    Look l = lookOf(&c);
    CHECK(l.tip == c.tooltip);
    CHECK(l.tipRecall == kDefaultTipRecall);
    CHECK(l.tooLong == c.tooltipRecall);
}

TEST_CASE("the framework's tooltip keys: the defaults and every vehicle's, nothing else") {
    SpawnConfig c;
    c.squad = "S";
    c.tooltipRecall = "uavc_recall";
    SpawnTable t = {{"Van", c}};
    CHECK(isFrameworkTip(t, kDefaultTip));
    CHECK(isFrameworkTip(t, "uavc_recall"));
    CHECK_FALSE(isFrameworkTip(t, kGameTip));
    CHECK_FALSE(isFrameworkTip(t, "another_mods_key"));
}
