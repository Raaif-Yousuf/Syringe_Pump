include(FetchContent)
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG v1.15.2
)
# Keep gtest's own runtime/settings out of our build.
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_executable(pump_tests
  test_pump_math.cpp
  test_state_machine.cpp
  test_golden_equivalence.cpp
)

target_link_libraries(pump_tests PRIVATE pump_core gtest_main)

include(GoogleTest)
gtest_discover_tests(pump_tests)
