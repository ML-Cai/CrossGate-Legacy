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

#-------------------------------------------------------------------------------
# Setup requirements
#-------------------------------------------------------------------------------
message(STATUS "Setup requirements ...")

find_package(Python3 REQUIRED COMPONENTS Interpreter)

execute_process(
    COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_LIST_DIR}/setup_requirements.py
            "${CMAKE_BINARY_DIR}/_deps/" # dependency install path
            "${CMAKE_BINARY_DIR}/_tmp"   # temp path
            "${CMAKE_BUILD_TYPE}"        # build type
    RESULT_VARIABLE DEPS_SETUP_RESULT
)

if(NOT DEPS_SETUP_RESULT EQUAL 0)
    message(FATAL_ERROR "setup_requirements.py failed with code ${DEPS_SETUP_RESULT}")
endif()

set(CMAKE_PREFIX_PATH "${CMAKE_BINARY_DIR}/_deps/${CMAKE_BUILD_TYPE};${CMAKE_PREFIX_PATH}")

#-------------------------------------------------------------------------------
# [Dependency check] [spdlog]
#-------------------------------------------------------------------------------
find_package(spdlog REQUIRED)

if (MSVC)
    add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")
endif()

#-------------------------------------------------------------------------------
# [Dependency check] [google-test]
#-------------------------------------------------------------------------------
if (CGL_WITH_TEST)
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    find_package(gtest REQUIRED)

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
