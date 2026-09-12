cmake_minimum_required(VERSION 3.16)

project(m3g
    VERSION 0.1.0
    DESCRIPTION "JSR-184 / M3G decode and glTF convert (C++17)"
    HOMEPAGE_URL "https://github.com/justforslop/slop"
    LANGUAGES C CXX
)

# ---------------------------------------------------------------------------
# Options
# ---------------------------------------------------------------------------

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(M3G_IS_TOP_LEVEL ON)
else()
    set(M3G_IS_TOP_LEVEL OFF)
endif()

option(M3G_BUILD_CLI "Build the m3g command-line tool" ${M3G_IS_TOP_LEVEL})
option(M3G_BUILD_VIEWER "Build the Sokol + ImGui debug viewer" OFF)
option(M3G_BUILD_TESTS "Build and register unit tests" ${M3G_IS_TOP_LEVEL})
option(M3G_ENABLE_IPO "Enable LTO/IPO on the m3g library" OFF)
option(M3G_INSTALL "Generate install rules (package export)" ON)

# glTF export TUs (gltf_writer / png_writer) need cgltf + stb_image_write headers.
option(M3G_WITH_EXPORT "Build glTF export path (Converter / GltfExporter)" ON)

# Vendored default backends — OFF when embedding so consumers supply their own
# DeflateIo / ImageIo / JsonIo / GltfIo (and headers for export) without bloat.
# Top-level defaults ON so CLI/tests work after `make setup` / `zig build setup`.
option(M3G_BUNDLE_MINIZ "Compile vendors/miniz + DeflateIo adapter" ${M3G_IS_TOP_LEVEL})
option(M3G_BUNDLE_STB "Compile stb_image(_write) impl + ImageIo adapter" ${M3G_IS_TOP_LEVEL})
option(M3G_BUNDLE_CJSON "Compile vendors/cjson + JsonIo adapter" ${M3G_IS_TOP_LEVEL})
option(M3G_BUNDLE_CGLTF "Compile cgltf header impl + GltfIo adapter" ${M3G_IS_TOP_LEVEL})

# When not bundling, point at external single-header trees (or leave empty if
# the parent target already has the include path).
set(M3G_MINIZ_INCLUDE_DIR "" CACHE PATH "Include dir for miniz.h (if not M3G_BUNDLE_MINIZ)")
set(M3G_STB_INCLUDE_DIR "" CACHE PATH "Include dir containing stb/ or stb_image*.h")
set(M3G_CJSON_INCLUDE_DIR "" CACHE PATH "Include dir for cJSON.h (if not M3G_BUNDLE_CJSON)")
set(M3G_CGLTF_INCLUDE_DIR "" CACHE PATH "Include dir containing cgltf/ or cgltf.h")

# Imported targets the consumer already built (optional link into m3g).
set(M3G_MINIZ_TARGET "" CACHE STRING "Optional CMake target to link for miniz")
set(M3G_STB_TARGET "" CACHE STRING "Optional CMake target to link for stb")
set(M3G_CJSON_TARGET "" CACHE STRING "Optional CMake target to link for cJSON")
set(M3G_CGLTF_TARGET "" CACHE STRING "Optional CMake target to link for cgltf")

# ---------------------------------------------------------------------------
# Language / defaults (only force cache when top-level)
# ---------------------------------------------------------------------------

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(M3G_IS_TOP_LEVEL)
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE BOOL "Generate compile_commands.json for LSP" FORCE)
    if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
        set(CMAKE_BUILD_TYPE Debug CACHE STRING "Debug or Release" FORCE)
    endif()
endif()

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

set(M3G_WARNINGS -Wall -Wextra)

# ---------------------------------------------------------------------------
# m3g library (installable package target)
# ---------------------------------------------------------------------------

# Core decode always built. Export / Converter / vendor adapters are optional.
set(M3G_LIB_SOURCES
    src/decode/decoder.cpp
)

if(M3G_WITH_EXPORT)
    list(APPEND M3G_LIB_SOURCES
        src/converter.cpp
        src/export/gltf_exporter.cpp
        src/gltf/gltf_writer.cpp
        src/util/png_writer.cpp
    )
endif()

set(M3G_BACKEND_DEFS "") # compile defs for default_backends.cpp / impl.c
set(M3G_NEED_IMPL_C FALSE)

function(m3g_require_vendor_file relpath hint)
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${relpath}")
        message(FATAL_ERROR "Missing ${relpath} (${hint})")
    endif()
endfunction()

if(M3G_BUNDLE_MINIZ)
    m3g_require_vendor_file("vendors/miniz/miniz.c" "run setup-miniz or set M3G_BUNDLE_MINIZ=OFF")
    list(APPEND M3G_LIB_SOURCES src/deflate_io_miniz.cpp vendors/miniz/miniz.c)
    list(APPEND M3G_BACKEND_DEFS M3G_HAS_MINIZ_BACKEND=1)
