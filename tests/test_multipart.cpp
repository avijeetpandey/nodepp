// ═══════════════════════════════════════════════════════════════════
//  test_multipart.cpp — Tests for multipart form-data parser
// ═══════════════════════════════════════════════════════════════════

#include <gtest/gtest.h>
#include <nodepp/multipart.h>

using namespace nodepp::multipart;

TEST(MultipartTest, ParseSimpleFields) {
    std::string boundary = "----WebKitFormBoundary7MA4YWxk";
    std::string body =
        "------WebKitFormBoundary7MA4YWxk\r\n"
        "Content-Disposition: form-data; name=\"username\"\r\n"
        "\r\n"
        "Alice\r\n"
        "------WebKitFormBoundary7MA4YWxk\r\n"
        "Content-Disposition: form-data; name=\"email\"\r\n"
        "\r\n"
        "alice@example.com\r\n"
        "------WebKitFormBoundary7MA4YWxk--\r\n";

    auto result = parse(body, "multipart/form-data; boundary=----WebKitFormBoundary7MA4YWxk");

    EXPECT_EQ(result.fields.at("username"), "Alice");
    EXPECT_EQ(result.fields.at("email"), "alice@example.com");
    EXPECT_TRUE(result.files.empty());
}

TEST(MultipartTest, ParseFileUpload) {
    std::string body =
        "--boundary\r\n"
        "Content-Disposition: form-data; name=\"file\"; filename=\"test.txt\"\r\n"
        "Content-Type: text/plain\r\n"
        "\r\n"
        "file contents here\r\n"
        "--boundary--\r\n";

    auto result = parse(body, "multipart/form-data; boundary=boundary");

    ASSERT_EQ(result.files.size(), 1u);
    EXPECT_EQ(result.files[0].fieldName, "file");
    EXPECT_EQ(result.files[0].filename, "test.txt");
    EXPECT_EQ(result.files[0].contentType, "text/plain");
    EXPECT_EQ(result.files[0].data, "file contents here");
    EXPECT_EQ(result.files[0].size, 18u);
}

TEST(MultipartTest, ParseMixedFieldsAndFiles) {
    std::string body =
        "--boundary\r\n"
        "Content-Disposition: form-data; name=\"title\"\r\n"
        "\r\n"
        "My Document\r\n"
        "--boundary\r\n"
        "Content-Disposition: form-data; name=\"doc\"; filename=\"doc.pdf\"\r\n"
        "Content-Type: application/pdf\r\n"
        "\r\n"
        "PDF_CONTENT\r\n"
        "--boundary--\r\n";

    auto result = parse(body, "multipart/form-data; boundary=boundary");

    EXPECT_EQ(result.fields.at("title"), "My Document");
    ASSERT_EQ(result.files.size(), 1u);
    EXPECT_EQ(result.files[0].filename, "doc.pdf");
}

TEST(MultipartTest, EmptyBoundary) {
    auto result = parse("some body", "text/plain");
    EXPECT_TRUE(result.fields.empty());
    EXPECT_TRUE(result.files.empty());
}

TEST(MultipartTest, ExtractBoundary) {
    auto b1 = detail::extractBoundary("multipart/form-data; boundary=abc123");
    EXPECT_EQ(b1, "abc123");

    auto b2 = detail::extractBoundary("multipart/form-data; boundary=\"quoted\"");
    EXPECT_EQ(b2, "quoted");
}
