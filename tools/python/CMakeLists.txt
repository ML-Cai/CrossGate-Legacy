# ==============================================================================
#   The MIT License (MIT)
#
#   Copyright (c) 2024-2025 MengLun,Cai
#
#   All rights reserved.
# ==============================================================================
cmake_minimum_required(VERSION 3.15)

set(PROJECT_NAME "cgl_pybind")
project(${PROJECT_NAME})

include(FetchContent)

#-------------------------------------------------------------------------------
# [find python3]
#-------------------------------------------------------------------------------
find_package(Python3 REQUIRED COMPONENTS Interpreter Development)

#-------------------------------------------------------------------------------
# [find pybind11]
#-------------------------------------------------------------------------------
FetchContent_Declare(
  pybind11
  GIT_REPOSITORY https://github.com/pybind/pybind11.git
  GIT_TAG        v3.0.0
)

FetchContent_MakeAvailable(pybind11)
set(PYTHON_LIB_PATH "${Python3_LIBRARY_RELEASE}")


#-------------------------------------------------------------------------------
# [build pyd module]
#-------------------------------------------------------------------------------
add_library(${PROJECT_NAME} MODULE cgl_pybind.cpp)
target_sources(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../engines/src/assets/map_builder.cpp)
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../engines/src
    ${CMAKE_CURRENT_SOURCE_DIR}/../../modules/graphics/include)
target_include_directories(${PROJECT_NAME}  PRIVATE ${Python3_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} PRIVATE pybind11::pybind11 ${PYTHON_LIB_PATH} cgl::all)

set(CGL_OUTPUT_DIR ${CMAKE_BINARY_DIR}/bin)
set_target_properties(${PROJECT_NAME} PROPERTIES
    PREFIX ""
    OUTPUT_NAME "cgl"
    LIBRARY_OUTPUT_DIRECTORY ${CGL_OUTPUT_DIR}
    LIBRARY_OUTPUT_DIRECTORY_DEBUG ${CGL_OUTPUT_DIR}
    LIBRARY_OUTPUT_DIRECTORY_RELEASE ${CGL_OUTPUT_DIR}
    LIBRARY_OUTPUT_DIRECTORY_RELWITHDEBINFO ${CGL_OUTPUT_DIR}
    LIBRARY_OUTPUT_DIRECTORY_MINSIZEREL ${CGL_OUTPUT_DIR}
)
if(WIN32)
    set_target_properties(${PROJECT_NAME} PROPERTIES SUFFIX ".pyd")
endif()
if (CGL_WITH_TEST)
    add_test(NAME python_session_cache
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_session_cache.py
                --module-dir $<TARGET_FILE_DIR:${PROJECT_NAME}>)
    set_tests_properties(python_session_cache PROPERTIES
        LABELS "unit;memory;python" TIMEOUT 60)
    add_test(NAME python_map_snapshot
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_map_snapshot.py
                --module-dir $<TARGET_FILE_DIR:${PROJECT_NAME}>)
    set_tests_properties(python_map_snapshot PROPERTIES LABELS "unit;memory;python" TIMEOUT 60)
    add_test(NAME python_map_preview
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_map_preview.py
                --module-dir $<TARGET_FILE_DIR:${PROJECT_NAME}>)
    set_tests_properties(python_map_preview PROPERTIES LABELS "unit;memory;python" TIMEOUT 60)
    add_test(NAME python_animation_stream
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_animation_stream.py
                --module-dir $<TARGET_FILE_DIR:${PROJECT_NAME}>)
    set_tests_properties(python_animation_stream PROPERTIES LABELS "unit;memory;python" TIMEOUT 60)
endif()

if (CGL_WITH_TEST)
    add_test(NAME anime_viewer_selection
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/AnimeViewer/tests/test_preview.py)
    set_tests_properties(anime_viewer_selection PROPERTIES LABELS "unit;memory;python" TIMEOUT 30)
endif()
