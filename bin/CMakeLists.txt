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
    # Instrumented asset initialization can outlast the ordinary 240-frame run.
    # Keep a bounded run, with enough frames to reach and validate scene entry.
    set(startup_frames 240)
    set(startup_timeout 50)
    if(CGL_ENABLE_ASAN)
        set(startup_frames 1200)
        set(startup_timeout 100)
    endif()
    math(EXPR startup_test_timeout "${startup_timeout} + 10")
    set(startup_options -DSTARTUP_FRAMES=${startup_frames} -DSTARTUP_TIMEOUT=${startup_timeout})
    foreach(CASE success large100 missing scale animation)
        add_test(NAME cgl_client_map_${CASE}
            COMMAND ${CMAKE_COMMAND} -DCLIENT=$<TARGET_FILE:cgl-client> -DCASE=${CASE} ${startup_options}
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/map_startup.cmake)
        set_tests_properties(cgl_client_map_${CASE} PROPERTIES
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR} TIMEOUT ${startup_test_timeout} LABELS "desktop;assets;integration" RESOURCE_LOCK cgl_desktop)
    endforeach()
    add_test(NAME cgl_client_startup
        COMMAND ${CMAKE_COMMAND}
            -DCLIENT=$<TARGET_FILE:cgl-client> ${startup_options}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/startup.cmake)
    set_tests_properties(cgl_client_startup PROPERTIES
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR} TIMEOUT ${startup_test_timeout} LABELS "desktop;assets;integration" RESOURCE_LOCK cgl_desktop)
    add_test(NAME cgl_client_invalid_resources
        COMMAND ${CMAKE_COMMAND}
            -DCLIENT=$<TARGET_FILE:cgl-client> ${startup_options}
            -DINVALID_RESOURCES=ON
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/startup.cmake)
    set_tests_properties(cgl_client_invalid_resources PROPERTIES
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR} TIMEOUT ${startup_test_timeout} LABELS "desktop;assets;integration" RESOURCE_LOCK cgl_desktop)
    foreach(CASE direction serial scale)
        add_test(NAME cgl_client_invalid_animation_${CASE}
            COMMAND ${CMAKE_COMMAND}
                -DCLIENT=$<TARGET_FILE:cgl-client> ${startup_options}
                -DINVALID_ANIMATION=${CASE}
                -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/startup.cmake)
        set_tests_properties(cgl_client_invalid_animation_${CASE} PROPERTIES
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR} TIMEOUT ${startup_test_timeout} LABELS "desktop;assets;integration" RESOURCE_LOCK cgl_desktop)
    endforeach()
endif()

# Runnable local snapshot demo; external game assets remain user-provided.
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/configs/server-demo100.ini
    ${CGL_BIN_PATH}/server-demo100.ini COPYONLY)
