cmake_minimum_required(VERSION 3.16)

set(PROJECT_NAME "cgl_graphics")
set(PROJECT_ALIAS "cgl::graphics")
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
target_link_libraries(${PROJECT_NAME} PUBLIC cgl::common PRIVATE glfw cgl::settings cgl::trace Vulkan::Vulkan)

# Embed compiled shaders; runtime loading does not depend on working directory.
if(NOT Vulkan_GLSLC_EXECUTABLE)
    message(FATAL_ERROR "glslc is required to compile the sprite shaders (install the Vulkan SDK).")
endif()
set(SPRITE_SHADER_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)
file(MAKE_DIRECTORY ${SPRITE_SHADER_DIR})
foreach(STAGE vert frag)
    set(SHADER_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/src/vulkan/shaders/sprite.${STAGE})
    set(SHADER_HEADER ${SPRITE_SHADER_DIR}/sprite_${STAGE}.inc)
    add_custom_command(OUTPUT ${SHADER_HEADER}
        COMMAND ${Vulkan_GLSLC_EXECUTABLE} -mfmt=c ${SHADER_SOURCE} -o ${SHADER_HEADER}
        DEPENDS ${SHADER_SOURCE}
        VERBATIM)
    target_sources(${PROJECT_NAME} PRIVATE ${SHADER_HEADER})
endforeach()
target_include_directories(${PROJECT_NAME} PRIVATE ${SPRITE_SHADER_DIR})

#-------------------------------------------------------------------------------
# [build Tests]
#-------------------------------------------------------------------------------
if (CGL_WITH_TEST)
    add_subdirectory(tests)
endif()
