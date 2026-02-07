cmake_minimum_required(VERSION 3.14)
project(ked VERSION 0.2.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

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
target_link_libraries(gen_testdata PRIVATE ${NETCDF_LIBRARIES})

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

# Install
install(TARGETS ked RUNTIME DESTINATION bin)
