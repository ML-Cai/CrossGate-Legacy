cmake_minimum_required(VERSION 3.16)

set(PROJECT_NAME "cgl_render")
set(PROJECT_ALIAS "cgl::render")
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
# [Dependency check] [glfw]
#-------------------------------------------------------------------------------
find_package(glfw3 REQUIRED)


#-------------------------------------------------------------------------------
# [Dependency check] [vulkan]
#-------------------------------------------------------------------------------
find_package(Vulkan REQUIRED)
include_directories(${Vulkan_INCLUDE_DIRS})

#-------------------------------------------------------------------------------
# [build library]
#-------------------------------------------------------------------------------
include_directories(${CMAKE_CURRENT_LIST_DIR}/include)
include_directories(${CMAKE_CURRENT_LIST_DIR}/src)

file(GLOB_RECURSE LIBRARY_SRC "src/*.cpp")

add_library(${PROJECT_NAME} ${LIBRARY_SRC})
add_library(${PROJECT_ALIAS} ALIAS ${PROJECT_NAME})

target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(${PROJECT_NAME} PRIVATE glfw cgl::settings cgl::trace cgl::common)

#-------------------------------------------------------------------------------
# [build Tests]
#-------------------------------------------------------------------------------
# if (CGL_WITH_TEST)
#     add_subdirectory(tests)
# endif()
