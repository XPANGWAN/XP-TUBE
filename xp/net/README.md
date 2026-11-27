# XP-TUBE native network

The XP network layer owns the HTTP request/response contract, cancellation,
TLS policy, bounded buffering, redirect policy, streaming, progress and
certificate inspection. `xp::net::CurlHttpTransport` is now the application
transport implementation. It uses libcurl as a platform dependency, with
system CA and hostname verification enabled by default, optional public-key
pinning, configurable redirects, HTTPS downgrade protection, low-speed
timeouts, cancellation and streaming sinks.

The PS4 backend keeps IPv4 resolution and disables the Linux-style TCP
keepalive options that previously caused problems on the console. DNS cache
sharing is retained; TLS sessions and live connections are not shared.

The application now injects this XP transport into YouTube, OAuth, image,
logo and update flows. The previous `XpHttpAdapter` and `HttpClient` remain
as compatibility code for older tests/callers while the migration is completed;
they are no longer the AppSession transport.

Peer-certificate inspection is informational only and never authenticates a
connection. SPKI digest extraction currently reuses the platform crypto
implementation during the migration; replacing that final shared helper is a
separate cleanup step.

The host tests exercise the transport contract and a real local HTTPS
streaming download with a trusted test CA. PS4 build success verifies
compilation and packaging only; real console network behavior still requires
hardware testing.
