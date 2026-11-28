# XP-TUBE native platform foundation

This module defines backend-neutral lifecycle, display, audio-focus, controller,
and text-input events, a bounded single-thread event queue, and the XP-owned
backend/runtime boundary.

The application now routes the existing SDL event stream through an XP runtime
and event queue. SDL/window initialization remains single-owner in the inherited
platform startup; the XP bridge does not initialize SDL or poll it separately.
Controller actions, quit, text input, and text-delete events cross the XP event
contract and are temporarily converted back for the inherited screen layer.
This is an integration seam, not a replacement for the inherited SDL/OpenOrbis
backend. XP now owns the PS4 system-service bootstrap (network module initialization,
system language ID, and timezone offset) behind an OpenOrbis-specific adapter.
The PS4 numeric-language-to-BCP-47 mapping is also implemented and host-tested
inside XP Platform; the legacy backend now delegates to that mapping.
SDL/window startup, controller discovery, display/presentation, audio focus,
and dialogs remain in the migration layer; this is a first platform slice, not
a complete PS4 backend.

Host tests cover queue ordering, capacity, overflow, shutdown, runtime startup,
timestamp propagation, event pumping, failure handling, and compatibility
action mapping. Host tests do not prove PS4 hardware behavior.
