#include <doctest/doctest.h>

#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "app/shell/chrome.hpp"
#include "app/shell/player_settings.hpp"
#include "app/shell/screens.hpp"
#include "app/shell/trickplay_preview.hpp"
#include "support/shell_harness.hpp"
#include "ui/image_view.hpp"
#include "ui/video_source.hpp"

using namespace playtube;
using namespace playtube::test;

namespace {

// the player's picture, as the frame loop would ask for it
bool picture(Shell& shell) {
    ui::VideoSource* video = shell.application->videoSource();
    if (video == nullptr) {
        return false;
    }
    video->renderVideo();
    return video->videoLayer().rgb0 != nullptr;
}

// the player's round buttons on screen, left to right
std::vector<ui::IconButton*> rowIcons(Shell& shell) {
    std::vector<ui::IconButton*> all;
    findViews<ui::IconButton>(*shell.application->top(), all);
    std::vector<ui::IconButton*> shown;
    for (ui::IconButton* icon : all) {
        bool visible = true;
        for (const ui::View* view = icon; view != nullptr; view = view->parent()) {
            visible = visible && view->visible;
        }
        if (visible) {
            shown.push_back(icon);
        }
    }
    return shown;
}

std::vector<std::string> rowLabels(Shell& shell) {
    std::vector<std::string> labels;
    for (ui::IconButton* icon : rowIcons(shell)) {
        labels.push_back(icon->label());
    }
    return labels;
}

ui::IconButton* focusedIcon(Shell& shell) {
    return dynamic_cast<ui::IconButton*>(shell.application->focus().focused());
}

using Labels = std::vector<std::string>;
using Rows = std::vector<std::pair<std::string, std::string>>;

// the Paramètres panel's lines, top to bottom: each name and value
Rows settingRows(Shell& shell) {
    std::vector<app::PlayerSettingLine*> lines;
    findViews<app::PlayerSettingLine>(shell.application->root(), lines);
    Rows rows;
    for (const app::PlayerSettingLine* line : lines) {
        rows.emplace_back(line->title(), line->value());
    }
    return rows;
}

app::PlayerSettingLine* focusedLine(Shell& shell) {
    return dynamic_cast<app::PlayerSettingLine*>(shell.application->focus().focused());
}

// a modal closed with ◯ gives the focus back to the icon that opened it
void backToIcon(Shell& shell, const std::string& label) {
    shell.press(Action::Back);
    shell.frames(3);
    CHECK_FALSE(shell.application->hasModal());
    REQUIRE_MESSAGE(focusedIcon(shell) != nullptr, label);
    CHECK(focusedIcon(shell)->label() == label);
}

}  // namespace

TEST_CASE("player: the streams of the first client that answers, with its User-Agent, video and separate audio") {
    Shell shell;
    shell.youtube.servePlayableMedia();
    shell.youtube.setPlayer("VISIONOS", shell.youtube.playerReply({137, 140, 18}));
    shell.application->replace(app::makeTab(shell.session, app::Tab::Settings));  // somewhere to come back to
    app::openVideo(shell.session, "dQw4w9WgXcQ");
    shell.frames(10);
    CHECK(picture(shell));
    CHECK(shows(*shell.application, "Demo"));         // the title from the player reply
    CHECK(showsSomewhere(*shell.application, "Lofi Girl"));  // the channel from the video page
    const auto video = shell.youtube.requestsTo("/media/137");
    const auto audio = shell.youtube.requestsTo("/media/140");
    REQUIRE(video.size() >= 2);  // the probe, then the player
    REQUIRE(audio.size() >= 2);  // the separate audio too: probed, then played
    CHECK(audio.back().range != audio.front().range);
    CHECK(video.back().userAgent.find("Safari") != std::string::npos);
    CHECK(audio.back().userAgent == video.back().userAgent);
    shell.press(Action::Back);
    CHECK(shell.application->videoSource() == nullptr);  // the engine released with the screen
}

TEST_CASE("player: a connection blocked by YouTube's bot check says so, with Retry and Back") {
    Shell shell;
    app::openVideo(shell.session, "dQw4w9WgXcQ");
    shell.frames(10);
    REQUIRE(shell.application->hasModal());
    CHECK(shows(*shell.application, "YouTube is blocking your connection for now."));
    CHECK_FALSE(shows(*shell.application, "YouTube refused this video"));
    shell.youtube.servePlayableMedia();
    shell.youtube.setPlayer("VISIONOS", shell.youtube.playerReply({137, 140}));
    pressAnywhere(shell, "Try again");
    shell.frames(10);
    CHECK_FALSE(shell.application->hasModal());
    CHECK(picture(shell));
}

TEST_CASE("player: a refused video says YouTube refused it and why, with Retry and Back") {
    Shell shell;
    for (const char* name : {"VISIONOS", "ANDROID_VR", "IOS", "ANDROID"}) {
        shell.youtube.setPlayer(name, R"({"playabilityStatus":{"status":"ERROR","reason":"Video unavailable"}})");
    }
    app::openVideo(shell.session, "dQw4w9WgXcQ");
    shell.frames(10);
    REQUIRE(shell.application->hasModal());
    CHECK(shows(*shell.application, "YouTube refused this video: Video unavailable"));
    shell.youtube.servePlayableMedia();
    shell.youtube.setPlayer("VISIONOS", shell.youtube.playerReply({137, 140}));
    pressAnywhere(shell, "Try again");
    shell.frames(10);
    CHECK_FALSE(shell.application->hasModal());
    CHECK(picture(shell));
}

TEST_CASE("player: ↓ puts the focus on Play/Pause, → moves along the row, ◯ leaves it and ← / → seek again") {
    Shell shell;
    shell.youtube.servePlayableMedia();
    shell.youtube.setPlayer("VISIONOS", shell.youtube.playerReply({137, 140}));
    shell.application->replace(app::makeTab(shell.session, app::Tab::Settings));
    app::openVideo(shell.session, "dQw4w9WgXcQ");
    shell.frames(10);
    REQUIRE(picture(shell));
    CHECK(focusedIcon(shell) == nullptr);  // the bar shows, the row waits
    shell.press(Action::Down);
    REQUIRE(focusedIcon(shell) != nullptr);
    CHECK(focusedIcon(shell)->label() == "Play");  // paused on its first picture in tests
    shell.press(Action::Right);
    REQUIRE(focusedIcon(shell) != nullptr);
    CHECK(focusedIcon(shell)->label() == "Comments");
    shell.press(Action::Right);
    shell.press(Action::Right);
    REQUIRE(focusedIcon(shell) != nullptr);
    CHECK(focusedIcon(shell)->label() == "Settings");
    shell.press(Action::Right);  // the last one: it stays there
    REQUIRE(focusedIcon(shell) != nullptr);
    CHECK(focusedIcon(shell)->label() == "Settings");
    shell.press(Action::Back);  // leaves the row, not the player
    CHECK(focusedIcon(shell) == nullptr);
    CHECK(shell.application->videoSource() != nullptr);
    shell.press(Action::Right);  // seeks again
    CHECK(shows(*shell.application, "0:03"));
    shell.press(Action::Left);
    CHECK(shows(*shell.application, "0:00"));
    CHECK(focusedIcon(shell) == nullptr);
    // ✕ on Play plays: the icon and its name become Pause, and back
    shell.press(Action::Down);
    shell.press(Action::Confirm);
    REQUIRE(focusedIcon(shell) != nullptr);
    CHECK(focusedIcon(shell)->label() == "Pause");
    CHECK(focusedIcon(shell)->icon() == ui::Icon::Pause);
    shell.press(Action::Confirm);
    REQUIRE(focusedIcon(shell) != nullptr);
    CHECK(focusedIcon(shell)->label() == "Play");
    CHECK(focusedIcon(shell)->icon() == ui::Icon::Play);
    shell.press(Action::Back);
    shell.press(Action::Back);
    CHECK(shell.application->videoSource() == nullptr);
}