endif()

if(M3G_BUNDLE_STB)
    m3g_require_vendor_file("vendors/stb/stb_image.h" "run setup-stb or set M3G_BUNDLE_STB=OFF")
    m3g_require_vendor_file("vendors/stb/stb_image_write.h" "run setup-stb or set M3G_BUNDLE_STB=OFF")
    list(APPEND M3G_LIB_SOURCES src/image_io_stb.cpp)
    list(APPEND M3G_BACKEND_DEFS M3G_HAS_STB_BACKEND=1 M3G_IMPL_STB=1)
    set(M3G_NEED_IMPL_C TRUE)
endif()

if(M3G_BUNDLE_CJSON)
    m3g_require_vendor_file("vendors/cjson/cJSON.c" "run setup-cjson or set M3G_BUNDLE_CJSON=OFF")
    list(APPEND M3G_LIB_SOURCES src/json_io_cjson.cpp vendors/cjson/cJSON.c)
    list(APPEND M3G_BACKEND_DEFS M3G_HAS_CJSON_BACKEND=1)
endif()

if(M3G_BUNDLE_CGLTF)
    m3g_require_vendor_file("vendors/cgltf/cgltf.h" "run setup-cgltf or set M3G_BUNDLE_CGLTF=OFF")
    m3g_require_vendor_file("vendors/cgltf/cgltf_write.h" "run setup-cgltf or set M3G_BUNDLE_CGLTF=OFF")
    list(APPEND M3G_LIB_SOURCES src/gltf_io_cgltf.cpp)
    list(APPEND M3G_BACKEND_DEFS M3G_HAS_CGLTF_BACKEND=1 M3G_IMPL_CGLTF=1)
    set(M3G_NEED_IMPL_C TRUE)
endif()

if(M3G_NEED_IMPL_C)
    list(APPEND M3G_LIB_SOURCES src/impl.c)
endif()

if(M3G_BUILD_CLI AND M3G_WITH_EXPORT)
    if(NOT M3G_BUNDLE_MINIZ OR NOT M3G_BUNDLE_STB OR NOT M3G_BUNDLE_CGLTF)
        message(WARNING
            "M3G_BUILD_CLI=ON with export expects default backends "
            "(M3G_BUNDLE_MINIZ/STB/CGLTF). Without them the CLI must not be "
            "the only place backends are registered — provide set_*_io or enable bundles.")
    endif()
endif()

if(M3G_WITH_EXPORT)
    if(NOT M3G_BUNDLE_CGLTF AND M3G_CGLTF_INCLUDE_DIR STREQUAL "" AND M3G_CGLTF_TARGET STREQUAL "")
        message(FATAL_ERROR
            "M3G_WITH_EXPORT=ON needs cgltf headers: enable M3G_BUNDLE_CGLTF "
            "or set M3G_CGLTF_INCLUDE_DIR / M3G_CGLTF_TARGET")
    endif()
    if(NOT M3G_BUNDLE_STB AND M3G_STB_INCLUDE_DIR STREQUAL "" AND M3G_STB_TARGET STREQUAL "")
        message(FATAL_ERROR
            "M3G_WITH_EXPORT=ON needs stb_image_write: enable M3G_BUNDLE_STB "
            "or set M3G_STB_INCLUDE_DIR / M3G_STB_TARGET")
    endif()
endif()

add_library(m3g ${M3G_LIB_SOURCES})
add_library(m3g::m3g ALIAS m3g)

set_target_properties(m3g PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    EXPORT_NAME m3g
    OUTPUT_NAME m3g
    POSITION_INDEPENDENT_CODE ON
)

target_compile_options(m3g PRIVATE ${M3G_WARNINGS})

target_compile_features(m3g PUBLIC cxx_std_17 c_std_99)

