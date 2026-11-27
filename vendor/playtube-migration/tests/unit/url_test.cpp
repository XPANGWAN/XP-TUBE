#include <doctest/doctest.h>

#include <string>
#include <vector>

#include "net/url.hpp"

using playtube::net::parseUrl;

TEST_CASE("parseUrl normalises scheme, host, port and path") {
    const auto url = parseUrl("  HTTPS://Media.Example.com:8920/Watch/  ");
    REQUIRE(url.has_value());
    CHECK(url->scheme == "https");
    CHECK(url->host == "media.example.com");
    CHECK(url->port == 8920);
    CHECK(url->path == "/Watch");
    CHECK(playtube::net::toString(*url) == "https://media.example.com:8920/Watch");
    CHECK(playtube::net::origin(*url) == "https://media.example.com:8920");
    const auto v6 = parseUrl("http://[::1]:8096");
    REQUIRE(v6.has_value());
    CHECK(v6->host == "[::1]");
    CHECK(v6->port == 8096);
}

TEST_CASE("parseUrl rejects what is not a plain http(s) server address") {
    for (const char* bad : {"", "ftp://host", "http://", "http://user:pw@host", "http://host:0", "http://host:70000",
                            "http://host:abc", "http://ho st", "http://host/?q=1", "http://host#x", "http://[::1",
                            "http://host:", "javascript:alert(1)", "http://ho\\st", "http://[zz]"}) {
        INFO(bad);
        CHECK_FALSE(parseUrl(bad).has_value());
    }
}

TEST_CASE("urlEncode and Query") {
    CHECK(playtube::net::urlEncode("a b&c=d/\xC3\xA9~_.-") == "a%20b%26c%3Dd%2F%C3%A9~_.-");
    CHECK(playtube::net::Query().add("userId", "u 1").add("limit", 20).str() == "?userId=u%201&limit=20");
    CHECK(playtube::net::Query().str().empty());
}
