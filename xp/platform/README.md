# XP-TUBE native platform foundation

This module defines backend-neutral lifecycle, display, audio-focus and
controller events plus a bounded event queue. It is original XP-TUBE-owned
infrastructure and deliberately contains no Playtube headers or PS4 SDK types.

The queue is single-thread-owned. A PS4 adapter must translate OpenOrbis/SDL
events into these stable XP event values on the owning event-pump thread.
Thread-safe cross-thread dispatch, controller polling, system dialogs, display
creation and audio-device integration are not claimed as implemented yet.

The host unit test verifies ordering, bounded capacity, overflow accounting,
zero-capacity behavior and shutdown semantics. Passing this test does not prove
PS4 controller or lifecycle behavior; that requires the PS4 adapter and
hardware validation.
