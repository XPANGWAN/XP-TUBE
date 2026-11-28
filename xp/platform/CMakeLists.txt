add_library(xp_platform STATIC
    src/event_queue.cpp
    src/runtime.cpp
)
target_include_directories(xp_platform PUBLIC include)
target_compile_features(xp_platform PUBLIC cxx_std_17)
target_link_libraries(xp_platform PUBLIC xp_core)
set_target_properties(xp_platform PROPERTIES CXX_EXTENSIONS OFF)

if(BUILD_TESTING)
    add_executable(xp_platform_event_queue_tests tests/event_queue_test.cpp)
    target_link_libraries(xp_platform_event_queue_tests PRIVATE xp_platform)
    add_test(NAME xp_platform_event_queue_unit COMMAND xp_platform_event_queue_tests)

    add_executable(xp_platform_runtime_tests tests/runtime_test.cpp)
    target_link_libraries(xp_platform_runtime_tests PRIVATE xp_platform)
    add_test(NAME xp_platform_runtime_unit COMMAND xp_platform_runtime_tests)
endif()
