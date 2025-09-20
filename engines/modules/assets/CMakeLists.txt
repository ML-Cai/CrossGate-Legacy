cmake_minimum_required(VERSION 3.16)

set(PROJECT_NAME "cgl_assets")
set(PROJECT_ALIAS "cgl::assets")
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
# [build library]
#-------------------------------------------------------------------------------
include_directories(${CMAKE_CURRENT_LIST_DIR}/include)
include_directories(${CMAKE_CURRENT_LIST_DIR}/src)

file(GLOB_RECURSE LIBRARY_SRC "src/*.cpp")

add_library(${PROJECT_NAME} ${LIBRARY_SRC})
add_library(${PROJECT_ALIAS} ALIAS ${PROJECT_NAME})
target_link_libraries(${PROJECT_NAME} PRIVATE cgl::settings cgl::trace cgl::utils cgl::common )

target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

#-------------------------------------------------------------------------------
# [build Tests]
#-------------------------------------------------------------------------------
if (CGL_WITH_TEST)
    add_subdirectory(tests)
endif()
