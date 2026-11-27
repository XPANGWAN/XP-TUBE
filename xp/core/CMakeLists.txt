add_library(xp_core STATIC
    src/error.cpp
)
target_include_directories(xp_core PUBLIC include)
target_compile_features(xp_core PUBLIC cxx_std_17)
set_target_properties(xp_core PROPERTIES CXX_EXTENSIONS OFF)

if(BUILD_TESTING)
    add_executable(xp_core_tests tests/result_test.cpp)
    target_link_libraries(xp_core_tests PRIVATE xp_core)
    add_test(NAME xp_core_unit COMMAND xp_core_tests)
endif()
