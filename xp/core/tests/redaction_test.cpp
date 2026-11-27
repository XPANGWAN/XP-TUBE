#include "xp/core/redaction.hpp"

#include <cstdlib>
#include <iostream>
#include <string>

namespace {
void expect(const std::string& input, const std::string& expected) {
    const auto actual = xp::core::redact_secrets(input);
    if (actual != expected) {
        std::cerr << "expected: " << expected << "\nactual:   " << actual << "\n";
        std::exit(1);
    }
}
}
int main() {
    expect("plain diagnostic", "plain diagnostic");
    expect("token=abc123&next=yes", "token=***&next=yes");
    expect("Authorization: Bearer abc123\nnext", "Authorization: ***\nnext");
    expect("{\"access_token\": \"secret-value\", \"ok\": true}",
           "{\"access_token\": \"***\", \"ok\": true}");
    expect("https://host/ip/private-value/master.m3u8",
           "https://host/ip/***/master.m3u8");
    expect("PASSWORD=hunter2", "PASSWORD=***");
    expect("token=\"quoted-secret\" tail", "token=\"***\" tail");
    return 0;
}
