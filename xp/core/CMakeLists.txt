add_library(xp_core STATIC
    src/error.cpp
    src/path.cpp
    src/storage_paths.cpp
    src/file_store.cpp
)
target_include_directories(xp_core PUBLIC include)
target_compile_features(xp_core PUBLIC cxx_std_17)
set_target_properties(xp_core PROPERTIES CXX_EXTENSIONS OFF)

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

    add_executable(xp_core_file_store_tests tests/file_store_test.cpp)
    target_link_libraries(xp_core_file_store_tests PRIVATE xp_core)
    add_test(NAME xp_core_file_store_unit COMMAND xp_core_file_store_tests)
endif()
