add_library(xp_core STATIC
    src/error.cpp
    src/path.cpp
    src/storage_paths.cpp
    src/storage_context.cpp
    src/file_store.cpp
    src/config_file.cpp
    src/directory_store.cpp
    src/redaction.cpp
    src/logger.cpp
    src/text.cpp
    src/url.cpp
)
target_include_directories(xp_core PUBLIC include)
target_compile_features(xp_core PUBLIC cxx_std_17)
set_target_properties(xp_core PROPERTIES CXX_EXTENSIONS OFF)

# Platform capability is supplied by the XP-TUBE parent build; this native
# module does not depend on migration-project compile definitions.
if(XP_TUBE_PLATFORM_PS4)
    target_compile_definitions(xp_core PRIVATE XP_TUBE_PLATFORM_PS4=1)
endif()

if(BUILD_TESTING)
    add_executable(xp_core_result_tests tests/result_test.cpp)
    target_link_libraries(xp_core_result_tests PRIVATE xp_core)
    add_test(NAME xp_core_result_unit COMMAND xp_core_result_tests)

    add_executable(xp_core_path_tests tests/path_test.cpp)
    target_link_libraries(xp_core_path_tests PRIVATE xp_core)
    add_test(NAME xp_core_path_unit COMMAND xp_core_path_tests)

    add_executable(xp_core_storage_paths_tests tests/storage_paths_test.cpp)
    target_link_libraries(xp_core_storage_paths_tests PRIVATE xp_core)
    add_test(NAME xp_core_storage_paths_unit COMMAND xp_core_storage_paths_tests)

    add_executable(xp_core_storage_context_tests tests/storage_context_test.cpp)
    target_link_libraries(xp_core_storage_context_tests PRIVATE xp_core)
    add_test(NAME xp_core_storage_context_unit COMMAND xp_core_storage_context_tests)

    add_executable(xp_core_directory_store_tests tests/directory_store_test.cpp)
    target_link_libraries(xp_core_directory_store_tests PRIVATE xp_core)
    add_test(NAME xp_core_directory_store_unit COMMAND xp_core_directory_store_tests)

    add_executable(xp_core_config_file_tests tests/config_file_test.cpp)
    target_link_libraries(xp_core_config_file_tests PRIVATE xp_core)
    add_test(NAME xp_core_config_file_unit COMMAND xp_core_config_file_tests)

    add_executable(xp_core_file_store_tests tests/file_store_test.cpp)
    target_link_libraries(xp_core_file_store_tests PRIVATE xp_core)
    add_test(NAME xp_core_file_store_unit COMMAND xp_core_file_store_tests)

    add_executable(xp_core_redaction_tests tests/redaction_test.cpp)
    target_link_libraries(xp_core_redaction_tests PRIVATE xp_core)
    add_test(NAME xp_core_redaction_unit COMMAND xp_core_redaction_tests)

    add_executable(xp_core_logger_tests tests/logger_test.cpp)
    target_link_libraries(xp_core_logger_tests PRIVATE xp_core)
    add_test(NAME xp_core_logger_unit COMMAND xp_core_logger_tests)

    add_executable(xp_core_text_tests tests/text_test.cpp)
    target_link_libraries(xp_core_text_tests PRIVATE xp_core)
    add_test(NAME xp_core_text_unit COMMAND xp_core_text_tests)

    add_executable(xp_core_url_tests tests/url_test.cpp)
    target_link_libraries(xp_core_url_tests PRIVATE xp_core)
    add_test(NAME xp_core_url_unit COMMAND xp_core_url_tests)
endif()
