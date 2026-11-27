add_library(xp_net STATIC
    src/http_contract.cpp
    src/curl_transport.cpp
    src/certificate.cpp
)
target_include_directories(xp_net PUBLIC include)
target_compile_features(xp_net PUBLIC cxx_std_17)
target_link_libraries(xp_net PUBLIC xp_core)
if(TARGET playtube_curl AND TARGET playtube_crypto)
    target_link_libraries(xp_net PRIVATE playtube_curl playtube_crypto)
else()
    # Standalone XP-native build (outside the PS4 application project).
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(CURL REQUIRED IMPORTED_TARGET libcurl)
    pkg_check_modules(LIBCRYPTO REQUIRED IMPORTED_TARGET libcrypto)
    target_link_libraries(xp_net PRIVATE PkgConfig::CURL PkgConfig::LIBCRYPTO)
endif()
set_target_properties(xp_net PROPERTIES CXX_EXTENSIONS OFF)
if(PLATFORM_PS4)
    target_compile_definitions(xp_net PRIVATE PLAYTUBE_PS4=1)
endif()

if(BUILD_TESTING)
    add_executable(xp_net_http_contract_tests tests/http_contract_test.cpp)
    target_link_libraries(xp_net_http_contract_tests PRIVATE xp_net xp_core)
    add_test(NAME xp_net_http_contract_unit COMMAND xp_net_http_contract_tests)
endif()
