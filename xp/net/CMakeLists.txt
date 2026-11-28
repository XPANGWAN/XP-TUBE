add_library(xp_net STATIC
    src/http_contract.cpp
    src/curl_transport.cpp
    src/certificate.cpp
)
target_include_directories(xp_net PUBLIC include)
target_compile_features(xp_net PUBLIC cxx_std_17)
target_link_libraries(xp_net PUBLIC xp_core)

# XP Network consumes platform capabilities through explicit CMake target
# inputs. It does not know or depend on the names of the migration project's
# targets. A standalone host build discovers the same system libraries itself.
if(DEFINED XP_TUBE_CURL_TARGET AND DEFINED XP_TUBE_CRYPTO_TARGET)
    if(TARGET ${XP_TUBE_CURL_TARGET} AND TARGET ${XP_TUBE_CRYPTO_TARGET})
        target_link_libraries(xp_net PRIVATE
            ${XP_TUBE_CURL_TARGET}
            ${XP_TUBE_CRYPTO_TARGET}
        )
    else()
        message(FATAL_ERROR "XP-TUBE network dependency targets were supplied but are not available")
    endif()
else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(CURL REQUIRED IMPORTED_TARGET libcurl)
    pkg_check_modules(LIBCRYPTO REQUIRED IMPORTED_TARGET libcrypto)
    target_link_libraries(xp_net PRIVATE PkgConfig::CURL PkgConfig::LIBCRYPTO)
endif()

set_target_properties(xp_net PROPERTIES CXX_EXTENSIONS OFF)
if(XP_TUBE_PLATFORM_PS4)
    target_compile_definitions(xp_net PRIVATE XP_TUBE_PLATFORM_PS4=1)
endif()

if(BUILD_TESTING)
    add_executable(xp_net_http_contract_tests tests/http_contract_test.cpp)
    target_link_libraries(xp_net_http_contract_tests PRIVATE xp_net xp_core)
    add_test(NAME xp_net_http_contract_unit COMMAND xp_net_http_contract_tests)
endif()
