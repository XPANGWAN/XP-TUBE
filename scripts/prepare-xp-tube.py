#!/usr/bin/env python3
"""Apply an auditable XP-TUBE identity, branding, and account-isolation patch."""
import json
import os
from pathlib import Path

ROOT = Path(os.environ.get("XP_TUBE_SOURCE_ROOT", Path(__file__).resolve().parents[1] / "upstream")).resolve()

FILES = {
    "src/core/version.cpp": [
        ('return "Playtube Dev";', 'return "XP-TUBE";'),
        ('return "PlaytubeDev";', 'return "XP-TUBE";'),
        ('return "PLTB00001";', 'return "XPTB00000";'),
        ('return "Playtube";', 'return "XP-TUBE";', 2),
        ('return "PLTB00000";', 'return "XPTB00000";'),
    ],
    "cmake/ps4_pkg.cmake": [
        ('set(title_id "PLTB00001")', 'set(title_id "XPTB00000")'),
        ('set(title "Playtube Dev")', 'set(title "XP-TUBE")'),
        ('set(title_id "PLTB00000")', 'set(title_id "XPTB00000")'),
        ('set(title "Playtube")', 'set(title "XP-TUBE")'),
    ],
    "scripts/build-ps4.sh": [
        ('"$ROOT"/build/ps4/pkg/IV0001-PLTB*.pkg', '"$ROOT"/build/ps4/pkg/IV0001-XPTB*.pkg'),
        ('IV0001-PLTB00000_00-PLTB000000100000.pkg', 'IV0001-XPTB00000_00-XPTB000000100000.pkg'),
        ('IV0001-PLTB00001_00-PLTB000010100000.pkg', 'IV0001-XPTB00000_00-XPTB000000100000.pkg'),
    ],
    "src/app/shell/chrome.cpp": [
        ('add<ui::Label>("XP-TUBE", style);', 'gap = ui::theme().spacingS / 2;\n        add<AccentLabel>("XP", style);\n        add<ui::Label>("TUBE", style);'),
    ],
    "src/app/shell/row_screens.cpp": [
        ('#include <memory>', '#include <memory>\n#include <nanovg.h>'),
        ('// A tab of titled rows of videos, each fetched on its own and shown when it arrives; the reason and "Réessayer"\n// when none came.\nclass RowsScreen : public ShellScreen {',
         '''// XP-TUBE's home showcase: a cinematic introduction above the live YouTube feed.
class HomeShowcase : public ui::Box {
public:
    HomeShowcase() : ui::Box(ui::Axis::Column) {
        width = ui::SizeSpec::fill();
        padding = {28, 36, 28, 36};
        gap = ui::theme().spacingS;
        cornerRadius = ui::theme().radius * 1.5f;
        auto* brand = add<ui::Label>("XP-TUBE", ui::FontStyle{ui::theme().fontSmall, ui::FontWeight::Bold});
        brand->color = ui::theme().accent;
        auto* title = add<ui::Label>(ui::tr("shell.tab.home"), ui::FontStyle{48, ui::FontWeight::Bold});
        title->color = ui::theme().text;
        auto* subtitle = add<ui::Label>(ui::tr("shell.search.title"), ui::FontStyle{ui::theme().fontBody, ui::FontWeight::Regular});
        subtitle->color = ui::theme().textSecondary;
        auto* hint = add<ui::Label>(ui::tr("shell.tab.search"), ui::FontStyle{ui::theme().fontSmall, ui::FontWeight::Bold});
        hint->color = ui::theme().accent;
    }

    void draw(NVGcontext* vg) const override {
        const float radius = ui::theme().radius * 1.5f;
        nvgBeginPath(vg);
        nvgRoundedRect(vg, 0, 0, frame.width, frame.height, radius);
        const NVGpaint fill = nvgLinearGradient(vg, 0, 0, frame.width, frame.height,
                                                nvgRGBA(28, 25, 39, 255),
                                                nvgRGBA(10, 11, 16, 255));
        nvgFillPaint(vg, fill);
        nvgFill(vg);
        ui::Box::draw(vg);
        nvgBeginPath(vg);
        nvgRoundedRect(vg, 1, 1, frame.width - 2, frame.height - 2, radius);
        nvgStrokeColor(vg, nvgRGBA(184, 192, 204, 220));
        nvgStrokeWidth(vg, 1.5f);
        nvgStroke(vg);
    }
};

// A tab of titled rows of videos, each fetched on its own and shown when it arrives; the reason and "Réessayer"
// when none came.
class RowsScreen : public ShellScreen {'''),
        ('        setStart(bar->currentTab());\n        rows_ = column_->add<ui::Box>(ui::Axis::Column);',
         '        setStart(bar->currentTab());\n        if (tab == Tab::Home) {\n            column_->add<HomeShowcase>();\n        }\n        rows_ = column_->add<ui::Box>(ui::Axis::Column);'),
    ],
    "src/app/shell/nav_bar.cpp": [
        ('NavBar::NavBar(AppSession& session, Tab current) : ui::Box(ui::Axis::Row) {\n    width = ui::SizeSpec::fill();',
         'NavBar::NavBar(AppSession& session, Tab current) : ui::Box(ui::Axis::Row) {\n    width = ui::SizeSpec::fill();\n    padding = {ui::theme().spacingS, ui::theme().spacingL, ui::theme().spacingS, ui::theme().spacingL};\n    background = ui::theme().surface;\n    cornerRadius = ui::theme().radius * 1.35f;'),
    ],
    "src/app/shell/cards.cpp": [
        ('constexpr float kImageWidth = 400;\nconstexpr float kImageHeight = 225;\nconstexpr float kTitleHeight = 64;  // two lines\nconstexpr float kGlowFeather = 50;',
         'constexpr float kImageWidth = 360;\nconstexpr float kImageHeight = 203;\nconstexpr float kTitleHeight = 58;  // two lines\nconstexpr float kGlowFeather = 36;'),
    ],
    "src/app/shell/player_screen.cpp": [
        ('if (!streams.ok()) {\n            PF_LOG_WARN("player: %s: %s", videoId_.c_str(), streams.error().message.c_str());',
         'if (!streams.ok()) {\n            streamRecoveryInFlight_ = false;\n            PF_LOG_WARN("player: %s: %s", videoId_.c_str(), streams.error().message.c_str());'),
        ('case player::EngineEvent::FirstFrame:\n            PF_LOG_INFO("player: %s", engine_->describe().c_str());',
         'case player::EngineEvent::FirstFrame:\n            streamRecoveryInFlight_ = false;\n            PF_LOG_INFO("player: %s", engine_->describe().c_str());'),
        ('case player::EngineEvent::Error:\n            PF_LOG_WARN("player: %s: %s", videoId_.c_str(), engine_->state().error.c_str());\n            showError(core::makeError(core::ErrorKind::Network, engine_->state().error));  // (the streams were checked)\n            break;',
         'case player::EngineEvent::Error:\n            PF_LOG_WARN("player: %s: %s", videoId_.c_str(), engine_->state().error.c_str());\n            if (!streamRecoveryInFlight_) {\n                // A stream URL can expire or be cut even after its initial reachability probe.\n                // Refresh it once automatically, preserving the last known playback position.\n                streamRecoveryInFlight_ = true;\n                lastPosition_ = std::max(lastPosition_, engine_->state().position);\n                PF_LOG_WARN("player: %s: refreshing a failed stream once", videoId_.c_str());\n                restart();\n                showBar();\n            } else {\n                streamRecoveryInFlight_ = false;\n                showError(core::makeError(core::ErrorKind::Network, engine_->state().error));\n            }\n            break;'),
        ('    bool frameShown_ = false;\n    bool errorShown_ = false;',
         '    bool frameShown_ = false;\n    bool streamRecoveryInFlight_ = false;  // one automatic fresh-URL attempt until a frame plays\n    bool errorShown_ = false;'),
        ('const ui::Color kPlayedColor{0xFF, 0x00, 0x33, 0xFF};   // YouTube red: the part played and the head', "// The played portion and head follow XP-TUBE's selected accent colour."),
        ('ui::nvgColor(kPlayedColor)', 'ui::nvgColor(ui::theme().accent)', 2),
    ],
    "src/app/gallery.cpp": [
        ('"Playtube toolkit"', '"XP-TUBE"'),
    ],
    "src/platform/sdl_platform.cpp": [
        ('core::dataPath("playtube.log")', 'core::dataPath("xp-tube.log")'),
    ],
    "src/core/boot_trace.cpp": [
        ('playtube boot:', 'XP-TUBE boot:', 2),
    ],
    "scripts/make-ps4-art.py": [
        ("Playtube", "XP-TUBE", 7),
        ("RED = (0xFF, 0x00, 0x33)", "RED = (0xB8, 0xC0, 0xCC)"),
        ("WHITE = (0xFF, 0xFF, 0xFF)", "WHITE = (0x8E, 0x4E, 0xC6)"),
        ('icon = Image.new("RGB", (512, 512), WHITE)', 'icon = Image.new("RGB", (512, 512), BACKGROUND)'),
        ("draw_name(icon, (512 - name_width(font(size), size)) / 2, 452, size, INK)", "draw_name(icon, (512 - name_width(font(size), size)) / 2, 452, size, LIGHT_TEXT)"),
    ],

    "src/youtube/account.hpp": [
        ('    std::int64_t expiresAt_ = 0;', '    std::int64_t expiresAt_ = 0;\n    std::uint64_t generation_ = 0;  // rejects stale refresh replies after sign-in/sign-out changes'),
    ],
    "src/youtube/account.cpp": [
        ('    imported_ = json::boolean(doc->root(), "imported", false);',
         '    imported_ = json::boolean(doc->root(), "imported", false);\n    ++generation_;'),
        ('    std::lock_guard<std::mutex> lock(mutex_);\n    if (!token.refreshToken.empty()) {',
         '    std::lock_guard<std::mutex> lock(mutex_);\n    ++generation_;\n    if (!token.refreshToken.empty()) {'),
        ('    std::string refreshToken;\n    {\n        std::lock_guard<std::mutex> lock(mutex_);',
         '    std::string refreshToken;\n    std::uint64_t generation = 0;\n    {\n        std::lock_guard<std::mutex> lock(mutex_);'),
        ('        refreshToken = refreshToken_;\n    }\n    const auto renewed = auth.refresh(refreshToken, cancel);',
         '        refreshToken = refreshToken_;\n        generation = generation_;\n    }\n    const auto renewed = auth.refresh(refreshToken, cancel);'),
        ('        if (renewed.error().kind == core::ErrorKind::Unauthorized && refreshToken_ == refreshToken) {',
         '        if (generation_ == generation && renewed.error().kind == core::ErrorKind::Unauthorized && refreshToken_ == refreshToken) {'),
        ('    if (refreshToken_ != refreshToken) {  // signed out or in again meanwhile',
         '    if (generation_ != generation || refreshToken_ != refreshToken) {  // account changed while renewal was in flight'),
        ('    std::string refreshToken = std::exchange(refreshToken_, {});',
         '    ++generation_;  // invalidate any refresh request already in flight\n    std::string refreshToken = std::exchange(refreshToken_, {});'),
        ('        refreshToken_ = token;\n        PF_LOG_INFO("account: sign-in imported from the previous Playtube");',
         '        ++generation_;  // invalidate any refresh request before changing credentials\n        refreshToken_ = token;\n        PF_LOG_INFO("account: sign-in imported from the previous Playtube");'),
    ],
    "tests/unit/youtube_account_test.cpp": [
        ('TEST_CASE("account: forgetting signs out at once and hands back the token to revoke") {',
         'TEST_CASE("account: a stale refresh cannot overwrite a newer sign-in with the same refresh token") {\n    FakeYouTube yt;\n    yt.setTokenDelay(400);\n    const auto http = testHttp();\n    const Auth auth(http, at(yt));\n    TempDir dir;\n    Account account(dir.path + "/account.json");\n    account.signIn(tokens("old-access", "same-refresh-token"), "Old", 1000);\n    bool staleRejected = false;\n    std::thread renewing([&] {\n        const auto result = account.accessToken(auth, 1000 + 4000);\n        staleRejected = !result.ok();\n    });\n    std::this_thread::sleep_for(std::chrono::milliseconds(100));\n    account.signIn(tokens("new-access", "same-refresh-token"), "New", 2000);\n    renewing.join();\n    CHECK(staleRejected);\n    CHECK(account.name() == "New");\n    const auto current = account.accessToken(auth, 2010);\n    REQUIRE(current.ok());\n    CHECK(current.value() == "new-access");\n}\n\nTEST_CASE("account: forgetting signs out at once and hands back the token to revoke") {'),
    ],
    "src/app/settings.hpp": [
        ('std::string accent = "white";', 'std::string accent = "violet";'),
    ],
    "src/app/shell/settings_screen.cpp": [
        ('// The banner at the top of the settings while a newer Playtube is published:', '// The banner at the top of the settings while a newer XP-TUBE release is published:'),
    ],
    "src/ui/theme.hpp": [
        ("// Design tokens of the Playtube identity (Playfin's look in YouTube red; one look, no theme setting): widgets read these, never\n// hard-coded values.", "// XP-TUBE design tokens: a dark neutral base with a user-selectable accent; widgets read these, never hard-coded values."),
        ('std::string id = "projecteur";  // the look (Playtube has one); derived pictures are kept per look', 'std::string id = "xp-tube";  // derived pictures are cached separately for XP-TUBE'),
        ('Color accent{0xFF, 0xFF, 0xFF, 0xFF};  // the accent setting (white by default)', 'Color accent{0x8E, 0x4E, 0xC6, 0xFF};  // XP-TUBE violet default; user accent choices remain available'),
        ('Color onAccent{0x0A, 0x0A, 0x0C, 0xFF};', 'Color onAccent{0xFF, 0xFF, 0xFF, 0xFF};'),
    ],
    "src/app/shell/chrome.hpp": [
        ('// "Playtube." with the dot in the accent colour; the picture given to setLogoPicture instead while there is one\n// (the YouTube logo, when the user chose it in the settings). Every logo shown changes at once.', "// XP-TUBE wordmark with its accent-coloured mark; the optional YouTube logo is shown only when selected in settings."),
    ],
    "src/app/shell/session.hpp": [
        ("// The header's logo of the settings: the YouTube logo fetched (YouTube's own file, never shipped with Playtube)\n    // and drawn, or Playtube's; the text logo stays while it comes or when it cannot.", "// The header logo: XP-TUBE by default, or YouTube's own fetched logo when explicitly selected in settings."),
    ],
    "src/app/app.cpp": [
        ('PF_LOG_INFO("Playtube %s, language %s, data %s"', 'PF_LOG_INFO("XP-TUBE %s, language %s, data %s"'),
        ('session.checkForUpdate();  // the settings tab gets a dot when a newer Playtube is published', 'session.checkForUpdate();  // the settings tab gets a dot when a newer XP-TUBE release is published'),
    ],
}



