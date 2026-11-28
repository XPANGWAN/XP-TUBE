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
backend. Independent PS4 lifecycle, display, audio, dialogs, and controller
discovery remain future migration work.

Host tests cover queue ordering, capacity, overflow, shutdown, runtime startup,
timestamp propagation, event pumping, failure handling, and compatibility
action mapping. Host tests do not prove PS4 hardware behavior.
