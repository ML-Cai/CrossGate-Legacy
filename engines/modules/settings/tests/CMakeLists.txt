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
    target_link_libraries(${EXE_NAME} PRIVATE GTest::gtest_main cgl::settings cgl::utils)
    set_target_properties(${EXE_NAME} PROPERTIES FOLDER "tests")

    gtest_discover_tests(${EXE_NAME})
    message(STATUS "Add test ${EXE_NAME}")
endforeach()
