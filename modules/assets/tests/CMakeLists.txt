cmake_minimum_required(VERSION 3.16)

include(GoogleTest)

# Find all .cpp files in the directory
file(GLOB_RECURSE TEST_SOURCES ${CMAKE_CURRENT_LIST_DIR}/*.cpp)

# Loop through each .cpp file and create an executable
foreach(TEST_SOURCE ${TEST_SOURCES})
    # Extract the file name without the directory and extension
    get_filename_component(EXE_NAME ${TEST_SOURCE} NAME_WE)

    # Create an executable for each .cpp file
    add_executable(${EXE_NAME} ${TEST_SOURCE})
    target_link_libraries(${EXE_NAME} PRIVATE GTest::gtest_main cgl::assets cgl::settings cgl::trace cgl::utils cgl::common)
    set_target_properties(${EXE_NAME} PROPERTIES FOLDER "tests")
    set_target_properties(${EXE_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CGL_BIN_PATH})

    set(TEST_LABELS "assets")
    if(EXE_NAME STREQUAL "test_reader_lifecycle" OR EXE_NAME STREQUAL "test_map_layout_reader")
        set(TEST_LABELS "unit\;memory")
    endif()
    gtest_discover_tests(${EXE_NAME} WORKING_DIRECTORY ${CGL_BIN_PATH}
        PROPERTIES LABELS "${TEST_LABELS}" TIMEOUT 60)
    message(STATUS "Add test ${EXE_NAME}")
endforeach()