TEST_CASE("player: Settings opens its panel, the values shown; \xE2\x97\x8B goes back to the Settings icon") {
    Shell shell;
    shell.youtube.servePlayableMedia();
    shell.youtube.setPlayer("VISIONOS", shell.youtube.playerReply({137, 140}));
    shell.application->replace(app::makeTab(shell.session, app::Tab::Settings));
    app::openVideo(shell.session, "dQw4w9WgXcQ");
    shell.frames(10);
    REQUIRE(picture(shell));
    shell.press(Action::Down);
    for (int i = 0; i < 3; ++i) {
        shell.press(Action::Right);
    }
    REQUIRE(focusedIcon(shell) != nullptr);
    REQUIRE(focusedIcon(shell)->label() == "Settings");
    shell.press(Action::Confirm);
    shell.frames(3);
    REQUIRE(shell.application->hasModal());
    // one track of audio: no audio line
    CHECK(settingRows(shell) ==
          Rows{{"Quality", "1080p"}, {"Speed", "Normal"}, {"Volume", "100 %"}, {"Picture", "Whole picture"}});
    REQUIRE(focusedLine(shell) != nullptr);
    CHECK(focusedLine(shell)->title() == "Quality");
    CHECK(shows(*shell.application, "Settings"));  // the panel's title
    CHECK_FALSE(shows(*shell.application, "Demo"));  // the picture alone behind it: the bar hidden
    backToIcon(shell, "Settings");
    CHECK(shows(*shell.application, "Demo"));  // the bar again, with the focus on its icon
    CHECK(shell.application->videoSource() != nullptr);  // the video is still there
    shell.press(Action::Back);  // leaves the row
    CHECK(focusedIcon(shell) == nullptr);
    CHECK(shell.application->videoSource() != nullptr);
}

TEST_CASE("player: a line of the settings opens its chooser; the panel comes back on that line, its value changed") {
    Shell shell;
    shell.youtube.servePlayableMedia();
    shell.youtube.setPlayer("VISIONOS", shell.youtube.playerReply({137, 140}));
    shell.application->replace(app::makeTab(shell.session, app::Tab::Settings));
    app::openVideo(shell.session, "dQw4w9WgXcQ");
    shell.frames(10);
    REQUIRE(picture(shell));
    shell.press(Action::Down);
    pressAnywhere(shell, "Settings");
    REQUIRE(shell.application->hasModal());
    const auto choose = [&](const std::string& line, const std::string& choice) {
        INFO(line);
        pressAnywhere(shell, line);
        REQUIRE(shell.application->hasModal());
        REQUIRE(focusedLine(shell) == nullptr);  // its chooser over the panel
        pressAnywhere(shell, choice);
        REQUIRE(shell.application->hasModal());  // the panel again
        REQUIRE(focusedLine(shell) != nullptr);
        CHECK(focusedLine(shell)->title() == line);
        CHECK(focusedLine(shell)->value() == choice);
    };
    choose("Speed", "1.5\xC3\x97");
    choose("Volume", "50 %");
    choose("Picture", "Whole picture \xC2\xB7 90 %");
    choose("Quality", "720p");
    CHECK(settingRows(shell) == Rows{{"Quality", "720p"},
                                     {"Speed", "1.5\xC3\x97"},
                                     {"Volume", "50 %"},
                                     {"Picture", "Whole picture \xC2\xB7 90 %"}});
    shell.press(Action::Back);
    shell.frames(3);
    CHECK_FALSE(shell.application->hasModal());
    REQUIRE(focusedIcon(shell) != nullptr);
    CHECK(focusedIcon(shell)->label() == "Settings");
    pressAnywhere(shell, "Settings");  // opened again: the values kept
    CHECK(settingRows(shell) == Rows{{"Quality", "720p"},
                                     {"Speed", "1.5\xC3\x97"},
                                     {"Volume", "50 %"},
                                     {"Picture", "Whole picture \xC2\xB7 90 %"}});
    shell.press(Action::Back);
}

TEST_CASE("player: the speed chooser lists the speeds as the panel writes them, in French too") {
    Shell shell;
    ui::translator().load(std::string(PLAYTUBE_SOURCE_DIR) + "/resources/i18n", "fr");
    shell.youtube.servePlayableMedia();
    shell.youtube.setPlayer("VISIONOS", shell.youtube.playerReply({137, 140}));
    shell.application->replace(app::makeTab(shell.session, app::Tab::Settings));
    app::openVideo(shell.session, "dQw4w9WgXcQ");
    shell.frames(10);
    REQUIRE(picture(shell));
    shell.press(Action::Down);
    pressAnywhere(shell, "Param\xC3\xA8tres");
    REQUIRE(shell.application->hasModal());
    pressAnywhere(shell, "Vitesse");
    REQUIRE(shell.application->hasModal());
    pressAnywhere(shell, "Normale");  // 1x as "Normale", not "1×"
    pressAnywhere(shell, "Vitesse");
    pressAnywhere(shell, "1,5\xC3\x97");
    bool shown = false;
    for (const auto& row : settingRows(shell)) {
        shown = shown || (row.first == "Vitesse" && row.second == "1,5\xC3\x97");
    }
    CHECK(shown);
    shell.press(Action::Back);
    ui::translator().load(std::string(PLAYTUBE_SOURCE_DIR) + "/resources/i18n", "en");
}

TEST_CASE("player: the panel's slot is cleared when the panel is destroyed") {
    auto slot = std::make_shared<app::PlayerSettings*>(nullptr);
    {
        auto panel = app::makePlayerSettings({});
        panel->trackIn(slot);
        CHECK(*slot == panel.get());
    }
    CHECK(*slot == nullptr);
    auto panel = app::makePlayerSettings({});
    panel->trackIn(nullptr);  // no slot: nothing to point at, nothing to clear later
}