# Screen-level XP-TUBE presentation: consistent editorial headers and clearer content hierarchy.
# Keep all existing controls, focus order, data loading, and navigation behavior unchanged.
FILES.update({
    "src/app/shell/search_screen.cpp": [
        ('        column_->add<NavBar>(session_, Tab::Search);\n        platform::TextInputRequest request;',
         '''        column_->add<NavBar>(session_, Tab::Search);
        auto* intro = column_->add<ui::Box>(ui::Axis::Column);
        intro->padding = {ui::theme().spacingL, ui::theme().spacingXl, ui::theme().spacingL, ui::theme().spacingXl};
        intro->background = ui::theme().surface;
        intro->cornerRadius = ui::theme().radius * 1.35f;
        intro->gap = ui::theme().spacingS;
        auto* brand = intro->add<ui::Label>("XP-TUBE", ui::FontStyle{ui::theme().fontSmall, ui::FontWeight::Bold});
        brand->color = ui::theme().accent;
        auto* heading = intro->add<ui::Label>(ui::tr("shell.search.title"), ui::FontStyle{ui::theme().fontTitle, ui::FontWeight::Bold});
        heading->color = ui::theme().text;
        auto* detail = intro->add<ui::Label>(ui::tr("search.filters"), bodyText());
        detail->color = ui::theme().textSecondary;
        platform::TextInputRequest request;'''),
    ],
    "src/app/shell/settings_screen.cpp": [
        ('// The banner at the top of the settings while a newer Playtube is published:', '// The banner at the top of the settings while a newer XP-TUBE release is published:'),
        ('        column->add<NavBar>(session_, Tab::Settings);\n        banner_ = column->add<UpdateBanner>',
         '''        column->add<NavBar>(session_, Tab::Settings);
        auto* intro = column->add<ui::Box>(ui::Axis::Column);
        intro->padding = {ui::theme().spacingL, ui::theme().spacingXl, ui::theme().spacingL, ui::theme().spacingXl};
        intro->background = ui::theme().surface;
        intro->cornerRadius = ui::theme().radius * 1.35f;
        intro->gap = ui::theme().spacingS;
        auto* brand = intro->add<ui::Label>("XP-TUBE", ui::FontStyle{ui::theme().fontSmall, ui::FontWeight::Bold});
        brand->color = ui::theme().accent;
        auto* heading = intro->add<ui::Label>(ui::tr("settings.display"), ui::FontStyle{ui::theme().fontTitle, ui::FontWeight::Bold});
        heading->color = ui::theme().text;
        banner_ = column->add<UpdateBanner>'''),
    ],
    "src/app/shell/channel_screen.cpp": [
        ('        header_->gap = ui::theme().spacingXl;\n        header_->crossAlign = ui::CrossAlign::Center;',
         '        header_->gap = ui::theme().spacingXl;\n        header_->padding = {ui::theme().spacingL, ui::theme().spacingXl, ui::theme().spacingL, ui::theme().spacingXl};\n        header_->background = ui::theme().surface;\n        header_->cornerRadius = ui::theme().radius * 1.35f;\n        header_->crossAlign = ui::CrossAlign::Center;'),
    ],
    "src/app/shell/playlist_screen.cpp": [
        ('        heading->width = ui::SizeSpec::fill();\n        heading->maxLines = 1;',
         '        heading->width = ui::SizeSpec::fill();\n        heading->padding = {ui::theme().spacingL, ui::theme().spacingXl, ui::theme().spacingL, ui::theme().spacingXl};\n        heading->background = ui::theme().surface;\n        heading->cornerRadius = ui::theme().radius * 1.35f;\n        heading->maxLines = 1;'),
        ('        details_->color = ui::theme().textSecondary;',
         '        details_->color = ui::theme().textSecondary;\n        details_->margin = {0, ui::theme().spacingXl, 0, ui::theme().spacingXl};'),
    ],
})



