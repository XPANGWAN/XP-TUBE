#include "xp/core/result.hpp"

#include <cassert>
#include <string>
#include <utility>

int main() {
    using xp::core::Error;
    using xp::core::ErrorCode;
    using xp::core::Result;

    auto value = Result<int>::success(42);
    assert(value.has_value());
    assert(static_cast<bool>(value));
    assert(value.value() == 42);

    auto failure = Result<int>::failure(
        Error::make(ErrorCode::network, "request failed", "unit-test"));
    assert(!failure.has_value());
    assert(failure.error().code == ErrorCode::network);
    assert(failure.error().message == "request failed");
    assert(failure.error().context == "unit-test");

    auto moved = Result<std::string>::success(std::string("XP-TUBE"));
    assert(std::move(moved).value() == "XP-TUBE");

    assert(ErrorCode::timeout != ErrorCode::network);
    assert(ErrorCode::tls != ErrorCode::certificate_untrusted);
    assert(ErrorCode::bad_response != ErrorCode::storage);

    auto no_value = Result<void>::success();
    assert(no_value.has_value());

    auto no_success = Result<void>::failure(
        Error::make(ErrorCode::cancelled, "cancelled"));
    assert(!no_success.has_value());
    assert(no_success.error().code == ErrorCode::cancelled);
}
