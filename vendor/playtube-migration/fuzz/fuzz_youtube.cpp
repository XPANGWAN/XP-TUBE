#include <cstddef>
#include <cstdint>
#include <string_view>

#include "youtube/links.hpp"
#include "youtube/parse.hpp"

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    using namespace playtube::youtube;
    const std::string_view text(reinterpret_cast<const char*>(data), size);
    (void)parseSearch(text);
    (void)parseChannel(text);
    (void)parseChannelMore(text);
    (void)parseNext(text);
    (void)parsePlayer(text);
    (void)parseTiles(text);
    (void)parseComments(text);
    (void)parseVideoState(text);
    (void)parseAddToPlaylist(text);
    (void)parseChannelPlaylists(text);
    (void)parseSuggestions(text);
    (void)parseSegments(text);
    (void)parseVisitorData(text);
    (void)parseAccountName(text);
    (void)parseInvidious(text, "https://invidious.example");
    (void)parseDeviceCode(text);
    (void)parseToken(text);
    (void)parseDuration(text);
    (void)videoIdFrom(text);
    return 0;
}
