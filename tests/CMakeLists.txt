add_executable(TowerTycoonTests
    PlaceholderTest.cpp
    test_SimClock.cpp
    test_ConfigManager.cpp
    test_GridSystem.cpp
    test_AgentSystem.cpp
    test_AgentElevator.cpp
    test_TransportSystem.cpp
    test_EconomyEngine.cpp
    test_StarRatingSystem.cpp
    test_Camera.cpp
    test_FontManager.cpp
    test_RenderFrame.cpp
    test_RenderAgent.cpp
    test_InputMapper.cpp
    test_DebugInfo.cpp
    test_Bootstrapper.cpp
    test_Integration.cpp
    test_SaveLoad.cpp
    test_ContentRegistry.cpp
    test_TenantSystem.cpp
    test_SpriteSheet.cpp
    test_HUDPanel.cpp
    test_EconomyLoop.cpp
    test_StairMovement.cpp
    test_Settlement.cpp
    test_GameOver.cpp
    test_GameStateManager.cpp
    test_Sprint4Integration.cpp
    test_Sprint5Integration.cpp
    test_BuildCursor.cpp
    test_SaveLoadPanel.cpp
)

target_link_libraries(TowerTycoonTests PRIVATE
    Catch2::Catch2WithMain
    project_includes
    spdlog::spdlog
    EnTT::EnTT
    nlohmann_json::nlohmann_json
    SDL2::SDL2
    SDL2_image::SDL2_image
    SDL2_ttf::SDL2_ttf
    imgui
)

# 구현 파일 링크
target_sources(TowerTycoonTests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/core/SimClock.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/core/EventBus.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/core/ConfigManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/core/Bootstrapper.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/core/ContentRegistry.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/core/GameStateManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/domain/GridSystem.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/domain/AgentSystem.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/domain/TransportSystem.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/domain/EconomyEngine.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/domain/StarRatingSystem.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/domain/SaveLoadSystem.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/domain/TenantSystem.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/domain/GameOverSystem.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/renderer/Camera.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/renderer/RenderFrameCollector.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/renderer/SDLRenderer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/renderer/InputMapper.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/renderer/DebugPanel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/renderer/HUDPanel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/renderer/SpriteSheet.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/renderer/AnimationSystem.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/renderer/FontManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/renderer/BuildCursor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/renderer/SaveLoadPanel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/renderer/AudioEngine.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/renderer/DayNightCycle.cpp
)

target_compile_options(TowerTycoonTests PRIVATE
    -Wall
    -Wextra
    -fno-exceptions
)

# 프로젝트 루트 경로를 매크로로 전달 (테스트에서 assets 파일 경로 사용)
target_compile_definitions(TowerTycoonTests PRIVATE
    VSE_PROJECT_ROOT="${CMAKE_SOURCE_DIR}"
    VSE_HAS_AUDIO=0
)

include(CTest)
include(Catch)
catch_discover_tests(TowerTycoonTests)
