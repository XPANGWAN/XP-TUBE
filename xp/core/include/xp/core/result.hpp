#pragma once

#include "xp/core/error.hpp"

#include <optional>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <variant>

namespace xp::core {

template <typename T>
class Result {
    static_assert(!std::is_reference<T>::value, "Result cannot contain references");

public:
    static Result success(T value) {
        return Result(std::in_place_index<0>, std::move(value));
    }

    static Result failure(Error error) {
        return Result(std::in_place_index<1>, std::move(error));
    }

    bool has_value() const noexcept { return data_.index() == 0; }
    explicit operator bool() const noexcept { return has_value(); }

    T& value() & {
        if (!has_value()) throw std::logic_error("Result has no value");
        return std::get<0>(data_);
    }

    const T& value() const& {
        if (!has_value()) throw std::logic_error("Result has no value");
        return std::get<0>(data_);
    }

    T&& value() && {
        if (!has_value()) throw std::logic_error("Result has no value");
        return std::move(std::get<0>(data_));
    }

    const Error& error() const& {
        if (has_value()) throw std::logic_error("Result has no error");
        return std::get<1>(data_);
    }

private:
    template <typename... Args>
    explicit Result(std::in_place_index_t<0> tag, Args&&... args)
        : data_(tag, std::forward<Args>(args)...) {}

    template <typename... Args>
    explicit Result(std::in_place_index_t<1> tag, Args&&... args)
        : data_(tag, std::forward<Args>(args)...) {}

    std::variant<T, Error> data_;
};

template <>
class Result<void> {
public:
    static Result success() { return Result(); }
    static Result failure(Error error) { return Result(std::move(error)); }

    bool has_value() const noexcept { return !error_.has_value(); }
    explicit operator bool() const noexcept { return has_value(); }

    const Error& error() const {
        if (has_value()) throw std::logic_error("Result has no error");
        return *error_;
    }

private:
    Result() = default;
    explicit Result(Error error) : error_(std::move(error)) {}

    std::optional<Error> error_;
};

} // namespace xp::core
