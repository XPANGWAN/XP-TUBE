#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <thread>

#include "support/fake_youtube.hpp"

// Standalone fake YouTube for the end-to-end screenshots (TLS with the test CA):
// fake_youtube [--url-file PATH] [--playable (every player client answers with the fake's streams)]
//              [--showcase DIR (the signed-in home's feeds and their pictures, for the README)]
int main(int argc, char** argv) {
    std::string urlFile;
    bool playable = false;
    std::string showcase;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--url-file") == 0 && i + 1 < argc) {
            urlFile = argv[++i];
        } else if (std::strcmp(argv[i], "--showcase") == 0 && i + 1 < argc) {
            showcase = argv[++i];
        } else if (std::strcmp(argv[i], "--playable") == 0) {
            playable = true;
        }
    }
    playtube::test::FakeYouTube server;
    if (!showcase.empty()) {
        server.setShowcase(showcase);
    }
    if (playable) {
        server.servePlayableMedia();
        server.setPlayer("VISIONOS", server.playerReply({137, 136, 135, 134, 140, 18}));
    }
    if (!urlFile.empty()) {
        std::ofstream(urlFile) << server.baseUrl();
    }
    std::printf("%s\n", server.baseUrl().c_str());
    std::fflush(stdout);
    for (;;) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}
