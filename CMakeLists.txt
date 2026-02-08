cmake_minimum_required(VERSION 3.14)
project(ked VERSION 0.3.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# POSIX/XSI features (realpath, etc.)
add_compile_definitions(_POSIX_C_SOURCE=200809L _XOPEN_SOURCE=700)

# Strict warnings
add_compile_options(-Wall -Wextra -Wpedantic -Werror)

# Find netCDF (required)
find_package(PkgConfig REQUIRED)
pkg_check_modules(NETCDF REQUIRED netcdf)

# Find ecCodes (optional - enables GRIB support)
pkg_check_modules(ECCODES eccodes)

# Source files
set(KED_SOURCES
    src/main.c
    src/cli.c
    src/dataset.c
    src/io.c
    src/io_netcdf.c
    src/ops_info.c
    src/ops_data.c
    src/term.c
    src/util.c
)

if(ECCODES_FOUND)
    list(APPEND KED_SOURCES src/io_grib.c)
    message(STATUS "ecCodes found (${ECCODES_VERSION}) - GRIB support enabled")
else()
    message(STATUS "ecCodes not found - GRIB support disabled")
endif()

# Main executable
add_executable(ked ${KED_SOURCES})

target_include_directories(ked PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${NETCDF_INCLUDE_DIRS}
)

target_link_directories(ked PRIVATE ${NETCDF_LIBRARY_DIRS})
target_link_libraries(ked PRIVATE ${NETCDF_LIBRARIES})

if(ECCODES_FOUND)
    target_compile_definitions(ked PRIVATE KED_HAS_ECCODES)
    target_include_directories(ked PRIVATE ${ECCODES_INCLUDE_DIRS})
    target_link_directories(ked PRIVATE ${ECCODES_LIBRARY_DIRS})
    target_link_libraries(ked PRIVATE ${ECCODES_LIBRARIES})
endif()

# Test data generator
add_executable(gen_testdata tools/gen_testdata.c)
target_include_directories(gen_testdata PRIVATE ${NETCDF_INCLUDE_DIRS})
target_link_directories(gen_testdata PRIVATE ${NETCDF_LIBRARY_DIRS})
target_link_libraries(gen_testdata PRIVATE ${NETCDF_LIBRARIES} m)

if(ECCODES_FOUND)
    target_compile_definitions(gen_testdata PRIVATE KED_HAS_ECCODES)
    target_include_directories(gen_testdata PRIVATE ${ECCODES_INCLUDE_DIRS})
    target_link_directories(gen_testdata PRIVATE ${ECCODES_LIBRARY_DIRS})
    target_link_libraries(gen_testdata PRIVATE ${ECCODES_LIBRARIES})
endif()

# Generate test data as part of build
add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/test_climate.nc
    COMMAND gen_testdata ${CMAKE_BINARY_DIR}/test_climate.nc
    DEPENDS gen_testdata
    COMMENT "Generating test climate data (netCDF)"
)

set(TESTDATA_DEPS ${CMAKE_BINARY_DIR}/test_climate.nc)

if(ECCODES_FOUND)
    add_custom_command(
        OUTPUT ${CMAKE_BINARY_DIR}/test_climate.grib2
        COMMAND gen_testdata ${CMAKE_BINARY_DIR}/test_climate.grib2
        DEPENDS gen_testdata
        COMMENT "Generating test climate data (GRIB2)"
    )
    list(APPEND TESTDATA_DEPS ${CMAKE_BINARY_DIR}/test_climate.grib2)
endif()

add_custom_target(testdata DEPENDS ${TESTDATA_DEPS})

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

if(ECCODES_FOUND)
    add_test(NAME grib_info
        COMMAND ked info ${CMAKE_BINARY_DIR}/test_climate.grib2)

    add_test(NAME grib_sinfo
        COMMAND ked sinfo ${CMAKE_BINARY_DIR}/test_climate.grib2)
endif()

# Phase 3: data operation tests
add_test(NAME copy_nc
    COMMAND ked copy ${CMAKE_BINARY_DIR}/test_climate.nc ${CMAKE_BINARY_DIR}/test_copy.nc)