TEST_CASE("player: the progress bar's head moves by whole pixels (the bar is drawn again only then)") {
    // a 15 minute video on a 1728 px bar: about two pixels a second
    CHECK(app::progressPixel(0, 900, 1728) == 0);
    CHECK(app::progressPixel(450, 900, 1728) == 864);
    CHECK(app::progressPixel(900, 900, 1728) == 1728);
    CHECK(app::progressPixel(450.0, 900, 1728) == app::progressPixel(450.03, 900, 1728));  // the next frame
    CHECK(app::progressPixel(450.0, 900, 1728) != app::progressPixel(451.0, 900, 1728));   // a second later
    CHECK(app::progressPixel(1000, 900, 1728) == 1728);  // past the end
    CHECK(app::progressPixel(-5, 900, 1728) == 0);
    CHECK(app::progressPixel(10, 0, 1728) == -1);  // no duration yet
    CHECK(app::progressPixel(10, 900, 0) == -1);   // not laid out yet
}

TEST_CASE("compact counts as YouTube writes them, in the language's units") {
    const std::string dir = std::string(PLAYTUBE_SOURCE_DIR) + "/resources/i18n";
    ui::translator().load(dir, "fr");
    CHECK(app::compactCount(290) == "290");
    CHECK(app::compactCount(5234) == "5,2\xC2\xA0k");
    CHECK(app::compactCount(1000) == "1\xC2\xA0k");
    CHECK(app::compactCount(52340) == "52\xC2\xA0k");
    CHECK(app::compactCount(19500302) == "19\xC2\xA0M");
    CHECK(app::compactCount(1950000000) == "1,9\xC2\xA0Md");
    ui::translator().load(dir, "ja");
    CHECK(app::compactCount(5200) == "5200");
    CHECK(app::compactCount(12345) == "1.2\xE4\xB8\x87");      // 1.2万
    CHECK(app::compactCount(19500302) == "1950\xE4\xB8\x87");  // 1950万
    ui::translator().load(dir, "en");
    CHECK(app::compactCount(999) == "999");
    CHECK(app::compactCount(5234) == "5.2K");
    CHECK(app::compactCount(999999) == "999K");
    CHECK(app::compactCount(521783) == "521K");
    CHECK(app::compactCount(-3) == "0");
    CHECK(app::speedText(1) == "Normal");
    CHECK(app::speedText(1.5) == "1.5\xC3\x97");
    ui::translator().load(dir, "fr");
    CHECK(app::speedText(1) == "Normale");
    CHECK(app::speedText(0.75) == "0,75\xC3\x97");
    ui::translator().load(dir, "en");
}

TEST_CASE("player: Shorts keep ↑ / ↓ for the next Short; △ reaches the row") {
    Shell shell;
    shell.youtube.servePlayableMedia();
    shell.youtube.setPlayer("VISIONOS", shell.youtube.playerReply({137, 140}));
    youtube::Video only;
    only.id = "dQw4w9WgXcQ";
    only.isShort = true;
    app::openShorts(shell.session, std::make_shared<const std::vector<youtube::Video>>(1, only), 0);
    shell.frames(10);
    shell.press(Action::Up);  // no Short before it: nothing changes, the row stays unfocused
    CHECK(focusedIcon(shell) == nullptr);
    shell.press(Action::Options);
    REQUIRE(focusedIcon(shell) != nullptr);
    CHECK(focusedIcon(shell)->label() == "Play");
    shell.press(Action::Back);
    CHECK(focusedIcon(shell) == nullptr);
    CHECK(shell.application->videoSource() != nullptr);
    shell.press(Action::Menu);  // the OPTIONS button does as △: the row, then back to the video
    REQUIRE(focusedIcon(shell) != nullptr);
    CHECK(focusedIcon(shell)->label() == "Play");
    shell.press(Action::Menu);
    CHECK(focusedIcon(shell) == nullptr);
    CHECK(shell.application->videoSource() != nullptr);
}

TEST_CASE("player: OPTIONS puts the focus on the row and takes it back, as △ does") {
    Shell shell;
    shell.youtube.servePlayableMedia();
    shell.youtube.setPlayer("VISIONOS", shell.youtube.playerReply({137, 140}));
    shell.application->replace(app::makeTab(shell.session, app::Tab::Settings));
    app::openVideo(shell.session, "dQw4w9WgXcQ");
    shell.frames(10);
    REQUIRE(picture(shell));
    shell.press(Action::Menu);
    REQUIRE(focusedIcon(shell) != nullptr);
    CHECK(focusedIcon(shell)->label() == "Play");
    shell.press(Action::Right);
    REQUIRE(focusedIcon(shell) != nullptr);
    CHECK(focusedIcon(shell)->label() == "Comments");
    shell.press(Action::Menu);  // leaves the row, not the player
    CHECK(focusedIcon(shell) == nullptr);
    CHECK(shell.application->videoSource() != nullptr);
    shell.press(Action::Options);
    REQUIRE(focusedIcon(shell) != nullptr);
    shell.press(Action::Menu);
    CHECK(focusedIcon(shell) == nullptr);
    shell.press(Action::Back);
    CHECK(shell.application->videoSource() == nullptr);
}

TEST_CASE("player: playing, the bar stays past its delay while the row has the focus, and hides once it is left") {
    Shell shell;
    shell.youtube.servePlayableMedia();
    shell.youtube.setPlayer("VISIONOS", shell.youtube.playerReply({137, 140}));
    shell.application->replace(app::makeTab(shell.session, app::Tab::Settings));
    app::openVideo(shell.session, "dQw4w9WgXcQ");
    shell.frames(10);
    REQUIRE(picture(shell));
    shell.press(Action::Down);
    shell.press(Action::Confirm);  // Play: it plays
    REQUIRE(focusedIcon(shell) != nullptr);
    REQUIRE(focusedIcon(shell)->label() == "Pause");
    shell.frames(300);  // 5 s on the frame clock, more than the bar's 4 s
    picture(shell);     // a frame of the player: where the bar would hide
    CHECK(shows(*shell.application, "Demo"));
    REQUIRE(focusedIcon(shell) != nullptr);
    shell.press(Action::Back);  // the row left: the bar hides after its delay, as usual
    CHECK(focusedIcon(shell) == nullptr);
    shell.frames(300);
    picture(shell);
    CHECK_FALSE(shows(*shell.application, "Demo"));
    shell.press(Action::Back);
}

TEST_CASE("player: with the next video's card shown and the row focused, ◯ cancels the card first") {
    Shell shell;
    shell.youtube.servePlayableMedia();
    shell.youtube.setPlayer("VISIONOS", shell.youtube.playerReply({137, 140}));
    youtube::Video queued;  // queued: the card shows near the end of even a short clip
    queued.id = "jICmkF585LA";
    queued.title = "Queued";
    shell.session.watchQueue().push_back(queued);
    shell.application->replace(app::makeTab(shell.session, app::Tab::Settings));
    app::openVideo(shell.session, "dQw4w9WgXcQ");
    shell.frames(10);
    REQUIRE(picture(shell));
    picture(shell);
    REQUIRE(shows(*shell.application, "Up next"));
    shell.press(Action::Down);
    REQUIRE(focusedIcon(shell) != nullptr);
    shell.press(Action::Back);  // the card first
    CHECK_FALSE(shows(*shell.application, "Up next"));
    REQUIRE(focusedIcon(shell) != nullptr);  // the row keeps the focus
    CHECK(focusedIcon(shell)->label() == "Play");
    shell.press(Action::Back);  // then the row
    CHECK(focusedIcon(shell) == nullptr);
    CHECK(shell.application->videoSource() != nullptr);
    shell.press(Action::Back);  // then the player
    CHECK(shell.application->videoSource() == nullptr);
}

