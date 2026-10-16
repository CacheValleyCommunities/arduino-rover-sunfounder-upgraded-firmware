cmake_minimum_required(VERSION 3.16)
project(galaxy-rvr LANGUAGES CXX)

# Host-side tests and IntelliSense. Device firmware still builds with
# Arduino CLI / PlatformIO.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

enable_testing()
add_subdirectory(test/native)
add_subdirectory(test/cam)