add_test(NAME copy_verify
    COMMAND ked sinfo ${CMAKE_BINARY_DIR}/test_copy.nc)
set_tests_properties(copy_verify PROPERTIES DEPENDS copy_nc)

add_test(NAME select_var
    COMMAND ked select -v temperature ${CMAKE_BINARY_DIR}/test_climate.nc ${CMAKE_BINARY_DIR}/test_sel.nc)

add_test(NAME select_verify
    COMMAND ked sinfo ${CMAKE_BINARY_DIR}/test_sel.nc)
set_tests_properties(select_verify PROPERTIES DEPENDS select_var)

add_test(NAME merge_files
    COMMAND ${CMAKE_COMMAND} -E env
        $<TARGET_FILE:ked> select -v temperature ${CMAKE_BINARY_DIR}/test_climate.nc ${CMAKE_BINARY_DIR}/test_merge_a.nc)

add_test(NAME merge_files_b
    COMMAND ${CMAKE_COMMAND} -E env
        $<TARGET_FILE:ked> select -v precipitation ${CMAKE_BINARY_DIR}/test_climate.nc ${CMAKE_BINARY_DIR}/test_merge_b.nc)
set_tests_properties(merge_files_b PROPERTIES DEPENDS merge_files)

add_test(NAME merge_op
    COMMAND ked merge ${CMAKE_BINARY_DIR}/test_merge_a.nc ${CMAKE_BINARY_DIR}/test_merge_b.nc ${CMAKE_BINARY_DIR}/test_merged.nc)
set_tests_properties(merge_op PROPERTIES DEPENDS merge_files_b)

add_test(NAME cat_op
    COMMAND ked cat ${CMAKE_BINARY_DIR}/test_climate.nc ${CMAKE_BINARY_DIR}/test_climate.nc ${CMAKE_BINARY_DIR}/test_catted.nc)

add_test(NAME cat_verify
    COMMAND ked sinfo ${CMAKE_BINARY_DIR}/test_catted.nc)
set_tests_properties(cat_verify PROPERTIES DEPENDS cat_op)

# Compression tests
add_test(NAME copy_z0
    COMMAND ked copy -z 0 ${CMAKE_BINARY_DIR}/test_climate.nc ${CMAKE_BINARY_DIR}/test_z0.nc)

add_test(NAME copy_z1
    COMMAND ked copy -z 1 ${CMAKE_BINARY_DIR}/test_climate.nc ${CMAKE_BINARY_DIR}/test_z1.nc)

add_test(NAME copy_z1_verify
    COMMAND ked sinfo ${CMAKE_BINARY_DIR}/test_z1.nc)
set_tests_properties(copy_z1_verify PROPERTIES DEPENDS copy_z1)

# sel alias test
add_test(NAME sel_alias
    COMMAND ked sel -v temperature ${CMAKE_BINARY_DIR}/test_climate.nc ${CMAKE_BINARY_DIR}/test_sel_alias.nc)

add_test(NAME sel_alias_verify
    COMMAND ked sinfo ${CMAKE_BINARY_DIR}/test_sel_alias.nc)
set_tests_properties(sel_alias_verify PROPERTIES DEPENDS sel_alias)

# Multi-variable select
add_test(NAME select_multi
    COMMAND ked select -v temperature,precipitation ${CMAKE_BINARY_DIR}/test_climate.nc ${CMAKE_BINARY_DIR}/test_sel_multi.nc)

add_test(NAME select_multi_verify
    COMMAND ked sinfo ${CMAKE_BINARY_DIR}/test_sel_multi.nc)
set_tests_properties(select_multi_verify PROPERTIES DEPENDS select_multi)

if(ECCODES_FOUND)
    add_test(NAME grib_to_nc
        COMMAND ked copy ${CMAKE_BINARY_DIR}/test_climate.grib2 ${CMAKE_BINARY_DIR}/test_grib_copy.nc)

    add_test(NAME grib_to_nc_verify
        COMMAND ked sinfo ${CMAKE_BINARY_DIR}/test_grib_copy.nc)
    set_tests_properties(grib_to_nc_verify PROPERTIES DEPENDS grib_to_nc)
endif()

# Install
install(TARGETS ked RUNTIME DESTINATION bin)
