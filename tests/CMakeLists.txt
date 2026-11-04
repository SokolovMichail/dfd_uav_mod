# Tests of the Spawn Framework plugin. They do not need the game.
#
#   core_test - the game-agnostic rules (plugins/spawn/core) through a plain test port

# A test is an ordinary doctest program run by CTest. It is not built like the mod DLLs (-static -mcrtdll=ucrt): in
# this MinGW libstdc++.a is built for msvcrt and libwinpthread.a for UCRT, and the I/O streams doctest uses do not
# link that way. So libstdc++/libgcc are linked statically, and libwinpthread-1.dll is copied next to the tests from
# the compiler folder. (Same as DFDRepairMod.)
get_filename_component(DFD_MINGW_BIN "${CMAKE_CXX_COMPILER}" DIRECTORY)
function(dfd_test name)
    add_executable(${name} ${ARGN})
    target_compile_definitions(${name} PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
    target_compile_options(${name} PRIVATE -Wall -Wextra -Wno-cast-function-type)
    target_link_options(${name} PRIVATE -static-libstdc++ -static-libgcc)
    target_include_directories(${name} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
    if (EXISTS "${DFD_MINGW_BIN}/libwinpthread-1.dll")
        add_custom_command(TARGET ${name} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different "${DFD_MINGW_BIN}/libwinpthread-1.dll"
                        "$<TARGET_FILE_DIR:${name}>")
    endif ()
    add_test(NAME ${name} COMMAND ${name} WORKING_DIRECTORY "$<TARGET_FILE_DIR:${name}>")
endfunction()

# The core, built on its own with <windows.h> switched off (its include guards defined up front): if the core ever
# starts depending on Windows or on logic.dll, this library stops compiling. doctest itself needs Windows, so the
# guard applies to the core sources only, not to core_test.cpp.
add_library(spawn_core_checked STATIC ${SPAWN_CORE_SOURCES})
target_compile_definitions(spawn_core_checked PRIVATE _WINDOWS_ _INC_WINDOWS)
target_compile_options(spawn_core_checked PRIVATE -Wall -Wextra)
dfd_test(core_test core_test.cpp)
target_link_libraries(core_test PRIVATE spawn_core_checked)
