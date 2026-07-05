cmake_minimum_required(VERSION 3.22)
project(ZenBoost CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

# ── Dobby hooking library ───────────────────────────────────────────────────
# Pinned to 5dfc854 (2024-03-14, last stable before regressions).
# PATCH_COMMAND fixes two Android build issues present in this commit:
#   1. armeabi-v7a: Cpu.h deleted from repo but registers-arm.h still includes it
#                   -> create a stub that forwards to CpuRegister.h
#   2. arm64-v8a:   closure_bridge_arm64.asm uses Apple Mach-O @PAGE/@PAGEOFF
#                   relocations which clang/ELF (Android NDK) rejects
#                   -> replace with ELF-compatible :lo12: syntax
FetchContent_Declare(Dobby
    GIT_REPOSITORY https://github.com/jmpews/Dobby.git
    GIT_TAG        5dfc8546954ce3b3198132ab13fddb89ee92cdd7
    PATCH_COMMAND  bash -c
        "printf '#pragma once\\n#include \\\"CpuRegister.h\\\"\\n' > source/core/arch/Cpu.h && sed -i 's/)@PAGE$/)/g' source/TrampolineBridge/ClosureTrampolineBridge/arm64/closure_bridge_arm64.asm && sed -i 's/, cdecl(\\([^)]*\\))@PAGEOFF/, :lo12:cdecl(\\1)/g' source/TrampolineBridge/ClosureTrampolineBridge/arm64/closure_bridge_arm64.asm"
    UPDATE_DISCONNECTED TRUE
)
set(DOBBY_BUILD_SHARED    OFF CACHE BOOL "" FORCE)
set(DOBBY_GENERATE_SHARED OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(Dobby)

# ── Dear ImGui (GLES2 backend) ──────────────────────────────────────────────
FetchContent_Declare(imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.91.6
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(imgui)
set(IMGUI_DIR ${imgui_SOURCE_DIR})

# ── ZenBoost shared library ─────────────────────────────────────────────────
add_library(ZenBoost SHARED
    src/main.cpp
    src/hooks/EGLHook.cpp
    src/hooks/InputHook.cpp
    src/features/FeatureManager.cpp
    src/gui/ZenGui.cpp

    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)

target_include_directories(ZenBoost PRIVATE
    include
    src
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
)

target_compile_definitions(ZenBoost PRIVATE
    IMGUI_IMPL_OPENGL_ES2
    GL_GLEXT_PROTOTYPES
)

target_link_libraries(ZenBoost
    dobby
    android
    log
    EGL
    GLESv2
)

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    add_custom_command(TARGET ZenBoost POST_BUILD
        COMMAND ${CMAKE_STRIP} --strip-all $<TARGET_FILE:ZenBoost>
        COMMENT "Stripping ZenBoost.so"
    )
endif()
