include(FetchContent)

# ──────────────────────────────────────────────
# SDL2 — system package (apt/brew) preferred
# ──────────────────────────────────────────────
find_package(SDL2 REQUIRED)
find_package(SDL2_image REQUIRED)
# SDL2_ttf: Ubuntu apt 패키지는 SDL2_ttf::SDL2_ttf target 없는 경우 있음
# CONFIG 없이 MODULE 모드로 찾고, 없으면 pkg-config 시도
find_package(SDL2_ttf QUIET)
if(NOT SDL2_ttf_FOUND AND NOT TARGET SDL2_ttf::SDL2_ttf)
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(SDL2_TTF REQUIRED SDL2_ttf)
        add_library(SDL2_ttf::SDL2_ttf INTERFACE IMPORTED)
        target_include_directories(SDL2_ttf::SDL2_ttf INTERFACE ${SDL2_TTF_INCLUDE_DIRS})
        target_link_libraries(SDL2_ttf::SDL2_ttf INTERFACE ${SDL2_TTF_LIBRARIES})
    else()
        message(FATAL_ERROR "SDL2_ttf not found")
    endif()
endif()

# ──────────────────────────────────────────────
# EnTT (header-only)
# ──────────────────────────────────────────────
FetchContent_Declare(
    EnTT
    GIT_REPOSITORY https://github.com/skypjack/entt.git
    GIT_TAG        v3.13.2
    GIT_SHALLOW    FALSE
)
FetchContent_MakeAvailable(EnTT)

# ──────────────────────────────────────────────
# Dear ImGui (docking branch)
# ──────────────────────────────────────────────
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        docking
    GIT_SHALLOW    FALSE
)
FetchContent_GetProperties(imgui)
if(NOT imgui_POPULATED)
    FetchContent_Populate(imgui)
    # Build ImGui as a static library with SDL2 backend
    add_library(imgui STATIC
        ${imgui_SOURCE_DIR}/imgui.cpp
        ${imgui_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_SOURCE_DIR}/imgui_tables.cpp
        ${imgui_SOURCE_DIR}/imgui_widgets.cpp
        ${imgui_SOURCE_DIR}/imgui_demo.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl2.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_sdlrenderer2.cpp
    )
    target_include_directories(imgui PUBLIC
        ${imgui_SOURCE_DIR}
        ${imgui_SOURCE_DIR}/backends
    )
    target_link_libraries(imgui PUBLIC SDL2::SDL2)
    target_compile_options(imgui PRIVATE -w)  # suppress warnings in third-party code
endif()

# ──────────────────────────────────────────────
# nlohmann/json
# ──────────────────────────────────────────────
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3
    GIT_SHALLOW    FALSE
)
set(JSON_BuildTests OFF CACHE BOOL "" FORCE)
set(JSON_Install   OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(nlohmann_json)

# ──────────────────────────────────────────────
# msgpack-c
# ──────────────────────────────────────────────
FetchContent_Declare(
    msgpack
    GIT_REPOSITORY https://github.com/msgpack/msgpack-c.git
    GIT_TAG        cpp-6.1.1
    GIT_SHALLOW    FALSE
)
set(MSGPACK_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
set(MSGPACK_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(MSGPACK_USE_BOOST      OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(msgpack)

# ──────────────────────────────────────────────
# spdlog
# ──────────────────────────────────────────────
FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG        v1.14.1
    GIT_SHALLOW    FALSE
)
set(SPDLOG_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(spdlog)

# ──────────────────────────────────────────────
# Catch2 (v3)
# ──────────────────────────────────────────────
FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.6.0
    GIT_SHALLOW    FALSE
)
set(CATCH_INSTALL_DOCS    OFF CACHE BOOL "" FORCE)
set(CATCH_INSTALL_EXTRAS  OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(Catch2)
