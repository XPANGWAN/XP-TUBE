# XP-TUBE native network contracts

This module defines XP-owned request, response, TLS-policy, cancellation,
progress, certificate-inspection, and streaming-sink contracts. It deliberately
does not select or implement a socket/TLS backend yet.

The migration application's current HTTP implementation remains in
`vendor/playtube-migration/src/net/http.cpp` and uses libcurl with the
platform's existing TLS libraries. The application target now compiles the XP
HTTP contract as an integration guard, while libcurl remains the active
transport. The next stage will implement an adapter behind
`IHttpTransport`, then move callers incrementally after PS4 TLS, redirect,
timeout, cancellation, and streaming tests pass.

Security defaults are explicit: system CA and hostname verification remain
the default; public-key pinning is opt-in; peer-certificate inspection is
informational and must never be treated as authenticated transport. The
contract also records HTTPS downgrade protection, redirect limits, low-speed
timeouts, and bounded buffered responses. Streaming downloads report
progress and can be stopped by cancellation or a refusing sink.

The shared XP error model includes typed timeout, TLS, untrusted-certificate,
bad-response, storage, and unsupported-operation categories.

This contract and compile guard do not replace the legacy HTTP implementation
and do not claim an independent HTTP backend.
