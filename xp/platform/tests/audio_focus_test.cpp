#include "xp/platform/audio_focus.hpp"

#include <cassert>

int main() {
    using xp::platform::AudioFocusPolicy;

    AudioFocusPolicy focus;
    assert(focus.has_focus());
    assert(!focus.user_paused());
    assert(!focus.should_pause_playback());

    focus.focus_lost();
    assert(!focus.has_focus());
    assert(focus.should_pause_playback());

    focus.focus_gained();
    assert(focus.has_focus());
    assert(!focus.should_pause_playback());

    focus.set_user_paused(true);
    assert(focus.should_pause_playback());
    focus.focus_lost();
    focus.focus_gained();
    assert(focus.user_paused());
    assert(focus.should_pause_playback());

    focus.set_user_paused(false);
    assert(!focus.should_pause_playback());
    focus.focus_lost();
    focus.set_user_paused(false);
    assert(focus.should_pause_playback());
    focus.focus_gained();
    assert(!focus.should_pause_playback());
}
