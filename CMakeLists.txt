cmake_minimum_required(VERSION 3.20)
project(DFD_UAV C CXX ASM)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "The game is 64-bit: an x86_64 toolchain is needed")
endif ()

set(MOD_VERSION "0.1")
set(GAME_DIR "C:/Games/Terminator Dark Fate - Defiance" CACHE PATH "The game folder (game files for the mod build, deploy)")
find_package(Python3 REQUIRED COMPONENTS Interpreter)

# Common build settings for the mod DLLs: no dependency on the MinGW runtime (libstdc++/libgcc/winpthread), the
# system UCRT, no link time stamp (reproducible DLLs). Same as DFDRepairMod.
function(dfd_dll target)
    set_target_properties(${target} PROPERTIES PREFIX "")
    target_compile_definitions(${target} PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wno-cast-function-type -mcrtdll=ucrt)
    target_link_options(${target} PRIVATE -static -mcrtdll=ucrt -Wl,--no-insert-timestamp $<$<CONFIG:Release>:-s>)
endfunction()

# ---- winhttp.dll: proxy + plugin loader (bin\winhttp.dll), the same as DFDRepairMod's
add_library(winhttp SHARED
        loader/dllmain.cpp
        loader/winhttp_exports.S
        loader/winhttp.def)
dfd_dll(winhttp)

# ---- the Spawn Framework plugin
# core/ - the game-agnostic rules (no Windows, no logic.dll), shared by the plugin and the tests;
# logic_dll/ - the binding to the current game build, under the crash guard (common/guard.h).
set(SPAWN_CORE_SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/plugins/spawn/core/config.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/plugins/spawn/core/spawner.cpp")
set(DFD_GUARD_SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/common/guard.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/common/guard_x64.S")
set(SPAWN_SOURCES ${SPAWN_CORE_SOURCES} ${DFD_GUARD_SOURCES}
        "${CMAKE_CURRENT_SOURCE_DIR}/plugins/spawn/logic_dll/plugin.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/plugins/spawn/logic_dll/tables.cpp")
# uuid: FOLDERID_Documents, for the folder the game keeps mods.json in (tables.cpp)
function(spawn_plugin target entry)
    add_library(${target} SHARED ${SPAWN_SOURCES} "${CMAKE_CURRENT_SOURCE_DIR}/plugins/spawn/logic_dll/${entry}")
    dfd_dll(${target})
    target_compile_definitions(${target} PRIVATE "SPAWN_VERSION=\"${MOD_VERSION}\"")
    target_link_libraries(${target} PRIVATE uuid)
endfunction()
# The standalone DLL (bin\plugins\SpawnFramework.dll, loaded by our winhttp.dll) and the same plugin for
# DefianceLoader (DefianceLoader\plugins\spawn_framework.dll + spawn_framework.plugin.json, generated with MOD_VERSION).
spawn_plugin(SpawnFramework standalone.cpp)
spawn_plugin(spawn_framework defiance.cpp)
configure_file(plugins/spawn/spawn_framework.plugin.json.in
        "${CMAKE_CURRENT_BINARY_DIR}/spawn_framework.plugin.json" @ONLY)

# ---- the example mod Patchwork UAV (mods\Patchwork_UAV), built from the game's current files
set(MOD_STAGE "${CMAKE_BINARY_DIR}/stage/mods/Patchwork_UAV")
add_custom_target(mod
        COMMAND ${Python3_EXECUTABLE} mod/build_mod.py --game "${GAME_DIR}" --out "${MOD_STAGE}"
                --cache "${CMAKE_BINARY_DIR}/stage/_game_src" --version ${MOD_VERSION}
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        COMMENT "Build the example mod -> ${MOD_STAGE}"
        USES_TERMINAL VERBATIM)

# ---- deploy: the standalone plugin and the example mod into the game (the game must be closed). Removes the
# DefianceLoader build of the plugin if it is there (both at once do not work), and the probe.
add_custom_target(deploy
        COMMAND ${CMAKE_COMMAND} -E rm -f "${GAME_DIR}/DefianceLoader/plugins/spawn_framework.dll"
                "${GAME_DIR}/DefianceLoader/plugins/spawn_framework.plugin.json"
                "${GAME_DIR}/bin/plugins/SpawnProbe.dll" "${GAME_DIR}/bin/plugins/SpawnProbe.ini"
        COMMAND ${CMAKE_COMMAND} -E rm -rf "${GAME_DIR}/mods/SpawnProbe"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${GAME_DIR}/bin/plugins"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:winhttp> "${GAME_DIR}/bin/"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:SpawnFramework> "${GAME_DIR}/bin/plugins/"
        COMMAND ${CMAKE_COMMAND} -E rm -rf "${GAME_DIR}/mods/Patchwork_UAV"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${MOD_STAGE}" "${GAME_DIR}/mods/Patchwork_UAV"
        DEPENDS winhttp SpawnFramework mod
        COMMENT "Install SpawnFramework.dll and mods\\Patchwork_UAV into ${GAME_DIR}"
        VERBATIM)

# ---- Phase 0 probe (bin\plugins\SpawnProbe.dll) and its test vehicle (mods\SpawnProbe)
add_library(SpawnProbe SHARED plugins/spawn_probe/probe.cpp)
dfd_dll(SpawnProbe)

set(PROBE_MOD_STAGE "${CMAKE_BINARY_DIR}/stage/mods/SpawnProbe")
add_custom_target(probe_mod
        COMMAND ${Python3_EXECUTABLE} mod/probe/build_probe_mod.py --game "${GAME_DIR}" --out "${PROBE_MOD_STAGE}"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        COMMENT "Build the probe mod -> ${PROBE_MOD_STAGE}"
        USES_TERMINAL VERBATIM)

# The probe into the game (the game must be closed). bin\winhttp.dll is the same loader as DFDRepairMod's; other
# plugins in bin\plugins are left alone. SpawnProbe.ini is replaced with the project's (its keys change between
# probe versions).
add_custom_target(deploy_probe
        COMMAND ${CMAKE_COMMAND} -E make_directory "${GAME_DIR}/bin/plugins"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:winhttp> "${GAME_DIR}/bin/"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:SpawnProbe>
                "${CMAKE_SOURCE_DIR}/plugins/spawn_probe/SpawnProbe.ini" "${GAME_DIR}/bin/plugins/"
        COMMAND ${CMAKE_COMMAND} -E rm -rf "${GAME_DIR}/mods/SpawnProbe"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${PROBE_MOD_STAGE}" "${GAME_DIR}/mods/SpawnProbe"
        DEPENDS winhttp SpawnProbe probe_mod
        COMMENT "Install SpawnProbe.dll and mods\\SpawnProbe into ${GAME_DIR}"
        VERBATIM)

# ---- tests (tests/): cmake --build <build> && ctest --test-dir <build>
option(DFD_TESTS "Build the Spawn Framework tests" ON)
if (DFD_TESTS AND NOT DFD_NESTED_RELEASE)
    enable_testing()
    add_subdirectory(tests)
endif ()
