add_library(xp_platform STATIC
    src/event_queue.cpp
    src/runtime.cpp
    src/legacy_action.cpp
    src/language.cpp
    src/controller_registry.cpp
    src/controller_input.cpp
    src/display.cpp
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

    add_executable(xp_platform_language_tests tests/language_test.cpp)
    target_link_libraries(xp_platform_language_tests PRIVATE xp_platform)
    add_test(NAME xp_platform_language_unit COMMAND xp_platform_language_tests)

    add_executable(xp_platform_controller_registry_tests tests/controller_registry_test.cpp)
    target_link_libraries(xp_platform_controller_registry_tests PRIVATE xp_platform)
    add_test(NAME xp_platform_controller_registry_unit COMMAND xp_platform_controller_registry_tests)

    add_executable(xp_platform_controller_input_tests tests/controller_input_test.cpp)
    target_link_libraries(xp_platform_controller_input_tests PRIVATE xp_platform)
    add_test(NAME xp_platform_controller_input_unit COMMAND xp_platform_controller_input_tests)

    add_executable(xp_platform_display_tests tests/display_test.cpp)
    target_link_libraries(xp_platform_display_tests PRIVATE xp_platform)
    add_test(NAME xp_platform_display_unit COMMAND xp_platform_display_tests)

    add_executable(xp_platform_audio_focus_tests tests/audio_focus_test.cpp)
    target_link_libraries(xp_platform_audio_focus_tests PRIVATE xp_platform)
    add_test(NAME xp_platform_audio_focus_unit COMMAND xp_platform_audio_focus_tests)
endif()

if(XP_TUBE_PLATFORM_PS4)
    target_sources(xp_platform PRIVATE src/ps4/system_services.cpp)
endif()