# Extend the same visual language to subscription and account-entry flows.
FILES.update({
    "src/app/shell/subscriptions_screen.cpp": [
        ('        auto* bar = column_->add<NavBar>(session_, Tab::Subscriptions);\n        setStart(bar->currentTab());',
         '''        auto* bar = column_->add<NavBar>(session_, Tab::Subscriptions);
        setStart(bar->currentTab());
        auto* intro = column_->add<ui::Box>(ui::Axis::Column);
        intro->padding = {ui::theme().spacingL, ui::theme().spacingXl, ui::theme().spacingL, ui::theme().spacingXl};
        intro->background = ui::theme().surface;
        intro->cornerRadius = ui::theme().radius * 1.35f;
        intro->gap = ui::theme().spacingS;
        auto* brand = intro->add<ui::Label>("XP-TUBE", ui::FontStyle{ui::theme().fontSmall, ui::FontWeight::Bold});
        brand->color = ui::theme().accent;
        auto* heading = intro->add<ui::Label>(ui::tr("shell.tab.subscriptions"), ui::FontStyle{ui::theme().fontTitle, ui::FontWeight::Bold});
        heading->color = ui::theme().text;'''),
    ],
    "src/app/shell/signin_screen.cpp": [
        ('        column->padding = {120, 160, 0, 160};\n        column->gap = ui::theme().spacingL;\n        column->add<ui::Label>(ui::tr("signin.title"),',
         '''        column->padding = {ui::theme().spacingXl, 160, ui::theme().spacingXl, 160};
        column->gap = ui::theme().spacingL;
        column->background = ui::theme().surface;
        column->cornerRadius = ui::theme().radius * 1.5f;
        auto* brand = column->add<ui::Label>("XP-TUBE", ui::FontStyle{ui::theme().fontSmall, ui::FontWeight::Bold});
        brand->color = ui::theme().accent;
        column->add<ui::Label>(ui::tr("signin.title"),'''),
    ],
})



