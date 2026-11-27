#pragma once

#include <string>

#include "core/log.hpp"

namespace playtube::app {

struct Options {
    int maxFrames = 0;       // > 0: quit after this many frames
    bool hidden = false;     // no visible window (tests)
    std::string dataDir;     // empty: platform default
    std::string testScript;  // UI test script (Linux test builds only)
    std::string testOutput;  // where test screenshots go
    log::Level logLevel = log::Level::Info;
    std::string screen = "shell";  // first screen: shell or gallery
    std::string renderer = "sw";   // sw: CPU drawing (both platforms); gl: NanoVG GLES2 (Linux, comparisons)
    int benchFrames = 0;           // > 0: draw this many frames of the first screen, log the times, quit
    std::string youtubeUrl;        // tests: every YouTube service at this address (Linux test builds only)
    std::string caFile;            // tests: certificates trusted instead of the shipped list
};

// Runs the app until the user quits; returns the process exit code.
int run(const Options& options);

}  // namespace playtube::app
