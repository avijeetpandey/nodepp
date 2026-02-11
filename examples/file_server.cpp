// ═══════════════════════════════════════════════════════════════════
//  file_server.cpp — File serving and download example
// ═══════════════════════════════════════════════════════════════════

#include <nodepp/http.h>
#include <nodepp/sendfile.h>
#include <nodepp/console.h>

using namespace nodepp;

int main() {
    auto app = http::createServer();

    // Serve a file with auto content-type detection
    app.get("/file/:name", [](http::Request& req, http::Response& res) {
        auto filename = "public/" + req.params["name"];
        sendfile::sendFile(req, res, filename);
    });

    // Trigger a browser download
    app.get("/download/:name", [](http::Request& req, http::Response& res) {
        auto filename = "public/" + req.params["name"];
        sendfile::download(req, res, filename);
    });

    app.listen(3000, [] {
        console::log("File server running on http://localhost:3000");
        console::log("  GET /file/:name      — serve a file");
        console::log("  GET /download/:name  — download a file");
    });
}
