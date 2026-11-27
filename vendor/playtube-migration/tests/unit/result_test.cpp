#include <doctest/doctest.h>

#include <string>

#include "core/result.hpp"

using playtube::core::Error;
using playtube::core::ErrorKind;
using playtube::core::Result;

TEST_CASE("Result holds a value or an error") {
    Result<int> good(5);
    CHECK(good.ok());
    CHECK(good.value() == 5);
    Result<int> bad(playtube::core::makeError(ErrorKind::NotFound, "missing", 404));
    CHECK_FALSE(bad.ok());
    CHECK(bad.error().kind == ErrorKind::NotFound);
    CHECK(bad.error().httpStatus == 404);
    CHECK(bad.error().message == "missing");
}

TEST_CASE("Result moves its value out") {
    Result<std::string> text(std::string("hello"));
    std::string taken = std::move(text.value());
    CHECK(taken == "hello");
}

TEST_CASE("error kinds have names") {
    CHECK(std::string(playtube::core::errorKindName(ErrorKind::CertificateUntrusted)) == "CertificateUntrusted");
    CHECK(std::string(playtube::core::errorKindName(ErrorKind::Storage)) == "Storage");
}
