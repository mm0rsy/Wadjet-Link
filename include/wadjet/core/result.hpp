#pragma once

/// @file result.hpp
/// @brief Result type for error handling (similar to Rust's Result<T, E>)

#include <optional>
#include <string>
#include <variant>

namespace wadjet {

/// @brief Error information
struct Error {
    int code{0};
    std::string message;

    Error() = default;
    explicit Error(std::string msg) : message(std::move(msg)) {}
    Error(int c, std::string msg) : code(c), message(std::move(msg)) {}

    [[nodiscard]] bool operator==(const Error& other) const {
        return code == other.code && message == other.message;
    }
};

/// @brief Result type for operations that can fail
/// @tparam T Success value type
/// @tparam E Error type (defaults to Error)
template <typename T, typename E = Error>
class Result {
public:
    /// @brief Construct a success result
    static Result ok(T value) { return Result(std::move(value)); }

    /// @brief Construct an error result
    static Result err(E error) { return Result(std::move(error)); }

    /// @brief Check if result is success
    [[nodiscard]] bool is_ok() const { return std::holds_alternative<T>(data_); }

    /// @brief Check if result is error
    [[nodiscard]] bool is_err() const { return std::holds_alternative<E>(data_); }

    /// @brief Get the success value (undefined if is_err())
    [[nodiscard]] T& value() & { return std::get<T>(data_); }
    [[nodiscard]] const T& value() const& { return std::get<T>(data_); }
    [[nodiscard]] T&& value() && { return std::get<T>(std::move(data_)); }

    /// @brief Get the error (undefined if is_ok())
    [[nodiscard]] E& error() & { return std::get<E>(data_); }
    [[nodiscard]] const E& error() const& { return std::get<E>(data_); }
    [[nodiscard]] E&& error() && { return std::get<E>(std::move(data_)); }

    /// @brief Get value or default
    [[nodiscard]] T value_or(T default_value) const& {
        if (is_ok()) {
            return value();
        }
        return default_value;
    }

    /// @brief Boolean conversion (true if ok)
    explicit operator bool() const { return is_ok(); }

    /// @brief Arrow operator for accessing value members
    T* operator->() { return &value(); }
    const T* operator->() const { return &value(); }

    /// @brief Dereference operator
    T& operator*() & { return value(); }
    const T& operator*() const& { return value(); }
    T&& operator*() && { return std::move(value()); }

private:
    explicit Result(T value) : data_(std::move(value)) {}
    explicit Result(E error) : data_(std::move(error)) {}

    std::variant<T, E> data_;
};

/// @brief Specialization for void success type
template <typename E>
class Result<void, E> {
public:
    static Result ok() { return Result(true); }
    static Result err(E error) { return Result(std::move(error)); }

    [[nodiscard]] bool is_ok() const { return !error_.has_value(); }
    [[nodiscard]] bool is_err() const { return error_.has_value(); }

    [[nodiscard]] E& error() & { return *error_; }
    [[nodiscard]] const E& error() const& { return *error_; }

    explicit operator bool() const { return is_ok(); }

private:
    explicit Result(bool) : error_(std::nullopt) {}
    explicit Result(E error) : error_(std::move(error)) {}

    std::optional<E> error_;
};

}  // namespace wadjet
