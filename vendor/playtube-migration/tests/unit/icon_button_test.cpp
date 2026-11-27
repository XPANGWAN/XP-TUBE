#include <doctest/doctest.h>

#include <nanovg.h>

#include <cstddef>
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

#include "render/nvg_sw.hpp"
#include "render/raster.hpp"
#include "support/fixed_metrics.hpp"
#include "ui/box.hpp"
#include "ui/context.hpp"
#include "ui/focus.hpp"
#include "ui/icon_button.hpp"

using namespace playtube::ui;
using playtube::platform::Action;

namespace {

struct Fixture {
    playtube::test::FixedMetrics metrics;
    FocusManager focus;
    Box root{Axis::Row};
    IconButton* button = nullptr;
    int presses = 0;
    Fixture() {
        context().focus = &focus;
        button = root.add<IconButton>(Icon::Like, "Like", [this] { ++presses; });
        focus.setScope(&root);
        root.place({0, 0, 400, 200}, metrics);
    }
    ~Fixture() {
        button = nullptr;
        root.clearChildren();
        context().focus = nullptr;
    }
};

struct Canvas {
    playtube::render::Surface surface{200, 200};
    NVGcontext* vg = playtube::render::createSoftwareCanvas(surface);
    ~Canvas() { playtube::render::destroySoftwareCanvas(vg); }
    const std::uint8_t* at(int x, int y) const {
        return &surface.rgba[(static_cast<std::size_t>(y) * 200 + static_cast<std::size_t>(x)) * 4];
    }
    void drawButton(const IconButton& b, std::uint8_t ground = 0) {
        surface.clear(ground, ground, ground, 255);
        nvgBeginFrame(vg, 200, 200, 1);
        nvgTranslate(vg, 50, 100);  // room above for the pill
        b.draw(vg);
        nvgEndFrame(vg);
    }
};

}  // namespace

TEST_CASE("icon button: 84 x 84, focusable, Confirm presses") {
    Fixture f;
    CHECK(f.button->frame.width == 84);
    CHECK(f.button->frame.height == 84);
    CHECK(f.button->canFocus());
    CHECK(f.focus.focus(f.button));
    CHECK(f.focus.dispatch(Action::Confirm));
    CHECK(f.presses == 1);
}

TEST_CASE("icon button: label, icon and initial change mark the layout dirty without changing the size") {
    Fixture f;
    CHECK(f.button->label() == "Like");
    f.button->setLabel("Liked");
    CHECK(f.button->label() == "Liked");
    CHECK(f.button->needsLayout());
    f.root.place({0, 0, 400, 200}, f.metrics);
    CHECK_FALSE(f.button->needsLayout());
    f.button->setIcon(Icon::LikeFilled);
    CHECK(f.button->needsLayout());
    f.root.place({0, 0, 400, 200}, f.metrics);
    f.button->setInitial("A");
    f.button->setLabelAlign(CrossAlign::End);
    f.root.place({0, 0, 400, 200}, f.metrics);
    CHECK(f.button->frame.width == 84);
    CHECK(f.button->frame.height == 84);
}

TEST_CASE("icon button: focus switches the fill and icon colours") {
    Fixture f;
    CHECK_FALSE(f.button->focusedNow());
    CHECK(f.button->fillColor().a == 0x24);
    CHECK(f.button->fillColor().r == 0xFF);
    CHECK(f.button->iconColor().r == theme().text.r);
    f.focus.focus(f.button);
    CHECK(f.button->focusedNow());
    CHECK(f.button->fillColor().a == 0xFF);
    CHECK(f.button->iconColor().r == 0x0A);
    CHECK(f.button->iconColor().b == 0x0C);
    f.button->onFocusChanged(false);
    CHECK_FALSE(f.button->focusedNow());
}

TEST_CASE("icon button: drawn fill follows focus, every icon draws, the pill shows only when focused") {
    Fixture f;
    Canvas c;
    REQUIRE(c.vg != nullptr);
    c.drawButton(*f.button);
    CHECK(c.at(50 + 42, 100 + 4)[0] < 80);   // 14 % white over black: dark grey
    CHECK(c.at(50 + 42, 100 - 15 - 12)[0] == 0);  // nothing above: no pill
    f.focus.focus(f.button);
    c.drawButton(*f.button);
    CHECK(c.at(50 + 42, 100 + 4)[0] == 255);  // white
    // every icon leaves ink inside the 39 px box
    for (Icon icon : {Icon::Play, Icon::Pause, Icon::Restart, Icon::Like, Icon::LikeFilled, Icon::Dislike,
                      Icon::DislikeFilled, Icon::Comments, Icon::LiveChat, Icon::Save, Icon::Bell,
                      Icon::BellTicked, Icon::Queue, Icon::Captions, Icon::Settings, Icon::Channel}) {
        f.button->setIcon(icon);
        c.drawButton(*f.button);
        int dark = 0;
        for (int y = 100 + 22; y < 100 + 62; ++y) {
            for (int x = 50 + 22; x < 50 + 62; ++x) {
                dark += c.at(x, y)[0] < 128 ? 1 : 0;
            }
        }
        CHECK_MESSAGE(dark > 20, "icon " << static_cast<int>(icon));
        CHECK_MESSAGE(dark < 1000, "icon " << static_cast<int>(icon));
    }
}

