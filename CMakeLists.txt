cmake_minimum_required(VERSION 3.21)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

project(TowerTycoon
    VERSION 0.1.0
    DESCRIPTION "Tower Tycoon — SimTower-style building management simulation"
    LANGUAGES CXX
)

# ──────────────────────────────────────────────
# Global settings
# ──────────────────────────────────────────────
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

# ──────────────────────────────────────────────
# macOS: support both Apple Silicon and Intel
# ──────────────────────────────────────────────
if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "macOS architectures" FORCE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0" CACHE STRING "Minimum macOS version" FORCE)
endif()

# ──────────────────────────────────────────────
# Fetch dependencies
# ──────────────────────────────────────────────
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(FetchDependencies)

# ──────────────────────────────────────────────
# Compiler warnings (project code only)
# ──────────────────────────────────────────────
add_library(project_warnings INTERFACE)
target_compile_options(project_warnings INTERFACE
    -Wall
    -Wextra
    -Wpedantic
    -fno-exceptions
)

# ──────────────────────────────────────────────
# Include directories
# ──────────────────────────────────────────────
add_library(project_includes INTERFACE)
target_include_directories(project_includes INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_compile_definitions(project_includes INTERFACE
    SPDLOG_NO_EXCEPTIONS  # required: project uses -fno-exceptions
)

# ──────────────────────────────────────────────
# Main executable
# ──────────────────────────────────────────────
add_executable(TowerTycoon
    src/main.cpp
    src/core/Bootstrapper.cpp
    src/core/ContentRegistry.cpp
    src/domain/GridSystem.cpp
    src/domain/AgentSystem.cpp
    src/domain/TransportSystem.cpp
    src/domain/EconomyEngine.cpp
    src/domain/StarRatingSystem.cpp
    src/domain/SaveLoadSystem.cpp
    src/domain/TenantSystem.cpp
    src/core/EventBus.cpp
    src/core/SimClock.cpp
    src/core/ConfigManager.cpp
    src/renderer/Camera.cpp
    src/renderer/SDLRenderer.cpp
    src/renderer/InputMapper.cpp
    src/renderer/RenderFrameCollector.cpp
    src/renderer/DebugPanel.cpp
)

target_compile_definitions(TowerTycoon PRIVATE
    VSE_PROJECT_ROOT="${CMAKE_SOURCE_DIR}"
)

target_link_libraries(TowerTycoon PRIVATE
    project_warnings
    project_includes
    SDL2::SDL2
    SDL2_image::SDL2_image
    SDL2_ttf::SDL2_ttf
    imgui
    EnTT::EnTT
    nlohmann_json::nlohmann_json
    msgpack-cxx
    spdlog::spdlog
)

# Windows (MinGW): SDL2main 링크 필수 (WinMain 리디렉션)
if(WIN32)
    target_link_libraries(TowerTycoon PRIVATE SDL2::SDL2main)
endif()

# macOS: link required frameworks
if(APPLE)
    target_link_libraries(TowerTycoon PRIVATE
        "-framework Cocoa"
        "-framework IOKit"
        "-framework CoreFoundation"
        "-framework CoreAudio"
        "-framework AudioToolbox"
        "-framework Metal"
        "-framework QuartzCore"
    )
endif()

# ──────────────────────────────────────────────
# Tests
# ──────────────────────────────────────────────
enable_testing()
# Catch2 cmake extras (provides catch_discover_tests)
list(APPEND CMAKE_MODULE_PATH "${catch2_SOURCE_DIR}/extras")
add_subdirectory(tests)