# Give the other feed-based tabs their own concise, localized section heading.
FILES["src/app/shell/row_screens.cpp"].extend([
    ('// A tab of titled rows of videos, each fetched on its own and shown when it arrives; the reason and "Réessayer"\n// when none came.',
     '''class SectionShowcase : public ui::Box {
public:
    explicit SectionShowcase(const std::string& heading) : ui::Box(ui::Axis::Column) {
        width = ui::SizeSpec::fill();
        padding = {ui::theme().spacingL, ui::theme().spacingXl, ui::theme().spacingL, ui::theme().spacingXl};
        background = ui::theme().surface;
        cornerRadius = ui::theme().radius * 1.35f;
        gap = ui::theme().spacingS;
        auto* brand = add<ui::Label>("XP-TUBE", ui::FontStyle{ui::theme().fontSmall, ui::FontWeight::Bold});
        brand->color = ui::theme().accent;
        auto* title = add<ui::Label>(heading, ui::FontStyle{ui::theme().fontTitle, ui::FontWeight::Bold});
        title->color = ui::theme().text;
    }
};

// A tab of titled rows of videos, each fetched on its own and shown when it arrives; the reason and "Réessayer"
// when none came.'''),
    ('        if (tab == Tab::Home) {\n            column_->add<HomeShowcase>();\n        }',
     '''        if (tab == Tab::Home) {
            column_->add<HomeShowcase>();
        } else if (tab == Tab::Subscriptions) {
            column_->add<SectionShowcase>(ui::tr("shell.tab.subscriptions"));
        } else if (tab == Tab::Library) {
            column_->add<SectionShowcase>(ui::tr("shell.tab.library"));
        }'''),
])