TEST_CASE("player: saving to watch later or a playlist from the row, signed in") {
    Shell shell;
    signIn(shell);
    shell.youtube.servePlayableMedia();
    shell.youtube.setPlayer("VISIONOS", shell.youtube.playerReply({137, 140}));
    app::openVideo(shell.session, "dQw4w9WgXcQ");
    shell.frames(10);
    shell.press(Action::Up);
    pressAnywhere(shell, "Save");
    shell.frames(3);
    REQUIRE(shell.application->hasModal());
    CHECK(shows(*shell.application, "Save to\xE2\x80\xA6"));
    pressAnywhere(shell, "Playlist 2");
    shell.frames(3);
    const auto saved = shell.youtube.requestsTo("/youtubei/v1/browse/edit_playlist");
    REQUIRE(saved.size() == 1);
    CHECK(saved[0].body.find(R"("playlistId":"PLdemo02xxxxxxxxxxxxxxxxxxxxxxxxxx")") != std::string::npos);
    CHECK(saved[0].body.find(R"("addedVideoId":"dQw4w9WgXcQ")") != std::string::npos);
    REQUIRE(focusedIcon(shell) != nullptr);  // back on the icon
    CHECK(focusedIcon(shell)->label() == "Save");
    pressAnywhere(shell, "Save");
    shell.frames(3);
    pressAnywhere(shell, "\xC3\x80 regarder plus tard");
    shell.frames(3);
    const auto later = shell.youtube.requestsTo("/youtubei/v1/browse/edit_playlist");
    REQUIRE(later.size() == 2);
    CHECK(later[1].body.find(R"("playlistId":"WL")") != std::string::npos);
}

TEST_CASE("player: rating from the row, pressing the active one takes it back; subscribing to the channel") {
    Shell shell;
    signIn(shell);
    shell.youtube.servePlayableMedia();
    shell.youtube.setPlayer("VISIONOS", shell.youtube.playerReply({137, 140}));
    app::openVideo(shell.session, "dQw4w9WgXcQ");
    shell.frames(10);
    shell.press(Action::Up);
    const std::string like = "Like \xC2\xB7 19\xC2\xA0M";  // with the likes YouTube counts
    pressAnywhere(shell, like);
    shell.frames(3);
    CHECK(shell.youtube.requestsTo("/youtubei/v1/like/like").size() == 1);
    REQUIRE(focusedIcon(shell) != nullptr);
    CHECK(focusedIcon(shell)->icon() == ui::Icon::LikeFilled);  // the active rating, filled
    pressAnywhere(shell, like);
    shell.frames(3);
    CHECK(shell.youtube.requestsTo("/youtubei/v1/like/removelike").size() == 1);
    pressAnywhere(shell, "Dislike \xC2\xB7 521K");  // with Return YouTube Dislike's count
    shell.frames(3);
    CHECK(shell.youtube.requestsTo("/youtubei/v1/like/dislike").size() == 1);
    REQUIRE(focusedIcon(shell) != nullptr);
    CHECK(focusedIcon(shell)->icon() == ui::Icon::DislikeFilled);
    pressAnywhere(shell, "Subscribe");
    shell.frames(3);
    REQUIRE(focusedIcon(shell) != nullptr);
    CHECK(focusedIcon(shell)->icon() == ui::Icon::BellTicked);
    const auto subscribed = shell.youtube.requestsTo("/youtubei/v1/subscription/subscribe");
    REQUIRE(subscribed.size() == 1);
    CHECK(subscribed[0].body.find(R"("channelIds":["UCSJ4gkVC6NrvII8umztf0Ow"])") != std::string::npos);
    pressAnywhere(shell, "Subscribed");
    shell.frames(3);
    CHECK(shell.youtube.requestsTo("/youtubei/v1/subscription/unsubscribe").size() == 1);
}

TEST_CASE("player: signed out, the row has Play, Comments, Channel and Settings; the votes in the channel line") {
    Shell shell;
    shell.youtube.servePlayableMedia();
    std::string reply = shell.youtube.playerReply({137, 140});
    SUBCASE("without captions") {
        shell.youtube.setPlayer("VISIONOS", reply);
        app::openVideo(shell.session, "dQw4w9WgXcQ");
        shell.frames(10);
        REQUIRE(picture(shell));
        CHECK(rowLabels(shell) == Labels{"Play", "Comments", "Channel", "Settings"});
        CHECK(shows(*shell.application, "Lofi Girl \xC2\xB7 19M likes \xC2\xB7 521K dislikes"));
        CHECK(findPressable(shell.application->root(), "Like") == nullptr);
        CHECK(findPressable(shell.application->root(), "Save") == nullptr);
        CHECK(findPressable(shell.application->root(), "Subscribe") == nullptr);
    }
    SUBCASE("with captions: Subtitles before Settings") {
        reply.insert(reply.size() - 1, R"(,"captions":{"playerCaptionsTracklistRenderer":{"captionTracks":[{"baseUrl":")" +
                                           shell.youtube.baseUrl() +
                                           R"(/captions?lang=en","languageCode":"en","name":{"simpleText":"English"}}]}})");
        shell.youtube.setPlayer("VISIONOS", reply);
        app::openVideo(shell.session, "dQw4w9WgXcQ");
        shell.frames(10);
        REQUIRE(picture(shell));
        CHECK(rowLabels(shell) == Labels{"Play", "Comments", "Channel", "Subtitles", "Settings"});
        shell.press(Action::Down);
        pressAnywhere(shell, "Subtitles");
        REQUIRE(shell.application->hasModal());
        CHECK_FALSE(shows(*shell.application, "Demo"));  // the bar hidden behind the chooser
        backToIcon(shell, "Subtitles");
        CHECK(shows(*shell.application, "Demo"));
    }
}

TEST_CASE("player: signed in, Like, Dislike, Save and Subscribe join the row, with the counts in their names") {
    Shell shell;
    signIn(shell);
    shell.youtube.servePlayableMedia();
    shell.youtube.setPlayer("VISIONOS", shell.youtube.playerReply({137, 140}));
    app::openVideo(shell.session, "dQw4w9WgXcQ");
    shell.frames(10);
    REQUIRE(picture(shell));
    CHECK(rowLabels(shell) == Labels{"Play", "Like \xC2\xB7 19\xC2\xA0M", "Dislike \xC2\xB7 521K", "Comments", "Save",
                                     "Subscribe", "Channel", "Settings"});
    CHECK(shows(*shell.application, "Lofi Girl"));  // the counts are on the buttons
}

