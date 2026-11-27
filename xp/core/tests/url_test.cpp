#include "xp/core/url.hpp"

#include <cassert>
#include <string>

int main() {
    using namespace xp::core::url;

    const auto parsed = parse("  HTTPS://Media.Example.com:8920/Watch/  ");
    assert(parsed);
    assert(parsed->scheme == "https");
    assert(parsed->host == "media.example.com");
    assert(parsed->port == 8920);
    assert(parsed->path == "/Watch");
    assert(to_string(*parsed) == "https://media.example.com:8920/Watch");
    assert(origin(*parsed) == "https://media.example.com:8920");

    const auto ipv6 = parse("http://[::1]:8096");
    assert(ipv6 && ipv6->host == "[::1]" && ipv6->port == 8096);

    for (const char* bad : {"", "ftp://host", "http://", "http://user:pw@host",
         "http://host:0", "http://host:70000", "http://host:abc", "http://ho st",
         "http://host/?q=1", "http://host#x", "http://[::1", "http://host:",
         "javascript:alert(1)", "http://ho\\st", "http://[zz]"}) {
        assert(!parse(bad));
    }

    assert(encode("a b&c=d/\xC3\xA9~_.-") == "a%20b%26c%3Dd%2F%C3%A9~_.-");
    assert(Query().add("userId", "u 1").add("limit", 20).str() == "?userId=u%201&limit=20");
    assert(Query().str().empty());
}
