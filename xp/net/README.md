# XP-TUBE native network

The XP network layer owns the application's HTTP contract and implementation:
request/response types, cancellation, TLS policy, bounded buffering, redirect
policy, streaming downloads, progress callbacks and certificate inspection.
`xp::net::CurlHttpTransport` is injected into the app session and used by
YouTube, OAuth, image, logo and updater flows.

libcurl and OpenSSL/mbedTLS remain platform dependencies; they are not reused
application-level Playtube HTTP modules. The PS4 backend keeps IPv4 resolution
and disables TCP keepalive options that previously caused console problems.
DNS cache sharing is retained; TLS sessions and live connections are not
shared.

The former application-level `net::HttpClient`, `XpHttpAdapter` and
`xp_http_compat` bridge have been removed from the active source/build.
YouTube request code now uses XP-native request/response types. URL encoding
and a small SHA-256 streaming utility remain in the migration source while
their XP-native replacements are prepared.

Peer-certificate inspection is informational only and never authenticates a
connection. SPKI hashing is implemented in XP Network using the platform
crypto library.

Host tests exercise the XP transport contract, buffering limits, cancellation,
certificate inspection and a real local HTTPS streaming download with a
trusted test CA. PS4 build success verifies compilation and packaging only;
real console network behavior still requires hardware testing.