TEST_CASE("player: From the start after Play when resumed; the queue before Channel; a live's chat before Comments") {
    Shell shell;
    shell.youtube.servePlayableMedia();
    shell.youtube.setPlayer("VISIONOS", shell.youtube.playerReply({137, 140}));
    SUBCASE("resumed") {
        shell.session.progress().set("dQw4w9WgXcQ", 125, 600, 2);
        app::openVideo(shell.session, "dQw4w9WgXcQ");
        shell.frames(10);
        CHECK(rowLabels(shell) == Labels{"Play", "From the start", "Comments", "Channel", "Settings"});
    }
    SUBCASE("a queue") {
        youtube::Video queued;
        queued.id = "jICmkF585LA";
        queued.title = "Queued";
        shell.session.watchQueue().push_back(queued);
        app::openVideo(shell.session, "dQw4w9WgXcQ");
        shell.frames(10);
        CHECK(rowLabels(shell) == Labels{"Play", "Comments", "Queue (1)", "Channel", "Settings"});
        shell.press(Action::Down);
        pressAnywhere(shell, "Queue (1)");
        REQUIRE(shell.application->hasModal());
        CHECK_FALSE(shows(*shell.application, "Demo"));  // the bar hidden behind the dialog
        pressAnywhere(shell, "Close");
        CHECK_FALSE(shell.application->hasModal());
        REQUIRE(focusedIcon(shell) != nullptr);
        CHECK(focusedIcon(shell)->label() == "Queue (1)");
    }
    SUBCASE("a live with a chat: no times") {
        shell.youtube.setPlayer(
            "VISIONOS", R"({"playabilityStatus":{"status":"OK"},"videoDetails":{"title":"Live","isLive":true,)"
                        R"("channelId":"UCSJ4gkVC6NrvII8umztf0Ow"},"streamingData":{"hlsManifestUrl":")" +
                            shell.youtube.baseUrl() + R"(/hls/live.m3u8"}})");
        app::openVideo(shell.session, "dQw4w9WgXcQ");
        shell.frames(10);
        picture(shell);
        CHECK(rowLabels(shell) == Labels{"Play", "Live chat", "Comments", "Channel", "Settings"});
        CHECK(shows(*shell.application, "LIVE"));
        CHECK_FALSE(showsSomewhere(*shell.application, "Ends at"));
        CHECK_FALSE(showsSomewhere(*shell.application, "0:00"));
        shell.press(Action::Down);
        pressAnywhere(shell, "Live chat");
        REQUIRE(shell.application->hasModal());
        CHECK_FALSE(shows(*shell.application, "LIVE"));  // the bar hidden behind the chat
        backToIcon(shell, "Live chat");
        CHECK(shows(*shell.application, "LIVE"));
        // its settings: no Quality or Speed on a live
        pressAnywhere(shell, "Settings");
        REQUIRE(shell.application->hasModal());
        CHECK(settingRows(shell) == Rows{{"Volume", "100 %"}, {"Picture", "Whole picture"}});
        backToIcon(shell, "Settings");
    }
}

TEST_CASE("player: the comments in a panel over the playing video; a comment's replies; \xE2\x97\x8B closes it") {
    Shell shell;
    shell.youtube.servePlayableMedia();
    shell.youtube.setPlayer("VISIONOS", shell.youtube.playerReply({137, 140}));
    app::openVideo(shell.session, "dQw4w9WgXcQ");
    shell.frames(10);
    shell.press(Action::Up);
    pressAnywhere(shell, "Comments");
    shell.frames(5);
    REQUIRE(shell.application->hasModal());
    CHECK_FALSE(shows(*shell.application, "Demo"));  // the bar hidden behind the panel: no icons under its hints
    CHECK(showsSomewhere(*shell.application, "Premier commentaire de d\xC3\xA9monstration"));
    CHECK(shows(*shell.application, "@auteur1 \xC2\xB7 il y a 6 ans (modifi\xC3\xA9)"));
    CHECK(shows(*shell.application, "104\xC2\xA0k likes \xC2\xB7 611 replies"));
    CHECK(shell.application->videoSource() != nullptr);  // the video plays on under the panel
    shell.press(Action::Confirm);  // the first comment's replies
    shell.frames(5);
    CHECK(showsSomewhere(*shell.application, "R\xC3\xA9ponse "));
    for (int i = 0; i < 6; ++i) {
        shell.press(Action::Down);  // near the end: the next page
    }
    shell.frames(5);
    std::size_t continuations = 0;
    for (const auto& request : shell.youtube.requestsTo("/youtubei/v1/next")) {
        continuations += request.body.find("\"continuation\"") != std::string::npos ? 1 : 0;
    }
    CHECK(continuations >= 3);  // first page, replies, next page
    backToIcon(shell, "Comments");
    CHECK(shows(*shell.application, "Demo"));  // the bar again, with the focus on its icon
    CHECK(shell.application->videoSource() != nullptr);
}

TEST_CASE("player: a video page that did not come is asked again by itself (else nothing would play next)") {
    Shell shell;
    shell.youtube.servePlayableMedia();
    shell.youtube.setPlayer("VISIONOS", shell.youtube.playerReply({137, 140}));
    shell.youtube.failNext("/youtubei/v1/next", 500);
    app::openVideo(shell.session, "dQw4w9WgXcQ");
    shell.frames(10);
    REQUIRE(picture(shell));
    CHECK(shell.youtube.requestsTo("/youtubei/v1/next").size() == 1);
    CHECK_FALSE(showsSomewhere(*shell.application, "Lofi Girl"));  // the channel's name comes with the page
    const auto play = [&shell](int frames) {  // the player's own clock runs when its picture is asked for
        for (int i = 0; i < frames; ++i) {
            shell.frames(1);
            picture(shell);
        }
    };
    play(200);  // 3 s: not yet
    CHECK(shell.youtube.requestsTo("/youtubei/v1/next").size() == 1);
    play(200);  // past 5 s
    CHECK(shell.youtube.requestsTo("/youtubei/v1/next").size() == 2);
    CHECK(showsSomewhere(*shell.application, "Lofi Girl"));
    play(800);  // it came: not asked again
    CHECK(shell.youtube.requestsTo("/youtubei/v1/next").size() == 2);
}

TEST_CASE("player: a video page that never comes is asked three times in all, then left alone") {
    Shell shell;
    shell.youtube.servePlayableMedia();
    shell.youtube.setPlayer("VISIONOS", shell.youtube.playerReply({137, 140}));
    shell.youtube.failNext("/youtubei/v1/next", 500, 10);
    app::openVideo(shell.session, "dQw4w9WgXcQ");
    shell.frames(10);
    REQUIRE(picture(shell));
    for (int i = 0; i < 2500; ++i) {  // well past 5 s then 10 s
        shell.frames(1);
        picture(shell);
    }
    CHECK(shell.youtube.requestsTo("/youtubei/v1/next").size() == 3);
}

TEST_CASE("player: the video that starts is remembered as played in this session") {
    Shell shell;
    shell.youtube.servePlayableMedia();
    shell.youtube.setPlayer("VISIONOS", shell.youtube.playerReply({137, 140}));
    CHECK(shell.session.played().empty());
    app::openVideo(shell.session, "dQw4w9WgXcQ");
    shell.frames(5);
    CHECK(shell.session.played() == std::vector<std::string>{"dQw4w9WgXcQ"});
}

