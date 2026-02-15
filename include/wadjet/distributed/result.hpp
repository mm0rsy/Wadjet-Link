#pragma once

#include <optional>
#include <stdexcept>
#include <string>

namespace wadjet::distributed {

/**
 * @brief Error type for distributed testing operations
 */
struct Error {
    std::string code;     ///< Error code (e.g., "TIMEOUT", "NOT_FOUND")
    std::string message;  ///< Human-readable error message

    /// Create an error with the given code and message
    static Error make(std::string_view code_param, std::string_view msg) {
        return Error{std::string(code_param), std::string(msg)};
    }
};

/**
 * @brief Result type for operations that can fail
 *
 * Represents either a successful value of type T or an error.
 * Similar to Rust's Result<T, E> or C++'s std::expected.
 *
 * @tparam T The type of the successful value
 */
template <typename T>
class Result {
public:
    /// Create a successful result with the given value
    explicit Result(T value) : value_(std::move(value)), error_(std::nullopt) {}

    /// Create a failed result with the given error
    explicit Result(Error err) : value_(std::nullopt), error_(std::move(err)) {}

    /// Check if the result is successful
    auto is_ok() const -> bool { return value_.has_value(); }

    /// Check if the result is an error
    auto is_err() const -> bool { return error_.has_value(); }

    /// Boolean conversion operator (true if successful)
    explicit operator bool() const { return is_ok(); }

    /// Get the value, throwing if it's an error
    auto unwrap() -> T& {
        if (!value_.has_value()) {
            throw std::runtime_error(error_->message);
        }
        return *value_;
    }

    /// Get the const value, throwing if it's an error
    auto unwrap() const -> const T& {
        if (!value_.has_value()) {
            throw std::runtime_error(error_->message);
        }
        return *value_;
    }

    /// Get the value or return a default
    auto unwrap_or(T default_value) const -> T {
        return value_.has_value() ? *value_ : default_value;
    }

    /// Get the error, throwing if it's successful
    auto unwrap_err() -> Error& {
        if (value_.has_value()) {
            throw std::runtime_error("Called unwrap_err on successful result");
        }
        return *error_;
    }

    /// Get the const error, throwing if it's successful
    auto unwrap_err() const -> const Error& {
        if (value_.has_value()) {
            throw std::runtime_error("Called unwrap_err on successful result");
        }
        return *error_;
    }

    /// Get the value if present, nullopt if error
    auto ok() const -> std::optional<T> { return value_; }

    /// Get the error if present, nullopt if successful
    auto err() const -> std::optional<Error> { return error_; }

    /// Get the value (non-const reference)
    auto value() -> T& {
        if (!value_.has_value()) {
            throw std::runtime_error("Called value() on failed result");
        }
        return *value_;
    }

    /// Get the value (const reference)
    auto value() const -> const T& {
        if (!value_.has_value()) {
            throw std::runtime_error("Called value() on failed result");
        }
        return *value_;
    }

    /// Get the error (non-const reference)
    auto error() -> Error& {
        if (!error_.has_value()) {
            throw std::runtime_error("Called error() on successful result");
        }
        return *error_;
    }

    /// Get the error (const reference)
    auto error() const -> const Error& {
        if (!error_.has_value()) {
            throw std::runtime_error("Called error() on successful result");
        }
        return *error_;
    }

private:
    std::optional<T> value_;
    std::optional<Error> error_;
};

/**
 * @brief Specialization for void results
 *
 * Represents either success (void) or an error.
 */
template <>
class Result<void> {
public:
    /// Create a successful result
    Result() : error_(std::nullopt) {}

    /// Create a failed result with the given error
    explicit Result(Error err) : error_(std::move(err)) {}

    /// Check if the result is successful
    auto is_ok() const -> bool { return !error_.has_value(); }

    /// Check if the result is an error
    auto is_err() const -> bool { return error_.has_value(); }

    /// Throw if the result is an error
    auto unwrap() const {
        if (error_.has_value()) {
            throw std::runtime_error(error_->message);
        }
    }

    /// Get the error, throwing if successful
    auto unwrap_err() -> Error& {
        if (!error_.has_value()) {
            throw std::runtime_error("Called unwrap_err on successful result");
        }
        return *error_;
    }

    /// Get the const error, throwing if successful
    auto unwrap_err() const -> const Error& {
        if (!error_.has_value()) {
            throw std::runtime_error("Called unwrap_err on successful result");
        }
        return *error_;
    }

    /// Get the error if present, nullopt if successful
    auto err() const -> std::optional<Error> { return error_; }

private:
    std::optional<Error> error_;
};

}  // namespace wadjet::distributed
