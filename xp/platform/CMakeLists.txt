add_library(xp_platform STATIC
    src/event_queue.cpp
    src/runtime.cpp
    src/legacy_action.cpp
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

    add_executable(xp_platform_legacy_action_tests tests/legacy_action_test.cpp)
    target_link_libraries(xp_platform_legacy_action_tests PRIVATE xp_platform)
    add_test(NAME xp_platform_legacy_action_unit COMMAND xp_platform_legacy_action_tests)
endif()

# The OpenOrbis system-service adapter is compiled only for PS4 builds.
if(XP_TUBE_PLATFORM_PS4)
    target_sources(xp_platform PRIVATE src/ps4/system_services.cpp)
endif()