TEST_CASE("player: comments when the video page failed the first time: asked again on \"Comments\"") {
    Shell shell;
    shell.youtube.servePlayableMedia();
    shell.youtube.setPlayer("VISIONOS", shell.youtube.playerReply({137, 140}));
    shell.youtube.failNext("/youtubei/v1/next", 500);
    app::openVideo(shell.session, "dQw4w9WgXcQ");
    shell.frames(10);
    shell.press(Action::Up);
    pressAnywhere(shell, "Comments");
    shell.frames(5);
    REQUIRE(shell.application->hasModal());
    CHECK(showsSomewhere(*shell.application, "Premier commentaire de d\xC3\xA9monstration"));
}

TEST_CASE("player: a playlist's \"Play all\" plays its first video; a card plays from that video") {
    Shell shell;
    shell.youtube.servePlayableMedia();
    shell.youtube.setPlayer("VISIONOS", shell.youtube.playerReply({137, 140}));
    youtube::Video playlist;
    playlist.playlist = true;
    playlist.id = "PLaklDK5Yxuss";
    playlist.title = "Lofi House 2026";
    playlist.videoCountText = "198 vid\xC3\xA9os";
    app::openPlaylist(shell.session, playlist);
    shell.frames(5);
    CHECK(shows(*shell.application, "Lofi House 2026"));
    CHECK(shows(*shell.application, "198 vid\xC3\xA9os"));
    pressAnywhere(shell, "Play all");
    shell.frames(5);
    auto players = shell.youtube.requestsTo("/youtubei/v1/player");
    REQUIRE_FALSE(players.empty());
    CHECK(players.back().body.find("jICmkF585LA") != std::string::npos);
    shell.press(Action::Back);
    shell.frames(5);
}

TEST_CASE("player: a pasted link in the search opens the player") {
    Shell shell;
    shell.youtube.servePlayableMedia();
    shell.youtube.setPlayer("VISIONOS", shell.youtube.playerReply({137, 140}));
    shell.application->replace(app::makeTab(shell.session, app::Tab::Search));
    shell.frames(5);
    auto* field = findView<ui::TextField>(shell.application->root());
    REQUIRE(shell.application->focus().focus(field));
    typeText(shell, "https://youtu.be/dQw4w9WgXcQ");
    shell.frames(5);
    CHECK(shell.application->videoSource() != nullptr);
    const auto players = shell.youtube.requestsTo("/youtubei/v1/player");
    REQUIRE_FALSE(players.empty());
    CHECK(players[0].body.find("dQw4w9WgXcQ") != std::string::npos);
}

TEST_CASE("player: ◯ on the error leaves the player (no dead screen behind a dismissed dialog)") {
    Shell shell;
    shell.application->replace(app::makeTab(shell.session, app::Tab::Settings));
    app::openVideo(shell.session, "dQw4w9WgXcQ");  // refused by every client
    shell.frames(10);
    REQUIRE(shell.application->hasModal());
    shell.press(Action::Back);
    shell.frames(5);
    CHECK_FALSE(shell.application->hasModal());
    CHECK(shell.application->videoSource() == nullptr);
    CHECK(shows(*shell.application, "Accent colour"));  // back on the settings
}

TEST_CASE("player: a refusal without YouTube's words says only that YouTube refused it") {
    Shell shell;
    for (const char* client : {"VISIONOS", "ANDROID_VR", "IOS", "ANDROID"}) {
        shell.youtube.setPlayer(client, R"({"playabilityStatus":{"status":"UNPLAYABLE"}})");
    }
    app::openVideo(shell.session, "dQw4w9WgXcQ");
    shell.frames(10);
    CHECK(shows(*shell.application, "YouTube refused this video."));
}

TEST_CASE("player: the audio language chosen from the settings") {
    Shell shell;
    shell.youtube.servePlayableMedia();
    const std::string base = shell.youtube.baseUrl();
    const std::string clen = "clen=" + std::to_string(test::readFixture("media/clip.mp4").size());
    auto audio = [&](const char* lang, const char* name, bool original) {
        return std::string(R"({"itag":140,"url":")") + base + "/media/140?lang=" + lang + "&" + clen +
               R"(","mimeType":"audio/mp4; codecs=\"mp4a.40.2\"","audioTrack":{"id":")" + lang +
               R"(","displayName":")" + name + R"(","audioIsDefault":)" + (original ? "true" : "false") + "}}";
    };
    shell.youtube.setPlayer(
        "VISIONOS",
        R"({"playabilityStatus":{"status":"OK"},"videoDetails":{"title":"Demo","lengthSeconds":"4"},)"
        R"("streamingData":{"adaptiveFormats":[{"itag":137,"url":")" + base + "/media/137?" + clen +
            R"(","mimeType":"video/mp4; codecs=\"avc1.640028\"","height":1080},)" +
            audio("de.3", "Allemand", false) + "," + audio("en-US.4", "Anglais (US) original", true) + "]}}");
    shell.application->replace(app::makeTab(shell.session, app::Tab::Settings));
    app::openVideo(shell.session, "dQw4w9WgXcQ");
    shell.frames(10);
    shell.press(Action::Up);
    pressAnywhere(shell, "Settings");  // its panel: the audio track last, with several tracks
    REQUIRE(shell.application->hasModal());
    CHECK(settingRows(shell) == Rows{{"Quality", "1080p"},
                                     {"Speed", "Normal"},
                                     {"Volume", "100 %"},
                                     {"Picture", "Whole picture"},
                                     {"Audio track", "Anglais (US) original"}});
    pressAnywhere(shell, "Audio track");
    REQUIRE(shell.application->hasModal());
    CHECK(shows(*shell.application, "Anglais (US) original"));
    pressAnywhere(shell, "Allemand");
    REQUIRE(focusedLine(shell) != nullptr);  // back in the panel, on its line, with the new track
    CHECK(focusedLine(shell)->title() == "Audio track");
    CHECK(focusedLine(shell)->value() == "Allemand");
    bool german = false;
    for (int i = 0; i < 300 && !german; ++i) {  // mpv opens it on its own thread
        shell.frames(1);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        for (const auto& r : shell.youtube.requestsTo("/media/140")) {
            german = german || r.target.find("lang=de.3") != std::string::npos;
        }
    }
    CHECK(german);
}

