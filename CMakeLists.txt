cmake_minimum_required(VERSION 3.22)
project(ZenBoost CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

# ── Dobby hooking library ───────────────────────────────────────────────────
# Pinned to 5dfc854 (2024-03-14). PATCH_COMMAND invokes cmake/DobbyPatch.cmake
# which fixes Android build issues in all known Dobby commits:
#   1. armeabi-v7a: Cpu.h deleted from repo but registers-arm.h still includes it
#   2. arm64-v8a: closure_bridge_arm64.asm uses Mach-O @PAGE/@PAGEOFF relocations;
#      replaced with GOT-indirect adrp+ldr for ELF shared libraries
#   3. os_arch_features.h: OSMemory used before declared (circular include)
#   4. ProcessRuntime.cc: load_address -> base, start -> start()
#   5. dobby_symbol_resolver.cc: load_address -> base
FetchContent_Declare(Dobby
    GIT_REPOSITORY https://github.com/jmpews/Dobby.git
    GIT_TAG        5dfc8546954ce3b3198132ab13fddb89ee92cdd7
    PATCH_COMMAND  ${CMAKE_COMMAND} -DDOBBY_SRC=<SOURCE_DIR>
                   -P ${CMAKE_SOURCE_DIR}/cmake/DobbyPatch.cmake
    UPDATE_DISCONNECTED TRUE
)
set(DOBBY_BUILD_SHARED    OFF CACHE BOOL "" FORCE)
set(DOBBY_GENERATE_SHARED OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(Dobby)

# ── Dear ImGui (GLES2 + Android backend) ───────────────────────────────────
FetchContent_Declare(imgui
    GIT_REPOSITORY https://github.com/dvaustria741-a11y/imgui.git
    GIT_TAG        776bf2ab0d61e719e58f2c6d27d109ab5dcf2af1
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
