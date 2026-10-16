cmake_minimum_required(VERSION 3.16)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  project(galaxy-rvr-tests CXX)
  set(CMAKE_CXX_STANDARD 17)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
  set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
  enable_testing()
endif()

if(NOT TARGET GTest::gtest_main)
  include(FetchContent)
  FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.zip
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
  )
  set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(googletest)
endif()

enable_testing()

set(FIRMWARE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../galaxy-rvr)

add_executable(galaxy_rvr_tests
  harness.cpp
  hardware_spy.cpp
  stubs/Arduino.cpp
  tests/test_protocol.cpp
  tests/test_app_state.cpp
  tests/test_rover.cpp
  tests/test_drivers.cpp
  ${FIRMWARE_DIR}/protocol.cpp
  ${FIRMWARE_DIR}/app_state.cpp
  ${FIRMWARE_DIR}/car_control.cpp
  ${FIRMWARE_DIR}/sensors.cpp
  ${FIRMWARE_DIR}/rgb.cpp
  ${FIRMWARE_DIR}/rover.cpp
)

target_include_directories(galaxy_rvr_tests PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/stubs
  ${FIRMWARE_DIR}
)

target_compile_definitions(galaxy_rvr_tests PRIVATE
  UNIT_TEST
  ARDUINO_AVR_UNO
)

target_compile_options(galaxy_rvr_tests PRIVATE
  -Wall
  -Wextra
  -Wno-c99-extensions
)

target_link_libraries(galaxy_rvr_tests PRIVATE GTest::gtest_main)

set_target_properties(galaxy_rvr_tests PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

include(GoogleTest)
gtest_discover_tests(galaxy_rvr_tests)
