#include <gtest/gtest.h>
#include <wadjet/core/result.hpp>

namespace wadjet::test {

TEST(Result, OkValue) {
    auto result = Result<int>::ok(42);
    EXPECT_TRUE(result.is_ok());
    EXPECT_FALSE(result.is_err());
    EXPECT_EQ(result.value(), 42);
    EXPECT_EQ(*result, 42);
    EXPECT_TRUE(static_cast<bool>(result));
}

TEST(Result, ErrValue) {
    auto result = Result<int>::err(Error{-1, "test error"});
    EXPECT_FALSE(result.is_ok());
    EXPECT_TRUE(result.is_err());
    EXPECT_EQ(result.error().code, -1);
    EXPECT_EQ(result.error().message, "test error");
    EXPECT_FALSE(static_cast<bool>(result));
}

TEST(Result, ValueOr) {
    auto ok_result = Result<int>::ok(42);
    auto err_result = Result<int>::err(Error{-1, "error"});

    EXPECT_EQ(ok_result.value_or(0), 42);
    EXPECT_EQ(err_result.value_or(0), 0);
}

TEST(Result, ArrowOperator) {
    struct Data {
        int x = 10;
        int y = 20;
    };

    auto result = Result<Data>::ok(Data{});
    EXPECT_EQ(result->x, 10);
    EXPECT_EQ(result->y, 20);
}

TEST(Result, VoidOk) {
    auto result = Result<void>::ok();
    EXPECT_TRUE(result.is_ok());
    EXPECT_FALSE(result.is_err());
    EXPECT_TRUE(static_cast<bool>(result));
}

TEST(Result, VoidErr) {
    auto result = Result<void>::err(Error{-1, "void error"});
    EXPECT_FALSE(result.is_ok());
    EXPECT_TRUE(result.is_err());
    EXPECT_EQ(result.error().message, "void error");
}

TEST(Result, MoveSemantics) {
    auto result = Result<std::string>::ok("hello");
    std::string value = std::move(*result);
    EXPECT_EQ(value, "hello");
}

}  // namespace wadjet::test
