add_executable(TowerTycoonTests
    PlaceholderTest.cpp
    test_SimClock.cpp
    test_ConfigManager.cpp
    test_GridSystem.cpp
    test_AgentSystem.cpp
    test_TransportSystem.cpp
    test_Camera.cpp
    test_RenderFrame.cpp
)

target_link_libraries(TowerTycoonTests PRIVATE
    Catch2::Catch2WithMain
    project_includes
    spdlog::spdlog
    EnTT::EnTT
    nlohmann_json::nlohmann_json
)

# 구현 파일 링크
target_sources(TowerTycoonTests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/core/SimClock.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/core/EventBus.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/core/ConfigManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/domain/GridSystem.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/domain/AgentSystem.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/domain/TransportSystem.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/renderer/Camera.cpp
)

target_compile_options(TowerTycoonTests PRIVATE
    -Wall
    -Wextra
    -fno-exceptions
)

# 프로젝트 루트 경로를 매크로로 전달 (테스트에서 assets 파일 경로 사용)
target_compile_definitions(TowerTycoonTests PRIVATE
    VSE_PROJECT_ROOT="${CMAKE_SOURCE_DIR}"
)

include(CTest)
include(Catch)
catch_discover_tests(TowerTycoonTests)
