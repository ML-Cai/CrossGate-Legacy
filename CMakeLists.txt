cmake_minimum_required(VERSION 3.16)

set(PROJECT_NAME "CrossGate-Legacy")
project(${PROJECT_NAME})


#-------------------------------------------------------------------------------
# [Options]
#-------------------------------------------------------------------------------
option(CGL_WITH_TEST         "Build with unit tests"                         ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set_property(GLOBAL PROPERTY USE_FOLDERS TRUE)

#-------------------------------------------------------------------------------
# [Dependency check] [google-test]
#-------------------------------------------------------------------------------
if (CGL_WITH_TEST)
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    add_subdirectory(${CMAKE_SOURCE_DIR}/3rdparty/googletest-v1.16.x)
    set_target_properties(gtest PROPERTIES FOLDER "3rdparty/googletest")
    set_target_properties(gmock PROPERTIES FOLDER "3rdparty/googletest")
    set_target_properties(gmock_main PROPERTIES FOLDER "3rdparty/googletest")
    set_target_properties(gtest_main PROPERTIES FOLDER "3rdparty/googletest")
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