TEST_CASE("player: resumed after its addresses expired, the video asks for new ones and goes on") {
    Shell shell;
    shell.youtube.servePlayableMedia();
    auto withExpiry = [&](const std::string& expire) {
        std::string reply = shell.youtube.playerReply({137, 140});
        for (std::size_t at = 0; (at = reply.find("?clen=", at)) != std::string::npos; at += 1) {
            reply.replace(at, 1, "?expire=" + expire + "&");
        }
        return reply;
    };
    shell.youtube.setPlayer("VISIONOS", withExpiry("4102444800"));  // 2100: still good
    shell.application->replace(app::makeTab(shell.session, app::Tab::Settings));
    app::openVideo(shell.session, "dQw4w9WgXcQ");
    shell.frames(10);
    REQUIRE(picture(shell));
    const std::size_t asked = shell.youtube.requestsTo("/youtubei/v1/player").size();
    shell.press(Action::Confirm);  // pause
    shell.press(Action::Confirm);  // play: the addresses still work, nothing asked
    shell.frames(3);
    CHECK(shell.youtube.requestsTo("/youtubei/v1/player").size() == asked);
    // the same addresses, now past their time (as after a night's pause)
    shell.youtube.setPlayer("VISIONOS", withExpiry("1"));
    shell.application->replace(app::makeTab(shell.session, app::Tab::Settings));
    app::openVideo(shell.session, "dQw4w9WgXcQ");
    shell.frames(10);
    REQUIRE(picture(shell));
    shell.youtube.setPlayer("VISIONOS", withExpiry("4102444800"));
    const std::size_t before = shell.youtube.requestsTo("/youtubei/v1/player").size();
    shell.press(Action::Confirm);  // pause
    shell.press(Action::Confirm);  // play
    shell.frames(10);
    CHECK(shell.youtube.requestsTo("/youtubei/v1/player").size() > before);
    CHECK_FALSE(shell.application->hasModal());
    CHECK(picture(shell));
}

TEST_CASE("player: a video starts again where it was left, and says so; its place is kept when it closes") {
    Shell shell;
    shell.youtube.servePlayableMedia();
    shell.youtube.setPlayer("VISIONOS", shell.youtube.playerReply({137, 140}));
    shell.session.progress().set("dQw4w9WgXcQ", 2, 4, 1);  // the clip is 4 s long: too short to resume
    shell.application->replace(app::makeTab(shell.session, app::Tab::Settings));
    app::openVideo(shell.session, "dQw4w9WgXcQ");
    shell.frames(10);
    REQUIRE(picture(shell));
    CHECK_MESSAGE(shell.application->toastText().find("Resumed") == std::string::npos, shell.application->toastText());  // within the first 30 s: from the start
    shell.press(Action::Back);
    shell.frames(3);
    CHECK(shell.session.progress().find("dQw4w9WgXcQ"));  // kept as it closed
    // a long video left at 2:05
    shell.session.progress().set("dQw4w9WgXcQ", 125, 600, 2);
    app::openVideo(shell.session, "dQw4w9WgXcQ");
    shell.frames(10);
    CHECK(shell.application->toastText() == "Resumed at 2:05");
    shell.press(Action::Back);
}

TEST_CASE("player: while seeking, YouTube's storyboard picture of the target shows above the bar") {
    Shell shell;
    shell.youtube.servePlayableMedia();
    std::string reply = shell.youtube.playerReply({137, 140});
    reply.insert(reply.size() - 1, R"(,"storyboards":{"playerStoryboardSpecRenderer":{"spec":")" +
                                       shell.youtube.baseUrl() +
                                       R"(/img/sb/$L/$N.jpg?x=1|160#90#2#5#5#2000#M$M#rs$S"}})");
    shell.youtube.setPlayer("VISIONOS", reply);
    shell.application->replace(app::makeTab(shell.session, app::Tab::Settings));
    app::openVideo(shell.session, "dQw4w9WgXcQ");
    shell.frames(10);
    REQUIRE(picture(shell));
    ui::View* found = anyNamed(*shell.application->top(), "trickplay preview");
    auto* preview = dynamic_cast<app::TrickplayPreview*>(found);
    REQUIRE(preview != nullptr);
    CHECK_FALSE(preview->visible);
    shell.press(Action::Right);
    shell.frames(1);
    CHECK(preview->visible);
    CHECK(preview->wantedUrl().find("/img/sb/0/M0.jpg?x=1&sigh=rs$S") != std::string::npos);
    shell.press(Action::Back);
}

TEST_CASE("player: a video's chapters show their title in the bar") {
    Shell shell;
    shell.youtube.servePlayableMedia();
    shell.youtube.setPlayer("VISIONOS", shell.youtube.playerReply({137, 140}));
    shell.youtube.setChapters({{0, "Intro"}, {1500, "Middle"}, {3000, "End"}});
    shell.application->replace(app::makeTab(shell.session, app::Tab::Settings));
    app::openVideo(shell.session, "dQw4w9WgXcQ");
    shell.frames(10);
    REQUIRE(picture(shell));
    CHECK((showsSomewhere(*shell.application, "Intro") || showsSomewhere(*shell.application, "Middle") ||
           showsSomewhere(*shell.application, "End")));  // the 4 s clip plays on in real time
    shell.press(Action::Back);
}

TEST_CASE("player: Return YouTube Dislike's counts in the channel line; none asked for when the setting is off") {
    Shell shell;
    shell.youtube.servePlayableMedia();
    shell.youtube.setPlayer("VISIONOS", shell.youtube.playerReply({137, 140}));
    shell.application->replace(app::makeTab(shell.session, app::Tab::Settings));
    app::openVideo(shell.session, "dQw4w9WgXcQ");
    shell.frames(10);
    REQUIRE(picture(shell));
    REQUIRE(shell.youtube.requestsTo("/votes").size() == 1);
    CHECK(shell.youtube.requestsTo("/votes")[0].target == "/votes?videoId=dQw4w9WgXcQ");
    // signed out: the counts in the channel line
    CHECK(showsSomewhere(*shell.application, "19M likes \xC2\xB7 521K dislikes"));
    shell.press(Action::Back);
    shell.session.settings().dislikes = false;
    app::openVideo(shell.session, "dQw4w9WgXcQ");
    shell.frames(10);
    CHECK(shell.youtube.requestsTo("/votes").size() == 1);  // not asked again
    shell.press(Action::Back);
}

TEST_CASE("queue: △ on a card queues the video; at the end of a video the queue's first plays, before À suivre") {
    Shell shell;
    shell.youtube.servePlayableMedia();
    shell.youtube.setPlayer("VISIONOS", shell.youtube.playerReply({137, 140}));
    shell.application->replace(app::makeTab(shell.session, app::Tab::Home));
    shell.frames(5);
    std::vector<app::VideoCard*> cards;
    findViews<app::VideoCard>(shell.application->root(), cards);
    for (app::VideoCard* card : cards) {  // the first card that is not a live (a live cannot be queued)
        REQUIRE(shell.application->focus().focus(card));
        shell.press(Action::Options);
        if (!shell.session.watchQueue().empty()) {
            break;
        }
    }
    REQUIRE(shell.session.watchQueue().size() == 1);
    CHECK(shell.application->toastText() == "Added to the queue \xC2\xB7 1");
    shell.press(Action::Options);  // not twice
    CHECK(shell.session.watchQueue().size() == 1);
    const std::string queued = shell.session.watchQueue().front().id;
    app::openVideo(shell.session, "dQw4w9WgXcQ");
    shell.frames(10);
    REQUIRE(picture(shell));
    shell.press(Action::Up);  // the row shows the queue
    CHECK(findPressable(shell.application->root(), "Queue (1)") != nullptr);
    shell.press(Action::Confirm);  // its first icon, Play: tests open a video paused on its first picture
    // the clip (4 s) plays to its end: the queued video comes next, and leaves the queue
    bool replaced = false;
    for (int i = 0; i < 1000 && !replaced; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        shell.frames(1);
        picture(shell);
        for (const auto& r : shell.youtube.requestsTo("/youtubei/v1/player")) {
            replaced = replaced || r.body.find(queued) != std::string::npos;
        }
    }
    CHECK(replaced);
    CHECK(shell.session.watchQueue().empty());
}

