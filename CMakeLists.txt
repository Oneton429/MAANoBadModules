cmake_minimum_required(VERSION 3.20)
project(MAANoBadModules VERSION 0.0.1 LANGUAGES C CXX RC)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── Extract and sanitize version components for RC file ───────────────────────
set(VER_MAJOR ${PROJECT_VERSION_MAJOR})
if(NOT VER_MAJOR)
    set(VER_MAJOR 0)
endif()
set(VER_MINOR ${PROJECT_VERSION_MINOR})
if(NOT VER_MINOR)
    set(VER_MINOR 0)
endif()
set(VER_PATCH ${PROJECT_VERSION_PATCH})
if(NOT VER_PATCH)
    set(VER_PATCH 0)
endif()
set(VER_TWEAK ${PROJECT_VERSION_TWEAK})
if(NOT VER_TWEAK)
    set(VER_TWEAK 0)
endif()

# ── MinHook (compiled from source as a static lib) ────────────────────────────
# Building statically means no extra DLL is dropped alongside ours,
# which avoids MAA's "unknown DLL" detection in Bootstrapper.cs.
set(MINHOOK_SRC "${CMAKE_SOURCE_DIR}/vendor/minhook/src")

add_library(MinHookStatic STATIC
    ${MINHOOK_SRC}/hook.c
    ${MINHOOK_SRC}/buffer.c
    ${MINHOOK_SRC}/trampoline.c
    ${MINHOOK_SRC}/hde/hde32.c
    ${MINHOOK_SRC}/hde/hde64.c
)
target_include_directories(MinHookStatic PUBLIC
    "${CMAKE_SOURCE_DIR}/vendor/minhook/include"  # exposes MinHook.h
    "${MINHOOK_SRC}"                               # internal headers (buffer.h etc.)
)
# MinHook source is plain C; suppress warnings we don't own
target_compile_options(MinHookStatic PRIVATE /W0)
set_target_properties(MinHookStatic PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    # Keep the intermediate .lib out of the Release output folder
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/intermediate"
    ARCHIVE_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/intermediate"
)

# ── Hook DLL ──────────────────────────────────────────────────────────────────
set(FILE_DESC "MAANoBadModules Hook DLL")
set(ORIGINAL_FILENAME "MAANoBadModules.dll")
set(FILE_TYPE "2") # VFT_DLL
configure_file(version.rc.in ${CMAKE_CURRENT_BINARY_DIR}/version_dll.rc @ONLY)

add_library(MAANoBadModules SHARED
    src/dllmain.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/version_dll.rc
)
target_link_libraries(MAANoBadModules PRIVATE MinHookStatic)
target_compile_options(MAANoBadModules PRIVATE /W4 /WX)
set_target_properties(MAANoBadModules PROPERTIES
    OUTPUT_NAME "MAANoBadModules"
)
# Copy the whitelist config template next to the DLL after each build
add_custom_command(TARGET MAANoBadModules POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/MAANoBadModules.txt"
        $<TARGET_FILE_DIR:MAANoBadModules>
    COMMENT "Copying MAANoBadModules.txt to output dir"
)

# ── Launcher EXE (auto-inject on MAA startup) ─────────────────────────────────
# Place next to MAA.exe and MAANoBadModules.dll.
# Run MAALauncher.exe instead of MAA.exe.
set(FILE_DESC "MAALauncher")
set(ORIGINAL_FILENAME "MAALauncher.exe")
set(FILE_TYPE "1") # VFT_APP
configure_file(version.rc.in ${CMAKE_CURRENT_BINARY_DIR}/version_exe.rc @ONLY)

add_executable(MAALauncher
    launcher/launcher.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/version_exe.rc
)
target_compile_options(MAALauncher PRIVATE /W4 /WX)
set_target_properties(MAALauncher PROPERTIES
    WIN32_EXECUTABLE TRUE    # no console window
    OUTPUT_NAME "MAALauncher"
)
