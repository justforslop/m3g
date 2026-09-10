cmake_minimum_required(VERSION 3.13)

project(m3g LANGUAGES C CXX)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE BOOL "Generate compile_commands.json for LSP" FORCE)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Debug or Release (Make BUILD=)" FORCE)
endif()

set(CMAKE_C_FLAGS_DEBUG "-O0 -g" CACHE STRING "Debug C flags" FORCE)
set(CMAKE_C_FLAGS_RELEASE "-Os -g0" CACHE STRING "Release C flags" FORCE)
set(CMAKE_C_FLAGS_RELWITHDEBINFO "-Os -g" CACHE STRING "RelWithDebInfo C flags" FORCE)
set(CMAKE_C_FLAGS_MINSIZEREL "-Os -g0" CACHE STRING "MinSizeRel C flags" FORCE)

set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g" CACHE STRING "Debug CXX flags" FORCE)
set(CMAKE_CXX_FLAGS_RELEASE "-Os -g0" CACHE STRING "Release CXX flags" FORCE)
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-Os -g" CACHE STRING "RelWithDebInfo CXX flags" FORCE)
set(CMAKE_CXX_FLAGS_MINSIZEREL "-Os -g0" CACHE STRING "MinSizeRel CXX flags" FORCE)

option(M3G_ENABLE_IPO "Enable LTO/IPO" OFF)

set(M3G_WARNINGS -Wall -Wextra)

macro(add_interface_lib name path)
    add_library(${name} INTERFACE)
    target_include_directories(${name} INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/${path}")
endmacro()

add_interface_lib(libs vendors/libs)

file(GLOB_RECURSE M3G_CXX_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)
file(GLOB_RECURSE M3G_C_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
)
set(M3G_VENDOR_C_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/vendors/cjson/cJSON.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/vendors/miniz/miniz.c"
)
if(M3G_CXX_SOURCES STREQUAL "")
    message(FATAL_ERROR "No C++ sources found in src/")
endif()

add_executable(m3g ${M3G_CXX_SOURCES} ${M3G_C_SOURCES} ${M3G_VENDOR_C_SOURCES})
target_compile_options(m3g PRIVATE ${M3G_WARNINGS})
target_include_directories(m3g PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/vendors"
    "${CMAKE_CURRENT_SOURCE_DIR}/vendors/miniz"
)
if(UNIX)
    target_compile_definitions(m3g PRIVATE _DEFAULT_SOURCE)
endif()
target_compile_definitions(m3g PRIVATE "APP_NAME=\"m3g\"")
target_compile_definitions(m3g PRIVATE $<$<CONFIG:Release>:NDEBUG> $<$<CONFIG:MinSizeRel>:NDEBUG>)

target_link_libraries(m3g PRIVATE
    libs
    m
)

if(M3G_ENABLE_IPO)
    include(CheckIPOSupported)
    check_ipo_supported(RESULT m3g_ipo_ok OUTPUT m3g_ipo_msg)
    if(m3g_ipo_ok)
        set_property(TARGET m3g PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
    else()
        message(STATUS "IPO requested but not supported: ${m3g_ipo_msg}")
    endif()
endif()

enable_testing()

set(TEST_BIN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/tests/bin")
file(MAKE_DIRECTORY "${TEST_BIN_DIR}")

set(TEST_N "" CACHE STRING "If set, run only matching NNN tests (comma-separated ok)")
set(TEST_S "" CACHE STRING "If set, run tests/NNN from NNN onward")

function(_pad_test_num raw out_var)
    string(LENGTH "${raw}" _len)
    if(_len EQUAL 1)
        set(${out_var} "00${raw}" PARENT_SCOPE)
    elseif(_len EQUAL 2)
        set(${out_var} "0${raw}" PARENT_SCOPE)
    else()
        set(${out_var} "${raw}" PARENT_SCOPE)
    endif()
endfunction()

function(_test_num_to_int num out_var)
    string(REGEX REPLACE "^0+" "" _unpadded "${num}")
    if(_unpadded STREQUAL "")
        set(_unpadded "0")
    endif()
    math(EXPR ${out_var} "${_unpadded}")
    set(${out_var} ${${out_var}} PARENT_SCOPE)
endfunction()

set(TEST_FILTER_LIST "")
if(NOT TEST_N STREQUAL "")
    string(REPLACE " " "" _test_n_compact "${TEST_N}")
    string(REPLACE "," ";" _test_n_parts "${_test_n_compact}")
    foreach(_part IN LISTS _test_n_parts)
        if(_part STREQUAL "")
            continue()
        endif()
        _pad_test_num("${_part}" _padded)
        list(APPEND TEST_FILTER_LIST "${_padded}")
    endforeach()
endif()

set(TEST_START_INT -1)
if(TEST_FILTER_LIST STREQUAL "" AND NOT TEST_S STREQUAL "")
    _pad_test_num("${TEST_S}" _test_start_padded)
    _test_num_to_int("${_test_start_padded}" TEST_START_INT)
endif()

set(TEST_IMPL_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/tests/impl.c")

file(GLOB TEST_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.c")
foreach(TEST_SOURCE ${TEST_SOURCES})
    get_filename_component(TEST_BASENAME ${TEST_SOURCE} NAME)
    if(TEST_BASENAME MATCHES "^([0-9][0-9][0-9])[-_].*\\.c$")
        set(TEST_NUM ${CMAKE_MATCH_1})
        if(NOT TEST_FILTER_LIST STREQUAL "")
            list(FIND TEST_FILTER_LIST "${TEST_NUM}" _filter_idx)
            if(_filter_idx LESS 0)
                continue()
            endif()
        endif()
        if(TEST_START_INT GREATER_EQUAL 0)
            _test_num_to_int("${TEST_NUM}" _test_num_int)
            if(_test_num_int LESS TEST_START_INT)
                continue()
            endif()
        endif()
        get_filename_component(TEST_NAME ${TEST_SOURCE} NAME_WE)
        add_executable(${TEST_NAME}
            ${TEST_SOURCE}
            ${TEST_IMPL_SOURCES}
        )
        target_compile_options(${TEST_NAME} PRIVATE ${M3G_WARNINGS})
        target_include_directories(${TEST_NAME} PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}"
            "${CMAKE_CURRENT_SOURCE_DIR}/include"
            "${CMAKE_CURRENT_SOURCE_DIR}/vendors/libs"
        )
        target_link_libraries(${TEST_NAME} PRIVATE libs)
        if(UNIX)
            target_compile_definitions(${TEST_NAME} PRIVATE _DEFAULT_SOURCE)
        endif()
        set_target_properties(${TEST_NAME} PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${TEST_BIN_DIR}"
            RUNTIME_OUTPUT_DIRECTORY_DEBUG "${TEST_BIN_DIR}"
            RUNTIME_OUTPUT_DIRECTORY_RELEASE "${TEST_BIN_DIR}"
        )
        add_test(NAME ${TEST_NAME} COMMAND "${TEST_BIN_DIR}/${TEST_NAME}")
    endif()
endforeach()
