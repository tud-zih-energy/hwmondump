cmake_minimum_required(VERSION 3.24)
project(sensor_test VERSION 1.0.0)

option(BUILD_TESTING "enable tests" OFF)

include(GNUInstallDirs)

include(FetchContent)
FetchContent_Declare(
    argparse
    GIT_REPOSITORY https://github.com/p-ranav/argparse.git
    FIND_PACKAGE_ARGS
)
FetchContent_MakeAvailable(argparse)

FetchContent_Declare(
    tomlplusplus
    GIT_REPOSITORY https://github.com/marzer/tomlplusplus.git
    GIT_TAG v3.4.0
)
FetchContent_MakeAvailable(tomlplusplus)

find_package(PkgConfig REQUIRED)
pkg_check_modules(CPUID REQUIRED libcpuid)

add_library(hwmondump_util INTERFACE)
target_sources(hwmondump_util PUBLIC
  FILE_SET HEADERS
  FILES include/hwmondump_util.hpp
)
target_include_directories(hwmondump_util INTERFACE
    include/
    "${CPUID_INCLUDE_DIRS}"
    "${tomlplusplus_SOURCE_DIR}/include"
)
target_link_libraries(hwmondump_util INTERFACE
    sensors
    argparse::argparse
    "${CPUID_LIBRARIES}"
    uuid
)

add_executable(hwmondump bin/hwmondump.cpp)
target_compile_options(hwmondump PRIVATE -std=c++20)
target_link_libraries(hwmondump PUBLIC hwmondump_util)

install(TARGETS hwmondump)

add_subdirectory(man)

if (BUILD_TESTING)
  enable_testing()
  add_subdirectory(test)
endif()
