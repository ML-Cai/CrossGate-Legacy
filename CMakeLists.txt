cmake_minimum_required(VERSION 3.16)

set(PROJECT_NAME "CrossGate-Legacy")
project(${PROJECT_NAME})

#-------------------------------------------------------------------------------
# [Options]
#-------------------------------------------------------------------------------
option(CGL_WITH_TEST         "Build with unit tests"                         ON)
option(CGL_WITH_BIN          "Build with binary"                             ON)

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

#-------------------------------------------------------------------------------
# Setup requirements
#-------------------------------------------------------------------------------
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(CMAKE_PREFIX_PATH "${CMAKE_CURRENT_LIST_DIR}/third_party/build/${CMAKE_BUILD_TYPE};${CMAKE_PREFIX_PATH}")

#-------------------------------------------------------------------------------
# [Dependency check] [spdlog]
#-------------------------------------------------------------------------------

find_package(spdlog REQUIRED)


#-------------------------------------------------------------------------------
# [Dependency check] [google-test]
#-------------------------------------------------------------------------------
if (CGL_WITH_TEST)
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    find_package(GTest REQUIRED)

    enable_testing()
endif()

#-------------------------------------------------------------------------------
# [build library]
#-------------------------------------------------------------------------------
include_directories(${CMAKE_CURRENT_LIST_DIR}/include)
include_directories(${CMAKE_CURRENT_LIST_DIR}/src)

file(GLOB_RECURSE LIBRARY_SRC "src/*.cpp")

add_library(cgl ${LIBRARY_SRC})
set_target_properties(cgl PROPERTIES FOLDER "CGL")
target_link_libraries(cgl PRIVATE spdlog::spdlog)
# target_precompile_headers(cgl PRIVATE ${CMAKE_CURRENT_LIST_DIR}/src/utils/pch.h)

set (PROJECT_LIBS ${PROJECT_LIBS} cgl)

if (MSVC)
    target_compile_options(cgl PRIVATE /W4 /WX)
endif()

#-------------------------------------------------------------------------------
# [build Tests]
#-------------------------------------------------------------------------------
if (CGL_WITH_TEST)
    add_subdirectory(tests)
endif()

#-------------------------------------------------------------------------------
# [build bin]
#-------------------------------------------------------------------------------
if (CGL_WITH_BIN)
    add_subdirectory(python)
endif()
