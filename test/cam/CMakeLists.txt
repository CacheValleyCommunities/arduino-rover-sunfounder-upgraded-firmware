cmake_minimum_required(VERSION 3.16)

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

include(FetchContent)
FetchContent_Declare(
  ArduinoJson
  URL https://github.com/bblanchon/ArduinoJson/archive/refs/tags/v7.4.2.zip
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(ArduinoJson)

set(CAM_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../ai-camera-firmware)

add_executable(ai_camera_tests
  harness.cpp
  stubs/Arduino.cpp
  stubs/Preferences.cpp
  stubs/WiFi.cpp
  stubs/WiFiUdp.cpp
  stubs/ESPmDNS.cpp
  stubs/wifi_helper.cpp
  stubs/ws_server.cpp
  tests/test_identity.cpp
  tests/test_app_session.cpp
  tests/test_led.cpp
  tests/test_serial_bridge.cpp
  tests/test_c2_http.cpp
  tests/test_c2_telemetry.cpp
  tests/test_c2_program.cpp
  tests/test_udp.cpp
  ${CAM_DIR}/src/identity.cpp
  ${CAM_DIR}/src/app_session.cpp
  ${CAM_DIR}/src/led_status.cpp
  ${CAM_DIR}/src/log.cpp
  ${CAM_DIR}/src/serial_bridge.cpp
  ${CAM_DIR}/src/http_helpers.cpp
  ${CAM_DIR}/src/c2_program.cpp
  ${CAM_DIR}/src/c2_server.cpp
  ${CAM_DIR}/src/udp_server.cpp
)

target_include_directories(ai_camera_tests PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/stubs
  ${CAM_DIR}/include
)

target_compile_definitions(ai_camera_tests PRIVATE
  UNIT_TEST
  ARDUINOJSON_ENABLE_ARDUINO_STRING=1
  ARDUINOJSON_ENABLE_ARDUINO_STREAM=0
  ARDUINOJSON_ENABLE_ARDUINO_PRINT=0
  ARDUINOJSON_ENABLE_PROGMEM=0
)

target_compile_options(ai_camera_tests PRIVATE
  -Wall
  -Wextra
  -Wno-c99-extensions
)

target_link_libraries(ai_camera_tests PRIVATE
  GTest::gtest_main
  ArduinoJson
)

set_target_properties(ai_camera_tests PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

include(GoogleTest)
gtest_discover_tests(ai_camera_tests)