TEST_CASE("player: signed in, the video goes into the account's YouTube history; not with the setting off") {
    Shell shell;
    shell.youtube.servePlayableMedia();
    shell.youtube.setPlayer("VISIONOS", shell.youtube.playerReply({137, 140}));
    shell.application->replace(app::makeTab(shell.session, app::Tab::Settings));
    app::openVideo(shell.session, "dQw4w9WgXcQ");  // signed out: nothing reported
    shell.frames(10);
    REQUIRE(picture(shell));
    shell.press(Action::Back);
    CHECK(shell.youtube.requestsTo("/api/stats/playback").empty());

    signIn(shell);
    app::openVideo(shell.session, "dQw4w9WgXcQ");
    shell.frames(10);
    REQUIRE(picture(shell));
    const auto players = shell.youtube.requestsTo("/youtubei/v1/player");
    REQUIRE_FALSE(players.empty());
    CHECK(players.back().clientName == "TVHTML5");  // the account's reporting addresses
    CHECK(players.back().body.find("signatureTimestamp") != std::string::npos);
    auto started = shell.youtube.requestsTo("/api/stats/playback");
    REQUIRE(started.size() == 1);
    CHECK(started[0].authorization == "Bearer demo-access-token");
    CHECK(started[0].target.find("docid=dQw4w9WgXcQ") != std::string::npos);
    const auto cpn = started[0].target.find("&ver=2&cpn=");
    REQUIRE(cpn != std::string::npos);
    const std::string nonce = started[0].target.substr(cpn + 11, 16);
    shell.press(Action::Back);
    shell.frames(3);
    for (const auto& part : shell.youtube.requestsTo("/api/stats/watchtime")) {  // the same playback's parts
        CHECK(part.target.find("&cpn=" + nonce + "&") != std::string::npos);
        CHECK(part.authorization == "Bearer demo-access-token");
    }

    shell.session.settings().watchHistory = false;
    app::openVideo(shell.session, "dQw4w9WgXcQ");
    shell.frames(10);
    shell.press(Action::Back);
    CHECK(shell.youtube.requestsTo("/api/stats/playback").size() == 1);  // not reported again
}

TEST_CASE("player: the Channel icon shows a channel glyph until the channel's name is known, then its initial") {
    Shell shell;
    shell.youtube.servePlayableMedia();
    std::string reply = shell.youtube.playerReply({137, 140});
    reply.insert(reply.find("\"lengthSeconds\""), R"("channelId":"UCSJ4gkVC6NrvII8umztf0Ow",)");
    shell.youtube.setPlayer("VISIONOS", reply);
    const auto channelIcon = [&]() -> ui::IconButton* {
        for (ui::IconButton* icon : rowIcons(shell)) {
            if (icon->label() == "Channel") {
                return icon;
            }
        }
        return nullptr;
    };
    SUBCASE("the video page failed: no name, the glyph (never \"?\")") {
        shell.youtube.failNext("/youtubei/v1/next", 500);
        app::openVideo(shell.session, "dQw4w9WgXcQ");
        shell.frames(10);
        REQUIRE(channelIcon() != nullptr);
        CHECK(channelIcon()->initial().empty());
        CHECK(channelIcon()->icon() == ui::Icon::Channel);
    }
    SUBCASE("the video page answered: the initial") {
        app::openVideo(shell.session, "dQw4w9WgXcQ");
        shell.frames(10);
        REQUIRE(channelIcon() != nullptr);
        CHECK(channelIcon()->initial() == "L");
        // and the channel's own picture over it, in the icon and at the start of the channel line
        CHECK(channelIcon()->picture().rfind("https://", 0) == 0);
        std::vector<ui::ImageView*> images;
        findViews<ui::ImageView>(*shell.application->top(), images);
        int pictures = 0;
        for (const ui::ImageView* image : images) {
            pictures += image->url() == channelIcon()->picture() ? 1 : 0;
        }
        CHECK(pictures == 1);
    }
}

TEST_CASE("player: a long chapter title is cut with an ellipsis; \"Ends at\" stays whole on screen") {
    Shell shell;
    shell.youtube.servePlayableMedia();
    shell.youtube.setPlayer("VISIONOS", shell.youtube.playerReply({137, 140}));
    std::string longTitle;
    for (int i = 0; i < 12; ++i) {
        longTitle += "A chapter with a very long title ";
    }
    shell.youtube.setChapters({{0, longTitle}, {60000, "Next"}});  // two at least (one is none)
    shell.application->replace(app::makeTab(shell.session, app::Tab::Settings));
    app::openVideo(shell.session, "dQw4w9WgXcQ");
    shell.frames(10);
    REQUIRE(picture(shell));
    picture(shell);
    shell.frames(1);
    std::vector<ui::Label*> labels;
    findViews<ui::Label>(*shell.application->top(), labels);
    const ui::Label* rest = nullptr;
    const ui::Label* endsAt = nullptr;
    for (const ui::Label* label : labels) {
        if (label->text().rfind("/ ", 0) == 0) {
            rest = label;
        } else if (label->text().rfind("Ends at ", 0) == 0) {
            endsAt = label;
        }
    }
    REQUIRE(rest != nullptr);
    REQUIRE(endsAt != nullptr);
    INFO(rest->text());
    REQUIRE(rest->text().find("A chapter with") != std::string::npos);
    REQUIRE(rest->lines().size() == 1);
    const std::string& shown = rest->lines().front();
    CHECK(shown.size() >= 3);
    CHECK(shown.compare(shown.size() - 3, 3, "\xE2\x80\xA6") == 0);  // cut with an ellipsis
    REQUIRE(endsAt->lines().size() == 1);
    CHECK(endsAt->lines().front() == endsAt->text());  // whole
    // on screen, at the right margin, after the chapter
    const ui::Rect at = endsAt->absoluteFrame();
    CHECK(at.x + at.width <= shell.application->root().frame.width - 96 + 1);
    CHECK(rest->absoluteFrame().x + rest->absoluteFrame().width <= at.x);
    shell.press(Action::Back);
}

TEST_CASE("player: signed in, Like keeps a count when YouTube gives none: Return YouTube Dislike's") {
    Shell shell;
    signIn(shell);
    shell.youtube.hideLikeCount();
    shell.youtube.servePlayableMedia();
    shell.youtube.setPlayer("VISIONOS", shell.youtube.playerReply({137, 140}));
    app::openVideo(shell.session, "dQw4w9WgXcQ");
    shell.frames(10);
    REQUIRE(picture(shell));
    const Labels labels = rowLabels(shell);
    REQUIRE(labels.size() >= 2);
    CHECK(labels[1] == "Like \xC2\xB7 19M");
}