# Make the active navigation destination unmistakable while keeping controller focus dominant.
FILES["src/app/shell/nav_bar.cpp"].append((
    """    background = focused_ ? ui::theme().controlFocused : ui::Color{0, 0, 0, 0};
    label_->color = focused_ ? ui::theme().onAccent : current_ ? ui::theme().text : ui::theme().textSecondary;""",
    """    background = focused_ ? ui::theme().controlFocused
                           : current_ ? ui::withAlpha(ui::theme().accent, 0x30) : ui::Color{0, 0, 0, 0};
    label_->color = focused_ ? ui::theme().onAccent : current_ ? ui::theme().accent : ui::theme().textSecondary;"""))

# Player settings must follow the user's selected accent instead of a fixed white highlight.
FILES["src/app/shell/player_settings.cpp"] = [
    ("background = focused_ ? kFocused : ui::Color{0, 0, 0, 0};",
     "background = focused_ ? ui::theme().accent : ui::Color{0, 0, 0, 0};"),
    ("name_->color = focused_ ? kOnFocused : ui::theme().text;\n    value_->color = focused_ ? kOnFocused : ui::theme().textSecondary;",
     "name_->color = focused_ ? ui::theme().onAccent : ui::theme().text;\n    value_->color = focused_ ? ui::theme().onAccent : ui::theme().textSecondary;"),
    ("card->cornerRadius = kRadius;",
     "card->cornerRadius = ui::theme().radius * 1.5f;"),
    ("    title->width = ui::SizeSpec::fill();",
     "    title->width = ui::SizeSpec::fill();\n    title->color = ui::theme().accent;"),
    ("constexpr float kRadius = 24;\n", ""),
    ("const ui::Color kFocused{0xFF, 0xFF, 0xFF, 0xFF};  // white, as the focused icon\n", ""),
    ("const ui::Color kOnFocused{0x0A, 0x0A, 0x0C, 0xFF};\n", ""),
]

