configure_file(config.h.in config.h)

Include(FetchContent)

FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG        v3.4.0 # or a later release
  FIND_PACKAGE_ARGS
)

FetchContent_MakeAvailable(Catch2)


add_executable(hwmondump_test hwmondump_test.cpp)
target_include_directories(hwmondump_test PUBLIC "${CMAKE_CURRENT_BINARY_DIR}")
target_compile_options(hwmondump_test PRIVATE -std=c++2b)
target_link_libraries(hwmondump_test PRIVATE Catch2::Catch2WithMain hwmondump_util)

add_executable(analysis_test analysis_test.cpp)
target_include_directories(analysis_test PUBLIC "${CMAKE_CURRENT_BINARY_DIR}")
target_compile_options(analysis_test PRIVATE -std=c++2b)
target_link_libraries(analysis_test PRIVATE Catch2::Catch2WithMain hwmondump_util)


add_test(NAME hwmondump_test COMMAND hwmondump_test)
add_test(NAME analysis_test COMMAND analysis_test)
add_test(NAME e2e_null COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/e2e_null_test.sh" "${PROJECT_BINARY_DIR}/hwmondump")
add_test(NAME e2e_sensor COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/e2e_sensor_test.sh" "${PROJECT_BINARY_DIR}/hwmondump")
