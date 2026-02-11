// ═══════════════════════════════════════════════════════════════════
//  test_validator.cpp — Tests for request validation
// ═══════════════════════════════════════════════════════════════════

#include <gtest/gtest.h>
#include <nodepp/validator.h>

using namespace nodepp;
using namespace nodepp::validator;

TEST(ValidatorTest, RequiredFieldMissing) {
    Schema s;
    s.field("name").required().isString();

    auto body = JsonValue(nlohmann::json::object());
    auto errors = s.validate(body);

    ASSERT_EQ(errors.size(), 1);
    EXPECT_EQ(errors[0].field, "name");
    EXPECT_EQ(errors[0].rule, "required");
}

TEST(ValidatorTest, RequiredFieldPresent) {
    Schema s;
    s.field("name").required().isString();

    auto body = JsonValue(nlohmann::json{{"name", "Alice"}});
    auto errors = s.validate(body);
    EXPECT_TRUE(errors.empty());
}

TEST(ValidatorTest, TypeValidation) {
    Schema s;
    s.field("age").required().isNumber();

    auto body = JsonValue(nlohmann::json{{"age", "not a number"}});
    auto errors = s.validate(body);

    ASSERT_EQ(errors.size(), 1);
    EXPECT_EQ(errors[0].rule, "type");
}

TEST(ValidatorTest, MinLengthValidation) {
    Schema s;
    s.field("password").required().isString().minLength(8);

    auto body = JsonValue(nlohmann::json{{"password", "abc"}});
    auto errors = s.validate(body);

    ASSERT_EQ(errors.size(), 1);
    EXPECT_EQ(errors[0].rule, "minLength");
}

TEST(ValidatorTest, MaxLengthValidation) {
    Schema s;
    s.field("username").required().isString().maxLength(20);

    auto body = JsonValue(nlohmann::json{{"username", "thisIsAVeryLongUsernameIndeed"}});
    auto errors = s.validate(body);

    ASSERT_EQ(errors.size(), 1);
    EXPECT_EQ(errors[0].rule, "maxLength");
}

TEST(ValidatorTest, EmailValidation) {
    Schema s;
    s.field("email").required().isString().email();

    auto badBody = JsonValue(nlohmann::json{{"email", "notanemail"}});
    auto goodBody = JsonValue(nlohmann::json{{"email", "user@example.com"}});

    EXPECT_FALSE(s.isValid(badBody));
    EXPECT_TRUE(s.isValid(goodBody));
}

TEST(ValidatorTest, MinMaxNumberValidation) {
    Schema s;
    s.field("age").required().isNumber().min(0).max(150);

    auto tooLow = JsonValue(nlohmann::json{{"age", -1}});
    auto tooHigh = JsonValue(nlohmann::json{{"age", 200}});
    auto justRight = JsonValue(nlohmann::json{{"age", 25}});

    EXPECT_FALSE(s.isValid(tooLow));
    EXPECT_FALSE(s.isValid(tooHigh));
    EXPECT_TRUE(s.isValid(justRight));
}

TEST(ValidatorTest, EnumValidation) {
    Schema s;
    s.field("role").required().isString().oneOf({"admin", "user", "guest"});

    auto bad = JsonValue(nlohmann::json{{"role", "superadmin"}});
    auto good = JsonValue(nlohmann::json{{"role", "admin"}});

    EXPECT_FALSE(s.isValid(bad));
    EXPECT_TRUE(s.isValid(good));
}

TEST(ValidatorTest, OptionalFieldNotRequired) {
    Schema s;
    s.field("nickname").optional().isString().maxLength(50);

    auto body = JsonValue(nlohmann::json::object()); // no nickname
    EXPECT_TRUE(s.isValid(body));
}

TEST(ValidatorTest, MultipleFieldErrors) {
    Schema s;
    s.field("name").required().isString();
    s.field("age").required().isNumber();

    auto body = JsonValue(nlohmann::json::object());
    auto errors = s.validate(body);
    EXPECT_EQ(errors.size(), 2);
}

TEST(ValidatorTest, CustomValidator) {
    Schema s;
    s.field("code").required().isString().custom([](const JsonValue& val) -> std::optional<std::string> {
        auto str = val.get<std::string>();
        if (str.size() != 6) return "code must be exactly 6 characters";
        return std::nullopt;
    });

    auto bad = JsonValue(nlohmann::json{{"code", "ABC"}});
    auto good = JsonValue(nlohmann::json{{"code", "ABC123"}});

    EXPECT_FALSE(s.isValid(bad));
    EXPECT_TRUE(s.isValid(good));
}
