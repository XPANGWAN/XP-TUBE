#pragma once

namespace xp::platform {

// Separates temporary platform focus from the user's explicit playback choice.
// This policy does not acquire audio devices or observe OS lifecycle events.
class AudioFocusPolicy {
public:
    void focus_lost() noexcept { has_focus_ = false; }
    void focus_gained() noexcept { has_focus_ = true; }
    void set_user_paused(bool paused) noexcept { user_paused_ = paused; }

    bool has_focus() const noexcept { return has_focus_; }
    bool user_paused() const noexcept { return user_paused_; }
    bool should_pause_playback() const noexcept { return !has_focus_ || user_paused_; }

private:
    bool has_focus_{true};
    bool user_paused_{false};
};

} // namespace xp::platform
