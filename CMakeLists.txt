cmake_minimum_required(VERSION 3.15)
project(PulseExec VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Export compile commands for IDEs
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Build type
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# Compiler flags
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -Wall -Wextra -Wpedantic")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG -march=native")

# Find required packages
find_package(Threads REQUIRED)
find_package(Boost 1.70 REQUIRED COMPONENTS system)
find_package(CURL REQUIRED)
find_package(SQLite3 REQUIRED)

# Include nlohmann/json
include(FetchContent)
FetchContent_Declare(
  json
  URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
)
FetchContent_MakeAvailable(json)

# Catch2 for testing
FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG v3.5.0
)
FetchContent_MakeAvailable(Catch2)

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${Boost_INCLUDE_DIRS})
include_directories(${CURL_INCLUDE_DIRS})
include_directories(${SQLite3_INCLUDE_DIRS})

# Source files
add_subdirectory(src)

# Tests
enable_testing()
add_subdirectory(tests)

# Benchmarks (optional)
option(BUILD_BENCHMARKS "Build benchmark suite" OFF)
if(BUILD_BENCHMARKS)
  add_subdirectory(bench)
endif()

# Install targets
install(TARGETS pulseexec DESTINATION bin)
