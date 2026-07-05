cmake_minimum_required(VERSION 3.22)
project(ZenBoost CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

# ── Dobby hooking library (pinned stable commit) ─────────────────────────────
FetchContent_Declare(Dobby
    GIT_REPOSITORY https://github.com/jmpews/Dobby.git
    GIT_TAG        51a0682de31dfe35c3ecd77ed093ddb2f90bcce5
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
