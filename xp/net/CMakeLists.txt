add_library(xp_net INTERFACE)
target_include_directories(xp_net INTERFACE include)
target_compile_features(xp_net INTERFACE cxx_std_17)

if(BUILD_TESTING)
    add_executable(xp_net_http_contract_tests tests/http_contract_test.cpp)
    target_link_libraries(xp_net_http_contract_tests PRIVATE xp_net xp_core)
    add_test(NAME xp_net_http_contract_unit COMMAND xp_net_http_contract_tests)
endif()
