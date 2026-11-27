#include "core/signals.hpp"

#include <signal.h>

#include <cstring>

namespace playtube::core {

void ignoreBrokenPipes() {
    // zeroed: the PS4 kernel's struct sigaction is laid out differently from musl's (musl passes it as it is), but
    // both start with the handler, so SIG_IGN with no flags and an empty mask reads the same; SIGPIPE is 13 on both
    struct sigaction action;
    std::memset(&action, 0, sizeof action);
    action.sa_handler = SIG_IGN;
    sigaction(SIGPIPE, &action, nullptr);
}

}  // namespace playtube::core
