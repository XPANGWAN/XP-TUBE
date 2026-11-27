# XP-TUBE native network contracts

This module defines XP-owned request, response, TLS-policy, cancellation,
progress, and streaming-sink contracts. It deliberately does not select or
implement a socket/TLS backend yet.

The migration application's current HTTP implementation remains in
`vendor/playtube-migration/src/net/http.cpp` and uses libcurl with the
platform's existing TLS libraries. The next migration stage will implement
an adapter behind `IHttpTransport`, then move callers incrementally after
PS4 TLS, redirect, timeout, cancellation, and streaming tests pass.

Security defaults are explicit: system CA and hostname verification remain
the default; public-key pinning is opt-in; peer-certificate inspection is a
separate mode and must never be treated as authenticated transport. Buffered
responses have a configured size ceiling. Streaming downloads report progress
and can be stopped by cancellation or a refusing sink.

This contract alone does not replace the legacy HTTP implementation and
does not claim an independent HTTP backend.