# Refine playback side-panels with the same rounded, theme-aware surface treatment.
FILES.update({
    "src/app/shell/chat_panel.cpp": [
        ('    card_->background = ui::theme().surfaceRaised;  // opaque: the video plays on at the left\n    card_->padding = ui::Insets::all(ui::theme().spacingXl);',
         '    card_->background = ui::theme().surfaceRaised;  // opaque: the video plays on at the left\n    card_->cornerRadius = ui::theme().radius * 1.35f;\n    card_->clip = true;\n    card_->padding = ui::Insets::all(ui::theme().spacingXl);'),
    ],
    "src/app/shell/comments_panel.cpp": [
        ('    card_->background = ui::theme().surfaceRaised;  // opaque: the video plays on at the left\n    card_->padding = ui::Insets::all(ui::theme().spacingXl);',
         '    card_->background = ui::theme().surfaceRaised;  // opaque: the video plays on at the left\n    card_->cornerRadius = ui::theme().radius * 1.35f;\n    card_->clip = true;\n    card_->padding = ui::Insets::all(ui::theme().spacingXl);'),
    ],
})



# Carry the XP-TUBE identity into the idle screen without adding animation or extra load.
FILES["src/app/shell/screensaver.cpp"] = [
    ('''    column_->gap = ui::theme().spacingS;
    time_ = column_->add<ui::Label>("", ui::FontStyle{ui::theme().fontDisplay * 1.6f, ui::FontWeight::SemiBold});''',
     '''    column_->gap = ui::theme().spacingS;
    auto* brand = column_->add<ui::Label>("XP-TUBE", ui::FontStyle{ui::theme().fontSmall, ui::FontWeight::Bold});
    brand->color = ui::Color{0x9A, 0x9A, 0xA4, 255};
    time_ = column_->add<ui::Label>("", ui::FontStyle{ui::theme().fontDisplay * 1.6f, ui::FontWeight::SemiBold});'''),
]



# Refine video-card proportions and corner treatment while retaining the existing theme accent and focus glow.
FILES["src/app/shell/cards.cpp"].extend([
    ('    picture->cornerRadius = ui::theme().radius;',
     '    picture->cornerRadius = ui::theme().radius * 1.25f;'),
    ('    title_->margin.top = 12;',
     '    title_->margin.top = ui::theme().spacingM;'),
    ('    channel_->margin.top = 4;',
     '    channel_->margin.top = ui::theme().spacingS;'),
])

def replace_file(rel, replacements):
    path = ROOT / rel
    body = path.read_text(encoding="utf-8")
    for item in replacements:
        old, new = item[0], item[1]
        expected = item[2] if len(item) == 3 else 1
        count = body.count(old)
        if count != expected:
            raise SystemExit(f"{rel}: expected {expected} occurrences of {old!r}, found {count}")
        body = body.replace(old, new)
    path.write_text(body, encoding="utf-8")

for rel, replacements in FILES.items():
    replace_file(rel, replacements)

# Remove the obsolete, remotely replaceable YouTube logo path. XP-TUBE's wordmark is
# compiled into the shell; no logo URL, image cache, or logo preference is needed.
def remove_once(path, old, description):
    target = ROOT / path
    body = target.read_text(encoding="utf-8")
    count = body.count(old)
    if count != 1:
        raise SystemExit(f"{path}: expected one {description}, found {count}")
    target.write_text(body.replace(old, "", 1), encoding="utf-8")

