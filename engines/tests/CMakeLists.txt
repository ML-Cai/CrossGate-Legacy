include(GoogleTest)
add_executable(test_scene_lifecycle test_scene_lifecycle.cpp)
target_include_directories(test_scene_lifecycle PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(test_scene_lifecycle PRIVATE GTest::gtest_main cgl::engine cgl::graphics cgl::assets cgl::settings cgl::common cgl::trace Vulkan::Vulkan glfw)
set_target_properties(test_scene_lifecycle PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CGL_BIN_PATH})
gtest_discover_tests(test_scene_lifecycle WORKING_DIRECTORY ${CGL_BIN_PATH}
    PROPERTIES LABELS "desktop\;assets\;memory" TIMEOUT 60 RESOURCE_LOCK cgl_desktop)

add_executable(test_engine_animation test_animation.cpp)
target_include_directories(test_engine_animation PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(test_engine_animation PRIVATE GTest::gtest_main cgl::engine cgl::graphics cgl::assets cgl::settings cgl::common)
set_target_properties(test_engine_animation PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CGL_BIN_PATH})
gtest_discover_tests(test_engine_animation WORKING_DIRECTORY ${CGL_BIN_PATH}
    PROPERTIES LABELS "unit" TIMEOUT 60)

add_executable(test_map_camera test_map_camera.cpp)
target_include_directories(test_map_camera PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(test_map_camera PRIVATE GTest::gtest_main cgl::common cgl::assets)
set_target_properties(test_map_camera PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CGL_BIN_PATH})
gtest_discover_tests(test_map_camera WORKING_DIRECTORY ${CGL_BIN_PATH}
    PROPERTIES LABELS "unit" TIMEOUT 60)

add_executable(test_map_render_data test_map_render_data.cpp)
target_include_directories(test_map_render_data PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(test_map_render_data PRIVATE GTest::gtest_main cgl::graphics cgl::common)
set_target_properties(test_map_render_data PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CGL_BIN_PATH})
gtest_discover_tests(test_map_render_data WORKING_DIRECTORY ${CGL_BIN_PATH}
    PROPERTIES LABELS "unit" TIMEOUT 60)

add_executable(test_map_hover test_map_hover.cpp)
target_include_directories(test_map_hover PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(test_map_hover PRIVATE GTest::gtest_main cgl::graphics cgl::assets cgl::common)
set_target_properties(test_map_hover PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CGL_BIN_PATH})
gtest_discover_tests(test_map_hover WORKING_DIRECTORY ${CGL_BIN_PATH}
    PROPERTIES LABELS "unit" TIMEOUT 60)

# CPU-only assembly: deliberately links no renderer, Vulkan, or window library.
add_executable(test_animation_builder test_animation_builder.cpp ../src/assets/animation_builder.cpp)
target_include_directories(test_animation_builder PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(test_animation_builder PRIVATE GTest::gtest_main Threads::Threads cgl::assets cgl::settings cgl::common)
set_target_properties(test_animation_builder PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CGL_BIN_PATH})
gtest_discover_tests(test_animation_builder WORKING_DIRECTORY ${CGL_BIN_PATH}
    PROPERTIES LABELS "unit\;memory" TIMEOUT 60)

add_executable(test_crowd_benchmark test_crowd_benchmark.cpp)
target_include_directories(test_crowd_benchmark PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(test_crowd_benchmark PRIVATE GTest::gtest_main cgl::settings cgl::common)
set_target_properties(test_crowd_benchmark PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CGL_BIN_PATH})
gtest_discover_tests(test_crowd_benchmark WORKING_DIRECTORY ${CGL_BIN_PATH}
    PROPERTIES LABELS "unit" TIMEOUT 60)

# Background decode scheduling/lifetime tests have no window or Vulkan dependency.
add_executable(test_animation_decode_worker test_animation_decode_worker.cpp ../src/assets/animation_decode_worker.cpp)
target_include_directories(test_animation_decode_worker PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(test_animation_decode_worker PRIVATE GTest::gtest_main Threads::Threads cgl::common)
set_target_properties(test_animation_decode_worker PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CGL_BIN_PATH})
gtest_discover_tests(test_animation_decode_worker WORKING_DIRECTORY ${CGL_BIN_PATH}
    PROPERTIES LABELS "unit\;memory" TIMEOUT 60)

add_executable(test_local_character_feed test_local_character_feed.cpp)
target_include_directories(test_local_character_feed PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(test_local_character_feed PRIVATE GTest::gtest_main cgl::common)
set_target_properties(test_local_character_feed PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CGL_BIN_PATH})
gtest_discover_tests(test_local_character_feed WORKING_DIRECTORY ${CGL_BIN_PATH}
    PROPERTIES LABELS "unit" TIMEOUT 60)

# Recording test doubles exercise rendering boundaries without a device/window.
add_executable(test_map_world test_map_world.cpp)
target_include_directories(test_map_world PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(test_map_world PRIVATE GTest::gtest_main cgl::engine cgl::graphics cgl::assets cgl::settings cgl::common)
set_target_properties(test_map_world PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CGL_BIN_PATH})
gtest_discover_tests(test_map_world WORKING_DIRECTORY ${CGL_BIN_PATH}
    PROPERTIES LABELS "unit\;memory" TIMEOUT 60)

add_executable(test_frame_telemetry test_frame_telemetry.cpp)
target_include_directories(test_frame_telemetry PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(test_frame_telemetry PRIVATE GTest::gtest_main cgl::settings cgl::common)
set_target_properties(test_frame_telemetry PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CGL_BIN_PATH})
gtest_discover_tests(test_frame_telemetry WORKING_DIRECTORY ${CGL_BIN_PATH}
    PROPERTIES LABELS "unit\;memory" TIMEOUT 60)

add_executable(test_idle_resource_cache test_idle_resource_cache.cpp)
target_include_directories(test_idle_resource_cache PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(test_idle_resource_cache PRIVATE GTest::gtest_main)
set_target_properties(test_idle_resource_cache PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CGL_BIN_PATH})
gtest_discover_tests(test_idle_resource_cache WORKING_DIRECTORY ${CGL_BIN_PATH}
    PROPERTIES LABELS "unit\;memory" TIMEOUT 60)
