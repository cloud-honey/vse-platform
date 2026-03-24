add_executable(TowerTycoonTests
    PlaceholderTest.cpp
    # Tests will be added as tasks are implemented
    # Example: GridSystemTests.cpp
)

target_link_libraries(TowerTycoonTests PRIVATE
    Catch2::Catch2WithMain
    project_includes
    spdlog::spdlog
)

target_compile_options(TowerTycoonTests PRIVATE
    -Wall
    -Wextra
    -fno-exceptions
)

include(CTest)
include(Catch)
catch_discover_tests(TowerTycoonTests)
