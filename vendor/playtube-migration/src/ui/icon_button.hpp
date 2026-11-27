#pragma once

#include <functional>
#include <string>

#include "ui/box.hpp"
#include "ui/image_view.hpp"
#include "ui/label.hpp"
#include "ui/view.hpp"

namespace playtube::ui {

enum class Icon {
    Play,
    Pause,
    Restart,
    Like,
    LikeFilled,
    Dislike,
    DislikeFilled,
    Comments,
    LiveChat,
    Save,
    Bell,
    BellTicked,
    Queue,
    Captions,
    Settings,
    Channel
};

// A round 84 px button holding a line icon (24-unit box scaled to 39 px, 3 px stroke). Translucent white fill;
// focused: white fill, dark icon, and a label pill above it (drawn outside the frame: the layout never changes).
class IconButton : public View {
public:
    IconButton(Icon icon, std::string label, std::function<void()> onPress);

    void setIcon(Icon icon);
    void setLabel(std::string label);
    // A letter in a circle instead of an icon (a channel's initial).
    void setInitial(std::string letter);
    // A picture over the initial, in its circle (a channel's avatar); the initial shows until it comes.
    void setPicture(std::string url);
    const std::string& picture() const { return picture_.url(); }
    const ImageView& pictureView() const { return picture_; }
    // Center (default) or End: the pill's right edge on the button's (the last icon on the right).
    void setLabelAlign(CrossAlign align);
    const std::string& label() const { return label_; }
    Icon icon() const { return icon_; }
    const std::string& initial() const { return initial_; }
    // the initial's text as drawn (for tests): placed at the button's centre
    const Label& initialLabel() const { return letter_; }
    bool focusedNow() const { return focused_; }

    // What draw() uses (for tests)
    Color fillColor() const;
    Color iconColor() const;

    void onFocusChanged(bool focused) override;
    void draw(NVGcontext* vg) const override;
    void onThemeChanged() override;
    // The pill's size and the frame-relative position of its right edge (for tests)
    float pillWidth() const { return pillW_; }

protected:
    void layout(TextMetrics& metrics) override;

private:
    void drawIcon(NVGcontext* vg, Color color) const;
    void drawLabel(NVGcontext* vg) const;

    Icon icon_;
    std::string label_;
    std::string initial_;
    CrossAlign labelAlign_ = CrossAlign::Center;
    bool focused_ = false;
    Label pill_;    // the label pill's text (not a child: laid out here, drawn outside the frame)
    Label letter_;  // the initial (not a child either: laid out here, drawn at the centre)
    ImageView picture_;  // the picture over the initial (not a child either)
    float pillW_ = 0;
    float pillH_ = 0;
};

}  // namespace playtube::ui
