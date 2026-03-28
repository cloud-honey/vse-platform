if(EXISTS "/home/sykim/.openclaw/workspace/vse-platform/build-win/tests/TowerTycoonTests.exe")
  if(NOT EXISTS "/home/sykim/.openclaw/workspace/vse-platform/build-win/tests/TowerTycoonTests-b12d07c_tests.cmake" OR
     NOT "/home/sykim/.openclaw/workspace/vse-platform/build-win/tests/TowerTycoonTests-b12d07c_tests.cmake" IS_NEWER_THAN "/home/sykim/.openclaw/workspace/vse-platform/build-win/tests/TowerTycoonTests.exe" OR
     NOT "/home/sykim/.openclaw/workspace/vse-platform/build-win/tests/TowerTycoonTests-b12d07c_tests.cmake" IS_NEWER_THAN "${CMAKE_CURRENT_LIST_FILE}")
    include("/home/sykim/.openclaw/workspace/vse-platform/build-win/_deps/catch2-src/extras/CatchAddTests.cmake")
    catch_discover_tests_impl(
      TEST_EXECUTABLE [==[/home/sykim/.openclaw/workspace/vse-platform/build-win/tests/TowerTycoonTests.exe]==]
      TEST_EXECUTOR [==[]==]
      TEST_WORKING_DIR [==[/home/sykim/.openclaw/workspace/vse-platform/build-win/tests]==]
      TEST_SPEC [==[]==]
      TEST_EXTRA_ARGS [==[]==]
      TEST_PROPERTIES [==[]==]
      TEST_PREFIX [==[]==]
      TEST_SUFFIX [==[]==]
      TEST_LIST [==[TowerTycoonTests_TESTS]==]
      TEST_REPORTER [==[]==]
      TEST_OUTPUT_DIR [==[]==]
      TEST_OUTPUT_PREFIX [==[]==]
      TEST_OUTPUT_SUFFIX [==[]==]
      CTEST_FILE [==[/home/sykim/.openclaw/workspace/vse-platform/build-win/tests/TowerTycoonTests-b12d07c_tests.cmake]==]
      TEST_DL_PATHS [==[]==]
      CTEST_FILE [==[]==]
    )
  endif()
  include("/home/sykim/.openclaw/workspace/vse-platform/build-win/tests/TowerTycoonTests-b12d07c_tests.cmake")
else()
  add_test(TowerTycoonTests_NOT_BUILT TowerTycoonTests_NOT_BUILT)
endif()
