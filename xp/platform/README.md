# XP-TUBE native platform foundation

This module defines backend-neutral lifecycle, display, audio-focus and
controller events, a bounded single-thread event queue, and the XP-owned
backend/runtime boundary.

A hardware adapter implements `Backend`, translates native events into XP
`Event` values, and is coordinated by `Runtime`. Native SDK/SDL headers and
handles must remain inside that adapter. The current repository does not yet
contain an OpenOrbis/SDL PS4 adapter, controller polling, display creation,
system dialogs or audio-device integration.

A small compatibility translator maps the inherited action names into XP
controller events without importing SDL or upstream headers. This is a
transitional seam only; it is not yet called by the application event loop.
Host tests cover queue ordering/capacity/overflow/shutdown, runtime startup,
event pumping, failure handling and shutdown, and compatibility action mapping. Passing host tests does not prove
PS4 hardware behavior; that requires the concrete adapter and console testing.
