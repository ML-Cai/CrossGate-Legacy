cmake_minimum_required(VERSION 3.16)

set(PROJECT_NAME "CrossGate-Legacy")
project(${PROJECT_NAME})

#-------------------------------------------------------------------------------
# [Options]
#-------------------------------------------------------------------------------
option(CGL_WITH_TEST         "Build with unit tests"                         ON)
option(CGL_WITH_TOOLS        "Build with tools"                              ON)
option(CGL_WITH_BINARIES     "Build with binaries"                           ON)

option(CGL_ENABLE_ASAN "Instrument project targets with AddressSanitizer" OFF)
if(CGL_ENABLE_ASAN)
    if(MSVC)
        add_compile_options(/fsanitize=address /Zi)
        add_compile_definitions(_DISABLE_VECTOR_ANNOTATION _DISABLE_STRING_ANNOTATION)
        add_link_options(/INCREMENTAL:NO)
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        add_compile_options(-fsanitize=address -fno-omit-frame-pointer -g)
        add_link_options(-fsanitize=address)
    else()
        message(FATAL_ERROR "CGL_ENABLE_ASAN is unsupported by this compiler")
    endif()
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set_property(GLOBAL PROPERTY USE_FOLDERS TRUE)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "RelWithDebInfo" CACHE STRING "Choose the type of build." FORCE)
endif()

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

if (MSVC)
    add_compile_options(/EHsc)

    # Enable UTF-8 encoding for build with spdlog
    add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")
endif()

set(CMAKE_PREFIX_PATH "${CMAKE_CURRENT_LIST_DIR}/third_party/build/${CMAKE_BUILD_TYPE};${CMAKE_PREFIX_PATH}")
set(CGL_BIN_PATH ${CMAKE_CURRENT_BINARY_DIR}/bin)

#-------------------------------------------------------------------------------
# [Dependency check] [google-test]
#-------------------------------------------------------------------------------
if (CGL_WITH_TEST)
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    find_package(GTest REQUIRED)

    enable_testing()
endif()

#-------------------------------------------------------------------------------
# [build modules]
#-------------------------------------------------------------------------------
add_subdirectory(modules/common)
add_subdirectory(modules/utils)
add_subdirectory(modules/settings)
add_subdirectory(modules/trace)
add_subdirectory(modules/assets)
add_subdirectory(modules/graphics)
add_subdirectory(engines/)

add_library(cgl_all INTERFACE)
add_library(cgl::all ALIAS cgl_all)
target_link_libraries(cgl_all INTERFACE
    cgl::common
    cgl::utils
    cgl::settings
    cgl::trace
    cgl::assets
)


#-------------------------------------------------------------------------------
# [build tools]
#-------------------------------------------------------------------------------
if (CGL_WITH_TOOLS)
    add_subdirectory(tools/python)
endif()

#-------------------------------------------------------------------------------
# [build bin]
#-------------------------------------------------------------------------------
if (CGL_WITH_BINARIES)
    add_subdirectory(bin)
endif()