remove_once("src/app/shell/session.hpp",
'''    // The header's logo of the settings: the YouTube logo fetched (YouTube's own file, never shipped with Playtube)
    // and drawn, or Playtube's; the text logo stays while it comes or when it cannot.
    std::string youtubeLogoUrl =
        "https://www.gstatic.com/youtube/img/branding/youtubelogo/svg/youtubelogo_dark.svg";
    void applyLogo();

''', "legacy remote logo API")
remove_once("src/app/shell/session.hpp", "    unsigned logoGeneration_ = 0;\n", "legacy logo generation state")
remove_once("src/app/shell/chrome.hpp",
'''// "Playtube." with the dot in the accent colour; the picture given to setLogoPicture instead while there is one
// (the YouTube logo, when the user chose it in the settings). Every logo shown changes at once.
std::unique_ptr<ui::View> makeLogo();
void setLogoPicture(std::shared_ptr<const ui::SvgPicture> picture);
bool logoPictureShown();
''', "replaceable logo declarations")
remove_once("src/app/settings.hpp", '    std::string logo = "xp-tube";        // XP-TUBE wordmark; legacy YouTube image selection is disabled\n', "obsolete logo setting")
remove_once("src/app/settings.cpp", '    writer.key("logo").value(settings.logo);\n', "obsolete logo serialization")
remove_once("tests/unit/settings_test.cpp", '    CHECK(settings.logo == "xp-tube");\n', "obsolete logo default assertion")
remove_once("tests/unit/shell_screens_test.cpp", '    CHECK(s.shell.session.settings().logo == "xp-tube");\n', "obsolete logo setting assertion")
remove_once("tests/unit/shell_screens_test.cpp",
'''''', "obsolete remote-logo behavior test")

# Remove the implementation body by its stable function boundary.
session_path = ROOT / "src/app/shell/session.cpp"
session_body = session_path.read_text(encoding="utf-8")
start = session_body.find("void AppSession::applyLogo() {")
if start < 0:
    raise SystemExit("src/app/shell/session.cpp: legacy applyLogo function not found")
end = session_body.find("\\n}  // namespace playtube::app", start)
if end < 0:
    raise SystemExit("src/app/shell/session.cpp: applyLogo function boundary not found")
session_path.write_text(session_body[:start] + session_body[end+1:], encoding="utf-8")

# Replace the logo renderer with a text-only XP-TUBE wordmark and remove its image cache.
chrome_path = ROOT / "src/app/shell/chrome.cpp"
chrome_body = chrome_path.read_text(encoding="utf-8")
start = chrome_body.find("namespace {\n\nstd::shared_ptr<const ui::SvgPicture>& logoPicture()")
end_marker = "std::unique_ptr<ui::View> makeAvatar("
end = chrome_body.find(end_marker, start)
if start < 0 or end < 0:
    raise SystemExit("src/app/shell/chrome.cpp: legacy logo image renderer boundary not found")
replacement = '''std::unique_ptr<ui::View> makeLogo() {
    auto logo = std::make_unique<ui::Box>(ui::Axis::Row);
    const ui::FontStyle style{ui::theme().fontLogo, ui::FontWeight::Bold};
    logo->gap = ui::theme().spacingS / 2;
    logo->add<AccentLabel>("XP", style);
    logo->add<ui::Label>("TUBE", style);
    return logo;
}

'''
chrome_body = chrome_body[:start] + replacement + chrome_body[end:]
chrome_path.write_text(chrome_body, encoding="utf-8")


# Keep upstream regression tests aligned with the deliberately independent XP-TUBE identity.
# These are behavioral expectations, not skipped tests: the new defaults, package namespace,
# wordmark, and no-migration policy are tested explicitly.
TEST_PATCHES = {
    "tests/unit/settings_test.cpp": [
        ('settings.accent == "white"', 'settings.accent == "violet"', 3),
    ],
    "tests/unit/shell_screens_test.cpp": [
        ('shows(*s.shell.application, "White")', 'shows(*s.shell.application, "Violet")'),
        ('"/pkg/Playtube-9.0.0.pkg"', '"/pkg/XP-TUBE-9.0.0.pkg"', 2),
        ('"Playtube 9.0.0 is available"', '"XP-TUBE 9.0.0 is available"'),
        ('"Playtube is up to date"', '"XP-TUBE is up to date"'),
    ],
    "tests/unit/theme_test.cpp": [
        ('playtube::app::Settings{}.accent == "white"', 'playtube::app::Settings{}.accent == "violet"'),
    ],
    "tests/unit/version_test.cpp": [
        ('"PLTB00000"', '"XPTB00000"'),
        ('"Playtube"', '"XP-TUBE"', 2),
    ],
    "tests/unit/youtube_session_test.cpp": [
        ("the previous Playtube: its sign-in and recent searches imported once, its file left as it is",
         "XP-TUBE never imports a previous Playtube account or search history"),
        ("CHECK(session.account().signedIn());", "CHECK_FALSE(session.account().signedIn());"),
        ('std::vector<std::string>{"mine", "lofi", "jazz"}', 'std::vector<std::string>{"mine"}', 2),
        ('        CHECK(session.settings().recentSearches == std::vector<std::string>{"mine"});\n    }\n    CHECK(test::readFile(old) == oldText);',
         '        CHECK(session.settings().recentSearches == std::vector<std::string>{"mine"});\n        REQUIRE(session.saveSettings());\n    }\n    CHECK(test::readFile(old) == oldText);'),
    ],
}
for rel, replacements in TEST_PATCHES.items():
    path = ROOT / rel
    body = path.read_text(encoding="utf-8")
    for item in replacements:
        before, after = item[0], item[1]
        expected = item[2] if len(item) == 3 else 1
        count = body.count(before)
        if count != expected:
            raise SystemExit(f"{rel}: expected {expected} test expectation occurrences of {before!r}, found {count}")
        body = body.replace(before, after)
    path.write_text(body, encoding="utf-8")

