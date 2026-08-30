#include "server.hpp"
#include <httplib.h>

void startServer(int port) {
    httplib::Server svr;

    svr.Post("/api/solve", [](const httplib::Request&, httplib::Response& res) {
        res.set_content("wip", "text/plain");
    });

    svr.listen("127.0.0.1", port);
}