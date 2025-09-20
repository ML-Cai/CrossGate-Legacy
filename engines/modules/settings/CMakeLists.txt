cmake_minimum_required(VERSION 3.16)

set(PROJECT_NAME "cgl_settings")
set(PROJECT_ALIAS "cgl::settings")
project(${PROJECT_NAME})


set(AUTO_GEN_PATH ${CMAKE_CURRENT_BINARY_DIR}/_gen)

#-------------------------------------------------------------------------------
# [Dependency check] [google-test]
#-------------------------------------------------------------------------------
if (CGL_WITH_TEST)
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    find_package(GTest REQUIRED)

    enable_testing()
endif()

# ------------------------------------------------------------------------------
# [Dependency check] [python]
# ------------------------------------------------------------------------------
find_package(Python3 COMPONENTS Interpreter REQUIRED)

# ------------------------------------------------------------------------------
# [Generate settings.hpp from engine_static_settings.json]
# ------------------------------------------------------------------------------
set(CGL_STATIC_SETTINGS_HEADER_EXPORT_PATH ${AUTO_GEN_PATH}/cgl/settings/engine_static_settings.h)

add_custom_command(
    OUTPUT ${CGL_STATIC_SETTINGS_HEADER_EXPORT_PATH}
    COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/generate_static_settings.py
            ${CMAKE_CURRENT_SOURCE_DIR}/engine_static_settings.json
            ${CGL_STATIC_SETTINGS_HEADER_EXPORT_PATH}
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/engine_static_settings.json
            ${CMAKE_CURRENT_SOURCE_DIR}/generate_static_settings.py
    COMMENT "Generating settings header from JSON..."
)

# ------------------------------------------------------------------------------
# [Generate settings.hpp from engine_runtime_settings.json]
# ------------------------------------------------------------------------------
set(CGL_RUNTIME_SETTINGS_HEADER_EXPORT_PATH ${AUTO_GEN_PATH}/cgl/settings/engine_runtime_settings.h)
set(CGL_RUNTIME_SETTINGS_CPP_EXPORT_PATH ${AUTO_GEN_PATH}/cgl/settings/engine_runtime_settings.cpp)
set(CGL_RUNTIME_SETTINGS_INI_EXPORT_PATH ${CGL_BIN_PATH}/settings.ini)

add_custom_command(
    OUTPUT ${CGL_RUNTIME_SETTINGS_HEADER_EXPORT_PATH}
           ${CGL_RUNTIME_SETTINGS_CPP_EXPORT_PATH}
           ${CGL_RUNTIME_SETTINGS_INI_EXPORT_PATH}
    COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/generate_runtime_settings.py
            ${CMAKE_CURRENT_SOURCE_DIR}/engine_runtime_settings.json
            ${CGL_RUNTIME_SETTINGS_HEADER_EXPORT_PATH}
            ${CGL_RUNTIME_SETTINGS_CPP_EXPORT_PATH}
            ${CGL_RUNTIME_SETTINGS_INI_EXPORT_PATH}
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/engine_runtime_settings.json
            ${CMAKE_CURRENT_SOURCE_DIR}/generate_runtime_settings.py
    COMMENT "Generating settings header from JSON..."
)

# ------------------------------------------------------------------------------
# [Create settings module]
# ------------------------------------------------------------------------------
add_custom_target(
    generate_cgl_settings_header
    DEPENDS ${CGL_STATIC_SETTINGS_HEADER_EXPORT_PATH}
            ${CGL_RUNTIME_SETTINGS_HEADER_EXPORT_PATH}
            ${CGL_RUNTIME_SETTINGS_CPP_EXPORT_PATH}
            ${CGL_RUNTIME_SETTINGS_INI_EXPORT_PATH}
)

add_library(${PROJECT_NAME} ${CGL_RUNTIME_SETTINGS_CPP_EXPORT_PATH})
add_library(${PROJECT_ALIAS} ALIAS ${PROJECT_NAME})
add_dependencies(${PROJECT_NAME} generate_cgl_settings_header)
target_link_libraries(${PROJECT_NAME} PRIVATE cgl::utils)
target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_LIST_DIR}/include ${AUTO_GEN_PATH})

#-------------------------------------------------------------------------------
# [build Tests]
#-------------------------------------------------------------------------------
if (CGL_WITH_TEST)
    add_subdirectory(tests)
endif()
