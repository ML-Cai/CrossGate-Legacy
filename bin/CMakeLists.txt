cmake_minimum_required(VERSION 3.16)

# Find all .cpp files in the directory
file(GLOB_RECURSE SOURCES ${CMAKE_CURRENT_LIST_DIR}/*.cpp)

# Loop through each .cpp file and create an executable
foreach(SOURCE ${SOURCES})
    # Extract the file name without the directory and extension
    get_filename_component(EXE_NAME ${SOURCE} NAME_WE)

    # Create an executable for each .cpp file
    add_executable(${EXE_NAME} ${SOURCE})
    target_link_libraries(${EXE_NAME} PRIVATE cgl::all cgl::engine)
    set_target_properties(${EXE_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CGL_BIN_PATH})

    message(STATUS "Add bin ${EXE_NAME}")
endforeach()
