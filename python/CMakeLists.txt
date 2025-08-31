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
find_package(Python3 COMPONENTS Development REQUIRED)

#-------------------------------------------------------------------------------
# [find pybind11]
#-------------------------------------------------------------------------------
FetchContent_Declare(
  pybind11
  GIT_REPOSITORY https://github.com/pybind/pybind11.git
  GIT_TAG        v3.0.0  # 或你要的版本
)

FetchContent_MakeAvailable(pybind11)
set(PYTHON_LIB_PATH "${Python3_LIBRARY_RELEASE}")


#-------------------------------------------------------------------------------
# [build pyd module]
#-------------------------------------------------------------------------------
add_library(${PROJECT_NAME} MODULE cgl_pybind.cpp)
target_include_directories(${PROJECT_NAME}  PRIVATE ${Python3_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} PRIVATE pybind11::pybind11 ${PYTHON_LIB_PATH} cgl spdlog::spdlog)

set(CGL_OUTPUT_DIR ${CMAKE_BINARY_DIR}/bin)
set_target_properties(${PROJECT_NAME} PROPERTIES
    PREFIX ""
    SUFFIX ".pyd"
    OUTPUT_NAME "cgl"
    LIBRARY_OUTPUT_DIRECTORY ${CGL_OUTPUT_DIR}
    LIBRARY_OUTPUT_DIRECTORY_DEBUG ${CGL_OUTPUT_DIR}
    LIBRARY_OUTPUT_DIRECTORY_RELEASE ${CGL_OUTPUT_DIR}
    LIBRARY_OUTPUT_DIRECTORY_RELWITHDEBINFO ${CGL_OUTPUT_DIR}
    LIBRARY_OUTPUT_DIRECTORY_MINSIZEREL ${CGL_OUTPUT_DIR}
)