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

if(CGL_WITH_TEST)
    foreach(CASE success large100 missing scale animation)
        add_test(NAME cgl_client_map_${CASE}
            COMMAND ${CMAKE_COMMAND} -DCLIENT=$<TARGET_FILE:cgl-client> -DCASE=${CASE}
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/map_startup.cmake)
        set_tests_properties(cgl_client_map_${CASE} PROPERTIES
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR} TIMEOUT 60 LABELS "desktop;integration")
    endforeach()
    add_test(NAME cgl_client_startup
        COMMAND ${CMAKE_COMMAND}
            -DCLIENT=$<TARGET_FILE:cgl-client>
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/startup.cmake)
    set_tests_properties(cgl_client_startup PROPERTIES
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR} TIMEOUT 60 LABELS "desktop;integration")
    add_test(NAME cgl_client_invalid_resources
        COMMAND ${CMAKE_COMMAND}
            -DCLIENT=$<TARGET_FILE:cgl-client>
            -DINVALID_RESOURCES=ON
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/startup.cmake)
    set_tests_properties(cgl_client_invalid_resources PROPERTIES
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR} TIMEOUT 60 LABELS "desktop;integration")
    foreach(CASE direction serial scale)
        add_test(NAME cgl_client_invalid_animation_${CASE}
            COMMAND ${CMAKE_COMMAND}
                -DCLIENT=$<TARGET_FILE:cgl-client>
                -DINVALID_ANIMATION=${CASE}
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/startup.cmake)
        set_tests_properties(cgl_client_invalid_animation_${CASE} PROPERTIES
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR} TIMEOUT 60 LABELS "desktop;integration")
    endforeach()
endif()
