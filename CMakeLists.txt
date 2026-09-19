cmake_minimum_required(VERSION 3.14)
project(SyringePumpCore CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Builds the pump logic (SyringePump/pump_core.cpp) against the host mocks
# in host/ instead of the real Arduino core, AccelStepper and
# LiquidCrystal_I2C, so it can be compiled and unit tested on a PC.
add_library(pump_core STATIC
  SyringePump/pump_core.cpp
  host/host_mocks.cpp
)

target_compile_definitions(pump_core PUBLIC PUMP_HOST_BUILD)
target_include_directories(pump_core PUBLIC
  SyringePump
  host
)

enable_testing()
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/CMakeLists.txt")
  add_subdirectory(tests)
endif()