# The old migration block is removed entirely: no Playtube settings or tokens are read.
app = ROOT / "src/app/app.cpp"
body = app.read_text(encoding="utf-8")
old = '''#ifdef PLAYTUBE_PS4
        if (onlyProfile(consoleUser)) {
            session.importPrevious("/data/Playtube/config.json");  // the previous Playtube's settings, only read
        }
#endif
'''
if body.count(old) != 1:
    raise SystemExit("src/app/app.cpp: expected the single legacy Playtube migration block")
app.write_text(body.replace(old, "        // XP-TUBE: never read, import, or copy Playtube's settings or account tokens.\n"), encoding="utf-8")

# Harden the importer itself as well as removing its startup call. This prevents a
# future accidental call site from reading or copying Playtube settings/tokens.
session = ROOT / "src/app/shell/session.cpp"
session_body = session.read_text(encoding="utf-8")
old_importer = '''void AppSession::importPrevious(const std::string& oldConfigPath) {
    if (account_.lookedForOld()) {
        return;
    }
    account_.importFromOld(oldConfigPath);
    std::string text;
    if (core::readSmallFile(oldConfigPath, 1024 * 1024, text) != core::ReadStatus::Ok) {
        return;
    }
    const auto doc = core::json::Document::parse(text);
    if (!doc) {
        return;
    }
    const auto terms = core::json::strings(core::json::object(doc->root(), "setting"), "yt_search_history");
    Settings imported = settings_;
    for (const std::string& term : terms) {  // after the searches made here, in their order
        if (imported.recentSearches.size() >= 8) {
            break;
        }
        if (std::find(imported.recentSearches.begin(), imported.recentSearches.end(), term) ==
            imported.recentSearches.end()) {
            imported.recentSearches.push_back(term);
        }
    }
    if (imported.recentSearches != settings_.recentSearches) {
        settings_.recentSearches = std::move(imported.recentSearches);
        saveSettings();
    }
}'''
new_importer = '''void AppSession::importPrevious(const std::string& oldConfigPath) {
    // XP-TUBE isolation: intentionally disabled. Never read legacy settings,
    // search history, or account tokens from another application.
    static_cast<void>(oldConfigPath);
}'''
if session_body.count(old_importer) != 1:
    raise SystemExit("src/app/shell/session.cpp: expected exactly one upstream legacy importer implementation")
session.write_text(session_body.replace(old_importer, new_importer), encoding="utf-8")


# Update visible product wording while retaining original product attribution in license pages.
def update_visible_brand(value, key=""):
    if isinstance(value, dict):
        return {name: update_visible_brand(item, name) for name, item in value.items()}
    if isinstance(value, list):
        return [update_visible_brand(item, key) for item in value]
    if isinstance(value, str) and not any(part in key.lower() for part in ("license", "notice", "copyright")):
        return value.replace("Playtube", "XP-TUBE")
    return value

for path in (ROOT / "resources/i18n").glob("*.json"):
    data = json.loads(path.read_text(encoding="utf-8"))
    path.write_text(json.dumps(update_visible_brand(data), ensure_ascii=False, indent=2) + "\n", encoding="utf-8")

checks = [
    ("src/core/version.cpp", ["PLTB00000", "PLTB00001", "PlaytubeDev", 'return "Playtube";']),
    ("cmake/ps4_pkg.cmake", ["PLTB00000", "PLTB00001"]),
    ("src/app/app.cpp", ["/data/Playtube/config.json", "session.importPrevious("]),
    ("src/platform/sdl_platform.cpp", ['core::dataPath("playtube.log")']),
    ("src/core/boot_trace.cpp", ["playtube boot:"]),
    ("src/app/update.hpp", ["LowganDev/PlaytubePS4/releases"]),
    ("src/app/update.cpp", ['"Playtube-"', 'std::string("Playtube/")']),
    ("scripts/build-ps4.sh", ["IV0001-PLTB"]),
    ("src/app/shell/chrome.cpp", ['add<ui::Label>("Playtube", style);']),
    ("src/app/shell/settings_screen.cpp", ['? "YouTube" : "Playtube"']),
    ("scripts/make-ps4-art.py", ["Playtube"]),
]
for rel, forbidden in checks:
    text = (ROOT / rel).read_text(encoding="utf-8")
    for value in forbidden:
        if value in text:
            raise SystemExit(f"{rel}: forbidden old identity remains: {value}")

print("XP-TUBE identity, visible branding, translations, updater, and account-isolation patch applied.")
