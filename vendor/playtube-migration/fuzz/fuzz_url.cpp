#include <cstddef>
#include <cstdint>
#include <string_view>

#include "net/url.hpp"

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    const std::string_view text(reinterpret_cast<const char*>(data), size);
    if (const auto url = playtube::net::parseUrl(text)) {
        (void)playtube::net::toString(*url);
    }
    (void)playtube::net::urlEncode(text);
    return 0;
}
