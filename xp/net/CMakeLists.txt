add_library(xp_net STATIC
    src/http_contract.cpp
    src/curl_transport.cpp
    src/certificate.cpp
)
target_include_directories(xp_net PUBLIC include)
target_compile_features(xp_net PUBLIC cxx_std_17)
target_link_libraries(xp_net PUBLIC xp_core PRIVATE playtube_curl playtube_crypto)
set_target_properties(xp_net PROPERTIES CXX_EXTENSIONS OFF)
if(PLATFORM_PS4)
    target_compile_definitions(xp_net PRIVATE PLAYTUBE_PS4=1)
endif()

if(BUILD_TESTING)
    add_executable(xp_net_http_contract_tests tests/http_contract_test.cpp)
    target_link_libraries(xp_net_http_contract_tests PRIVATE xp_net xp_core)
    add_test(NAME xp_net_http_contract_unit COMMAND xp_net_http_contract_tests)
endif()
