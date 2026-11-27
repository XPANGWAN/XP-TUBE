#include <doctest/doctest.h>

#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>

#include "core/signals.hpp"

TEST_CASE("a write to a closed connection fails with EPIPE instead of ending the process") {
    playtube::core::ignoreBrokenPipes();
    int fds[2] = {-1, -1};
    REQUIRE(socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0);
    close(fds[1]);
    const char byte = 'x';
    // a plain write: no MSG_NOSIGNAL, as when the flag does not reach the kernel (the PS4)
    const ssize_t written = write(fds[0], &byte, 1);
    const int error = errno;
    close(fds[0]);
    CHECK(written == -1);
    CHECK(error == EPIPE);
}
