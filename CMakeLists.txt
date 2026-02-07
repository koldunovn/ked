cmake_minimum_required(VERSION 3.14)
project(ked VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Strict warnings
add_compile_options(-Wall -Wextra -Wpedantic -Werror)

# Find netCDF
find_package(PkgConfig REQUIRED)
pkg_check_modules(NETCDF REQUIRED netcdf)

# Main executable
add_executable(ked
    src/main.c
    src/cli.c
    src/dataset.c
    src/io_netcdf.c
    src/ops_info.c
    src/term.c
    src/util.c
)

target_include_directories(ked PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${NETCDF_INCLUDE_DIRS}
)

target_link_directories(ked PRIVATE ${NETCDF_LIBRARY_DIRS})
target_link_libraries(ked PRIVATE ${NETCDF_LIBRARIES})

# Test data generator
add_executable(gen_testdata tools/gen_testdata.c)
target_include_directories(gen_testdata PRIVATE ${NETCDF_INCLUDE_DIRS})
target_link_directories(gen_testdata PRIVATE ${NETCDF_LIBRARY_DIRS})
target_link_libraries(gen_testdata PRIVATE ${NETCDF_LIBRARIES})

# Generate test data as part of build
add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/test_climate.nc
    COMMAND gen_testdata ${CMAKE_BINARY_DIR}/test_climate.nc
    DEPENDS gen_testdata
    COMMENT "Generating test climate data"
)
add_custom_target(testdata DEPENDS ${CMAKE_BINARY_DIR}/test_climate.nc)

# CTest
enable_testing()

add_test(NAME info_basic
    COMMAND ked info ${CMAKE_BINARY_DIR}/test_climate.nc)

add_test(NAME sinfo_basic
    COMMAND ked sinfo ${CMAKE_BINARY_DIR}/test_climate.nc)

add_test(NAME info_no_color
    COMMAND ${CMAKE_COMMAND} -E env NO_COLOR=1 $<TARGET_FILE:ked> info ${CMAKE_BINARY_DIR}/test_climate.nc)

add_test(NAME help_flag
    COMMAND ked --help)

add_test(NAME no_args
    COMMAND ked)
set_tests_properties(no_args PROPERTIES WILL_FAIL TRUE)

add_test(NAME nonexistent_file
    COMMAND ked info nonexistent_file.nc)
set_tests_properties(nonexistent_file PROPERTIES WILL_FAIL TRUE)

# Install
install(TARGETS ked RUNTIME DESTINATION bin)
