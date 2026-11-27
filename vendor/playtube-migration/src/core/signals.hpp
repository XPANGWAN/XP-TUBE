#pragma once

namespace playtube::core {

// A write to a connection the other side has closed raises SIGPIPE, whose default ends the process with no trace.
// FFmpeg and curl ask the kernel not to with MSG_NOSIGNAL, but the PS4's musl headers carry Linux's value (0x4000)
// where the PS4 kernel (FreeBSD) expects 0x20000: a video resumed after a long pause (YouTube had closed the idle
// connection) closed Playtube while FFmpeg ended its TLS session. Once ignored, such a write
// fails with EPIPE instead. Called first thing in main.
void ignoreBrokenPipes();

}  // namespace playtube::core
