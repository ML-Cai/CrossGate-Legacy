cmake_minimum_required(VERSION 3.21)
project(CrossGatePythonTests LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
get_filename_component(root "${CMAKE_CURRENT_SOURCE_DIR}/../../../.." ABSOLUTE)
include("${root}/cmake/AddressSanitizer.cmake")
if(MSVC)
    add_compile_options(/EHsc /utf-8)
endif()
# Build the production binding and CPU modules, without desktop packages or mocks.
set(CGL_WITH_TEST OFF)
set(CGL_BIN_PATH "${CMAKE_CURRENT_BINARY_DIR}/bin")
enable_testing()
find_package(Python3 REQUIRED COMPONENTS Interpreter Development)
foreach(name IN ITEMS common utils settings trace assets)
    add_subdirectory("${root}/modules/${name}" "modules/${name}")
endforeach()

# Exercise the native reader adapter with the same real CPU readers as the binding.
# No Vulkan, graphics SDK or window initialization is needed.
if(NOT EXISTS "${root}/third_party/googletest/CMakeLists.txt")
    message(FATAL_ERROR "Initialize googletest: git submodule update --init third_party/googletest")
endif()
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
add_subdirectory("${root}/third_party/googletest" googletest EXCLUDE_FROM_ALL)
find_package(Threads REQUIRED)
include(GoogleTest)
add_executable(test_animation_builder "${root}/engines/tests/test_animation_builder.cpp"
    "${root}/engines/src/assets/animation_builder.cpp")
target_include_directories(test_animation_builder PRIVATE "${root}/engines/src")
target_link_libraries(test_animation_builder PRIVATE GTest::gtest_main Threads::Threads
    cgl::assets cgl::settings cgl::common)
set_target_properties(test_animation_builder PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CGL_BIN_PATH}")
gtest_discover_tests(test_animation_builder WORKING_DIRECTORY "${CGL_BIN_PATH}"
    PROPERTIES LABELS "unit\;memory\;headless" TIMEOUT 60)
add_library(cgl_all INTERFACE)
add_library(cgl::all ALIAS cgl_all)
target_link_libraries(cgl_all INTERFACE cgl::common cgl::utils cgl::settings cgl::trace cgl::assets)
add_subdirectory("${root}/tools/python" python)
foreach(name IN ITEMS session_cache map_snapshot map_preview animation_stream)
    add_test(NAME python_${name}
        COMMAND ${Python3_EXECUTABLE} "${root}/tools/python/tests/test_${name}.py"
                --module-dir $<TARGET_FILE_DIR:cgl_pybind>)
    set_tests_properties(python_${name} PROPERTIES LABELS "unit;memory;python;headless" TIMEOUT 60)
endforeach()
