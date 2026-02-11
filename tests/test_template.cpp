// ═══════════════════════════════════════════════════════════════════
//  test_template.cpp — Tests for Mustache-like template engine
// ═══════════════════════════════════════════════════════════════════

#include <gtest/gtest.h>
#include <nodepp/template_engine.h>

using namespace nodepp::tmpl;

TEST(TemplateTest, SimpleVariableSubstitution) {
    auto result = render("Hello, {{name}}!", {{"name", "Alice"}});
    EXPECT_EQ(result, "Hello, Alice!");
}

TEST(TemplateTest, MultipleVariables) {
    auto result = render("{{greeting}}, {{name}}!",
        {{"greeting", "Hi"}, {"name", "Bob"}});
    EXPECT_EQ(result, "Hi, Bob!");
}

TEST(TemplateTest, HtmlEscaping) {
    auto result = render("{{content}}", {{"content", "<b>bold</b>"}});
    EXPECT_EQ(result, "&lt;b&gt;bold&lt;/b&gt;");
}

TEST(TemplateTest, UnescapedVariable) {
    auto result = render("{{{content}}}", {{"content", "<b>bold</b>"}});
    EXPECT_EQ(result, "<b>bold</b>");
}

TEST(TemplateTest, MissingVariable) {
    auto result = render("Hello, {{name}}!", nlohmann::json::object());
    EXPECT_EQ(result, "Hello, !");
}

TEST(TemplateTest, SectionWithArray) {
    auto result = render(
        "{{#items}}- {{name}}\n{{/items}}",
        {{"items", nlohmann::json::array({
            {{"name", "Apple"}},
            {{"name", "Banana"}}
        })}}
    );
    EXPECT_EQ(result, "- Apple\n- Banana\n");
}

TEST(TemplateTest, SectionWithBoolean) {
    auto result = render("{{#show}}Visible{{/show}}", {{"show", true}});
    EXPECT_EQ(result, "Visible");

    auto result2 = render("{{#show}}Visible{{/show}}", {{"show", false}});
    EXPECT_EQ(result2, "");
}

TEST(TemplateTest, InvertedSection) {
    auto result = render("{{^items}}No items{{/items}}", {{"items", nlohmann::json::array()}});
    EXPECT_EQ(result, "No items");

    auto result2 = render("{{^items}}No items{{/items}}",
        {{"items", nlohmann::json::array({1, 2})}});
    EXPECT_EQ(result2, "");
}

TEST(TemplateTest, Comments) {
    auto result = render("Hello{{! this is a comment }}, World!", nlohmann::json::object());
    EXPECT_EQ(result, "Hello, World!");
}

TEST(TemplateTest, DottedPaths) {
    auto result = render("{{user.name}} ({{user.email}})", {
        {"user", {{"name", "Alice"}, {"email", "alice@example.com"}}}
    });
    EXPECT_EQ(result, "Alice (alice@example.com)");
}

TEST(TemplateTest, NumberVariables) {
    auto result = render("Count: {{count}}", {{"count", 42}});
    EXPECT_EQ(result, "Count: 42");
}

TEST(TemplateTest, NestedSections) {
    auto result = render(
        "{{#users}}{{name}}: {{#skills}}{{.}} {{/skills}}\n{{/users}}",
        {{"users", nlohmann::json::array({
            {{"name", "Alice"}, {"skills", nlohmann::json::array({"C++", "Python"})}},
        })}}
    );
    EXPECT_TRUE(result.find("Alice:") != std::string::npos);
}
