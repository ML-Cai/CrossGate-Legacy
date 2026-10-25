include_guard(GLOBAL)
include(GoogleTest)

# The desktop and SDK-free test projects compile these same production sources.
function(cgl_add_headless_core_tests root output_dir)
    find_package(Threads REQUIRED)
    foreach(name IN ITEMS character_catalog idle_resource_cache animation_decode_worker scene_animation_load appearance_scheduler appearance_demand_telemetry map_builder map_decode_worker)
        set(sources "${root}/engines/tests/test_${name}.cpp")
        if(name STREQUAL "animation_decode_worker")
            list(APPEND sources "${root}/engines/src/assets/animation_decode_worker.cpp")
        endif()
        if(name STREQUAL "map_builder")
            list(APPEND sources "${root}/engines/src/assets/map_builder.cpp")
        endif()
        if(name STREQUAL "map_decode_worker")
            list(APPEND sources "${root}/engines/src/assets/map_decode_worker.cpp" "${root}/engines/src/assets/map_builder.cpp")
        endif()
        add_executable(test_${name} ${sources})
        target_include_directories(test_${name} PRIVATE
            "${root}/engines/src" "${root}/modules/common/include" "${root}/modules/graphics/include"
            "${root}/modules/assets/include")
        target_link_libraries(test_${name} PRIVATE GTest::gtest_main Threads::Threads)
        set_target_properties(test_${name} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${output_dir}")
        gtest_discover_tests(test_${name} WORKING_DIRECTORY "${output_dir}"
            PROPERTIES LABELS "unit\;memory\;headless" TIMEOUT 60)
    endforeach()
endfunction()
