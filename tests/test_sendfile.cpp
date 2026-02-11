// ═══════════════════════════════════════════════════════════════════
//  test_sendfile.cpp — Tests for file serving and download
// ═══════════════════════════════════════════════════════════════════

#include <gtest/gtest.h>
#include <nodepp/sendfile.h>
#include <fstream>
#include <filesystem>

using namespace nodepp;

class SendFileTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create test file
        std::ofstream f("test_file.txt");
        f << "Hello, World! This is a test file for sendfile.";
        f.close();

        std::ofstream h("test_file.html");
        h << "<html><body>Test</body></html>";
        h.close();
    }

    void TearDown() override {
        std::filesystem::remove("test_file.txt");
        std::filesystem::remove("test_file.html");
    }
};

TEST_F(SendFileTest, SendsFileWithCorrectMimeType) {
    http::Request req;
    req.method = "GET";
    req.path = "/file";

    std::string sentBody;
    int sentStatus = 0;
    std::unordered_map<std::string, std::string> sentHeaders;

    http::Response res([&](int s, const auto& h, const std::string& b) {
        sentStatus = s;
        sentHeaders = h;
        sentBody = b;
    });

    sendfile::sendFile(req, res, "test_file.txt");

    EXPECT_EQ(sentStatus, 200);
    EXPECT_EQ(sentHeaders.at("Content-Type"), "text/plain");
    EXPECT_EQ(sentBody, "Hello, World! This is a test file for sendfile.");
}

TEST_F(SendFileTest, SendsHtmlFile) {
    http::Request req;
    req.method = "GET";
    req.path = "/page";

    std::string sentBody;
    std::unordered_map<std::string, std::string> sentHeaders;

    http::Response res([&](int, const auto& h, const std::string& b) {
        sentHeaders = h;
        sentBody = b;
    });

    sendfile::sendFile(req, res, "test_file.html");

    EXPECT_EQ(sentHeaders.at("Content-Type"), "text/html");
    EXPECT_EQ(sentBody, "<html><body>Test</body></html>");
}

TEST_F(SendFileTest, Returns404ForMissingFile) {
    http::Request req;
    req.method = "GET";

    int sentStatus = 0;
    http::Response res([&](int s, const auto&, const std::string&) {
        sentStatus = s;
    });

    sendfile::sendFile(req, res, "nonexistent.txt");
    EXPECT_EQ(sentStatus, 404);
}

TEST_F(SendFileTest, SupportsRangeRequests) {
    http::Request req;
    req.method = "GET";
    req.headers["range"] = "bytes=0-4";

    int sentStatus = 0;
    std::string sentBody;
    std::unordered_map<std::string, std::string> sentHeaders;

    http::Response res([&](int s, const auto& h, const std::string& b) {
        sentStatus = s;
        sentHeaders = h;
        sentBody = b;
    });

    sendfile::sendFile(req, res, "test_file.txt");

    EXPECT_EQ(sentStatus, 206);
    EXPECT_EQ(sentBody, "Hello");
    EXPECT_TRUE(sentHeaders.count("Content-Range"));
}

TEST_F(SendFileTest, DownloadSetsContentDisposition) {
    http::Request req;
    req.method = "GET";

    std::unordered_map<std::string, std::string> sentHeaders;
    http::Response res([&](int, const auto& h, const std::string&) {
        sentHeaders = h;
    });

    sendfile::download(req, res, "test_file.txt", "download.txt");
    EXPECT_EQ(sentHeaders.at("Content-Disposition"), "attachment; filename=\"download.txt\"");
}