TEST_CASE("icon button: the pill shows above a focused button, centred or right-aligned") {
    Fixture f;
    Canvas c;
    REQUIRE(c.vg != nullptr);
    const int midRow = 100 - 15 - 22;  // inside the pill (45 px high, 15 px above the button)
    auto darkSpan = [&](int& left, int& right) {
        left = 1000;
        right = -1;
        for (int x = 0; x < 200; ++x) {
            if (c.at(x, midRow)[0] < 60) {
                left = std::min(left, x);
                right = std::max(right, x);
            }
        }
    };
    int left = 0;
    int right = 0;
    c.drawButton(*f.button, 128);
    darkSpan(left, right);
    CHECK(right < 0);  // unfocused: nothing above
    f.focus.focus(f.button);
    c.drawButton(*f.button, 128);
    darkSpan(left, right);
    REQUIRE(right > 0);
    CHECK(c.at(92, 100 - 15 - 3)[0] < 60);  // a pixel inside the pill, near its bottom
    CHECK(std::abs((left + right) / 2 - 92) <= 2);  // centred on the button (x 50..134)
    f.button->setLabelAlign(CrossAlign::End);
    f.root.place({0, 0, 400, 200}, f.metrics);
    c.drawButton(*f.button, 128);
    darkSpan(left, right);
    CHECK(std::abs(right - 133) <= 1);  // the pill's right edge on the button's
    CHECK(left < 92 - 20);
}

TEST_CASE("icon button: the initial is a label centred in the button, as the channel line's avatar draws it") {
    Fixture f;
    CHECK(f.button->initial().empty());
    f.button->setInitial("L");
    CHECK(f.button->initial() == "L");
    f.root.place({0, 0, 400, 200}, f.metrics);
    const Label& letter = f.button->initialLabel();
    CHECK(letter.text() == "L");
    CHECK(letter.style.size == 22);
    CHECK(letter.style.weight == FontWeight::Bold);
    REQUIRE(letter.lines().size() == 1);
    CHECK(letter.lines().front() == "L");
    CHECK(letter.frame.width > 0);
    CHECK(letter.frame.height > 0);
    // its box centred on the button's centre
    CHECK(std::abs(letter.frame.x + letter.frame.width / 2 - 42) < 0.5f);
    CHECK(std::abs(letter.frame.y + letter.frame.height / 2 - 42) < 0.5f);
    f.button->setInitial("");  // back to the icon
    CHECK(f.button->initial().empty());
}

TEST_CASE("icon button: a picture over the initial, in the same circle") {
    Fixture f;
    f.button->setInitial("L");
    CHECK(f.button->picture().empty());
    f.button->setPicture("https://example.test/avatar.jpg");
    CHECK(f.button->picture() == "https://example.test/avatar.jpg");
    f.root.place({0, 0, 400, 200}, f.metrics);
    const ImageView& picture = f.button->pictureView();
    CHECK(picture.frame.width == 51);
    CHECK(picture.frame.height == 51);
    CHECK(std::abs(picture.frame.x + picture.frame.width / 2 - 42) < 0.5f);
    CHECK(std::abs(picture.frame.y + picture.frame.height / 2 - 42) < 0.5f);
    CHECK(picture.cornerRadius == 25.5f);
    CHECK(picture.placeholder.a == 0);  // the initial shows until the picture comes, and when it does not
}

// Writes a strip of every icon, unfocused then focused, to $PLAYTUBE_ICON_STRIP (a PPM) when set; a look-and-feel aid.
TEST_CASE("icon button: icon strip for review" * doctest::skip(std::getenv("PLAYTUBE_ICON_STRIP") == nullptr)) {
    Fixture f;
    const int n = 16;
    playtube::render::Surface strip{n * 100 + 10, 20 + 160 + 100};
    NVGcontext* vg = playtube::render::createSoftwareCanvas(strip);
    REQUIRE(vg != nullptr);
    strip.clear(21, 21, 24, 255);
    nvgBeginFrame(vg, static_cast<float>(strip.width), static_cast<float>(strip.height), 1);
    for (int row = 0; row < 2; ++row) {
        f.focus.focus(nullptr);
        f.button->onFocusChanged(row == 1);
        for (int i = 0; i < n; ++i) {
            f.button->setIcon(static_cast<Icon>(i));
            nvgSave(vg);
            nvgTranslate(vg, 10.0f + static_cast<float>(i) * 100, 20.0f + static_cast<float>(row) * 160);
            f.button->draw(vg);
            nvgRestore(vg);
        }
    }
    nvgEndFrame(vg);
    std::FILE* out = std::fopen(std::getenv("PLAYTUBE_ICON_STRIP"), "wb");
    REQUIRE(out != nullptr);
    std::fprintf(out, "P6\n%d %d\n255\n", strip.width, strip.height);
    for (std::size_t i = 0; i < static_cast<std::size_t>(strip.width) * static_cast<std::size_t>(strip.height); ++i) {
        std::fwrite(&strip.rgba[i * 4], 1, 3, out);
    }
    std::fclose(out);
    playtube::render::destroySoftwareCanvas(vg);
}
