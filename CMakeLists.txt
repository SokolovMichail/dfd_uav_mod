cmake_minimum_required(VERSION 3.20)
project(DFD_UAV C CXX ASM)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "The game is 64-bit: an x86_64 toolchain is needed")
endif ()

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