target_include_directories(m3g
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Layout expected by sources: "miniz.h", "stb/stb_image.h", "cgltf/cgltf.h", "cJSON.h"
if(M3G_BUNDLE_MINIZ)
    target_include_directories(m3g PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/vendors/miniz
        ${CMAKE_CURRENT_SOURCE_DIR}/vendors
    )
elseif(NOT M3G_MINIZ_INCLUDE_DIR STREQUAL "")
    target_include_directories(m3g PRIVATE ${M3G_MINIZ_INCLUDE_DIR})
endif()

if(M3G_BUNDLE_STB)
    target_include_directories(m3g PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/vendors)
elseif(NOT M3G_STB_INCLUDE_DIR STREQUAL "")
    target_include_directories(m3g PRIVATE ${M3G_STB_INCLUDE_DIR})
endif()

if(M3G_BUNDLE_CJSON)
    target_include_directories(m3g PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/vendors/cjson
        ${CMAKE_CURRENT_SOURCE_DIR}/vendors
    )
elseif(NOT M3G_CJSON_INCLUDE_DIR STREQUAL "")
    target_include_directories(m3g PRIVATE ${M3G_CJSON_INCLUDE_DIR})
endif()

if(M3G_BUNDLE_CGLTF)
    target_include_directories(m3g PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/vendors
        ${CMAKE_CURRENT_SOURCE_DIR}/vendors/cgltf
    )
elseif(NOT M3G_CGLTF_INCLUDE_DIR STREQUAL "")
    target_include_directories(m3g PRIVATE ${M3G_CGLTF_INCLUDE_DIR})
endif()

if(NOT M3G_MINIZ_TARGET STREQUAL "")
    target_link_libraries(m3g PUBLIC ${M3G_MINIZ_TARGET})
endif()
if(NOT M3G_STB_TARGET STREQUAL "")
    target_link_libraries(m3g PUBLIC ${M3G_STB_TARGET})
endif()
if(NOT M3G_CJSON_TARGET STREQUAL "")
    target_link_libraries(m3g PUBLIC ${M3G_CJSON_TARGET})
endif()
if(NOT M3G_CGLTF_TARGET STREQUAL "")
    target_link_libraries(m3g PUBLIC ${M3G_CGLTF_TARGET})
endif()

if(UNIX)
    target_compile_definitions(m3g PRIVATE _DEFAULT_SOURCE)
    target_link_libraries(m3g PUBLIC m)
endif()

target_compile_definitions(m3g PRIVATE
    $<$<CONFIG:Release>:NDEBUG>
    $<$<CONFIG:MinSizeRel>:NDEBUG>
    ${M3G_BACKEND_DEFS}
)

if(NOT M3G_WITH_EXPORT)
    target_compile_definitions(m3g PUBLIC M3G_NO_EXPORT=1)
endif()

if(M3G_ENABLE_IPO)
    include(CheckIPOSupported)
    check_ipo_supported(RESULT m3g_ipo_ok OUTPUT m3g_ipo_msg)
    if(m3g_ipo_ok)
        set_property(TARGET m3g PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
    else()
        message(STATUS "IPO requested but not supported: ${m3g_ipo_msg}")
    endif()
endif()

# ---------------------------------------------------------------------------
# CLI tool
# ---------------------------------------------------------------------------

if(M3G_BUILD_CLI AND NOT M3G_WITH_EXPORT)
    message(FATAL_ERROR "M3G_BUILD_CLI requires M3G_WITH_EXPORT=ON (Converter / glTF export)")
endif()

if(M3G_BUILD_CLI)
    add_executable(m3g_cli src/main.cpp)
    set_target_properties(m3g_cli PROPERTIES OUTPUT_NAME m3g)
    target_compile_options(m3g_cli PRIVATE ${M3G_WARNINGS})
    target_compile_definitions(m3g_cli PRIVATE "APP_NAME=\"m3g\"")
    if(UNIX)
        target_compile_definitions(m3g_cli PRIVATE _DEFAULT_SOURCE)
    endif()
    target_link_libraries(m3g_cli PRIVATE m3g::m3g)
endif()

# ---------------------------------------------------------------------------
# Optional viewer (Sokol + ImGui) — not part of the installed package by default
# ---------------------------------------------------------------------------

if(M3G_BUILD_VIEWER)
    set(M3G_IMGUI_SOURCES
        vendors/imgui/imgui.cpp
        vendors/imgui/imgui_draw.cpp
        vendors/imgui/imgui_tables.cpp
        vendors/imgui/imgui_widgets.cpp
    )
    foreach(_f IN LISTS M3G_IMGUI_SOURCES)
        if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${_f}")
            message(FATAL_ERROR "M3G_BUILD_VIEWER requires ${_f} (run: make setup-imgui)")
        endif()
    endforeach()
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/vendors/sokol/sokol_app.h")
        message(FATAL_ERROR "M3G_BUILD_VIEWER requires Sokol headers (run: make setup-sokol)")
    endif()

    add_executable(m3g_viewer src/debug.c ${M3G_IMGUI_SOURCES})
    set_source_files_properties(src/debug.c PROPERTIES LANGUAGE CXX)
    set_target_properties(m3g_viewer PROPERTIES OUTPUT_NAME m3g-view)
    target_compile_options(m3g_viewer PRIVATE ${M3G_WARNINGS})
    target_include_directories(m3g_viewer PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/vendors
        ${CMAKE_CURRENT_SOURCE_DIR}/vendors/imgui
    )
    if(UNIX)
        target_compile_definitions(m3g_viewer PRIVATE _DEFAULT_SOURCE)
        target_link_libraries(m3g_viewer PRIVATE m3g::m3g GL X11 Xi Xcursor dl pthread)
    elseif(WIN32)
        # src/debug.c uses SOKOL_D3D11 on Windows (not WGL).
        target_link_libraries(m3g_viewer PRIVATE m3g::m3g d3d11 dxgi user32 gdi32 shell32)
    else()
        target_link_libraries(m3g_viewer PRIVATE m3g::m3g)
    endif()
endif()

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------

if(M3G_BUILD_TESTS)
    enable_testing()

    set(TEST_BIN_DIR "${CMAKE_CURRENT_BINARY_DIR}/tests/bin")
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

    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/vendors/libs/testfw.h")
        message(STATUS "tests: vendors/libs/testfw.h missing (make setup-libs); skipping unit tests")
    else()
                set(TEST_IMPL_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/tests/impl.c")
        file(GLOB TEST_SOURCES_C CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.c")
        file(GLOB TEST_SOURCES_CXX CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp")
        set(TEST_SOURCES ${TEST_SOURCES_C} ${TEST_SOURCES_CXX})
        foreach(TEST_SOURCE ${TEST_SOURCES})
            get_filename_component(TEST_BASENAME ${TEST_SOURCE} NAME)
            if(TEST_BASENAME STREQUAL "impl.c")
                continue()
            endif()
            if(NOT TEST_BASENAME MATCHES "^([0-9][0-9][0-9])[-_].*\\.(c|cpp)$")
                continue()
            endif()
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
            get_filename_component(TEST_EXT ${TEST_SOURCE} EXT)
            add_executable(${TEST_NAME} ${TEST_SOURCE} ${TEST_IMPL_SOURCES})
            target_compile_options(${TEST_NAME} PRIVATE ${M3G_WARNINGS})
            target_include_directories(${TEST_NAME} PRIVATE
                ${CMAKE_CURRENT_SOURCE_DIR}
                ${CMAKE_CURRENT_SOURCE_DIR}/include
                ${CMAKE_CURRENT_SOURCE_DIR}/vendors/libs
            )
            if(TEST_EXT STREQUAL ".cpp")
                target_link_libraries(${TEST_NAME} PRIVATE m3g::m3g)
            endif()
            if(UNIX)
                target_compile_definitions(${TEST_NAME} PRIVATE _DEFAULT_SOURCE)
            endif()
            set_target_properties(${TEST_NAME} PROPERTIES
                RUNTIME_OUTPUT_DIRECTORY "${TEST_BIN_DIR}"
                RUNTIME_OUTPUT_DIRECTORY_DEBUG "${TEST_BIN_DIR}"
                RUNTIME_OUTPUT_DIRECTORY_RELEASE "${TEST_BIN_DIR}"
            )
            add_test(NAME ${TEST_NAME} COMMAND "$<TARGET_FILE:${TEST_NAME}>"
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
        endforeach()
    endif()
endif()

# ---------------------------------------------------------------------------
# Install / CMake package (find_package(m3g))
# ---------------------------------------------------------------------------

if(M3G_INSTALL)
    set(M3G_INSTALL_CMAKEDIR "${CMAKE_INSTALL_LIBDIR}/cmake/m3g")

    install(TARGETS m3g
        EXPORT m3gTargets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )

    if(M3G_BUILD_CLI)
        install(TARGETS m3g_cli
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        )
    endif()

    install(FILES
        include/m3g.hpp
        include/m3g.h
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )

    install(EXPORT m3gTargets
        FILE m3gTargets.cmake
        NAMESPACE m3g::
        DESTINATION ${M3G_INSTALL_CMAKEDIR}
    )

    configure_package_config_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/m3gConfig.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/m3gConfig.cmake"
        INSTALL_DESTINATION ${M3G_INSTALL_CMAKEDIR}
    )

    write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/m3gConfigVersion.cmake"
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion
    )

    install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/m3gConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/m3gConfigVersion.cmake"
        DESTINATION ${M3G_INSTALL_CMAKEDIR}
    )

    # Build-tree export (add_subdirectory / export without install)
    export(EXPORT m3gTargets
        FILE "${CMAKE_CURRENT_BINARY_DIR}/m3gTargets.cmake"
        NAMESPACE m3g::
    )
endif()

# ---------------------------------------------------------------------------
# Summary
# ---------------------------------------------------------------------------

message(STATUS "m3g ${PROJECT_VERSION}: CLI=${M3G_BUILD_CLI} viewer=${M3G_BUILD_VIEWER} tests=${M3G_BUILD_TESTS} install=${M3G_INSTALL} export=${M3G_WITH_EXPORT}")
message(STATUS "m3g backends: miniz=${M3G_BUNDLE_MINIZ} stb=${M3G_BUNDLE_STB} cjson=${M3G_BUNDLE_CJSON} cgltf=${M3G_BUNDLE_CGLTF}")
