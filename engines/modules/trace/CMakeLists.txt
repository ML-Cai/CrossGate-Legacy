cmake_minimum_required(VERSION 3.16)

set(PROJECT_NAME "cgl_logger")
set(PROJECT_ALIAS "cgl::logger")
project(${PROJECT_NAME})

#-------------------------------------------------------------------------------
# [Dependency check] [google-test]
#-------------------------------------------------------------------------------
if (CGL_WITH_TEST)
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    find_package(GTest REQUIRED)

    enable_testing()
endif()

#-------------------------------------------------------------------------------
# [Dependency check] [spdlog]
#-------------------------------------------------------------------------------
find_package(spdlog REQUIRED)

#-------------------------------------------------------------------------------
# [find cgl modules]
#-------------------------------------------------------------------------------
# find_package(cgl::common REQUIRED)

#-------------------------------------------------------------------------------
# [build library]
#-------------------------------------------------------------------------------
include_directories(${CMAKE_CURRENT_LIST_DIR}/include)
include_directories(${CMAKE_CURRENT_LIST_DIR}/src)

file(GLOB_RECURSE LIBRARY_SRC "src/*.cpp")

add_library(${PROJECT_NAME} ${LIBRARY_SRC})
add_library(${PROJECT_ALIAS} ALIAS ${PROJECT_NAME})
target_link_libraries(${PROJECT_NAME} PRIVATE spdlog::spdlog cgl::common cgl::settings)

#-------------------------------------------------------------------------------
# [build Tests]
#-------------------------------------------------------------------------------
if (CGL_WITH_TEST)
    add_subdirectory(tests)
endif()
