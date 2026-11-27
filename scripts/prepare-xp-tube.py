#!/usr/bin/env python3
"""Apply an auditable XP-TUBE identity, branding, and account-isolation patch."""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1] / "upstream"

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
    "src/app/update.hpp": [
        ('https://api.github.com/repos/LowganDev/PlaytubePS4/releases/latest',
         'https://api.github.com/repos/XPANGWAN/XP-TUBE/releases/latest'),
    ],
    "src/app/update.cpp": [
        ('"Playtube-" + safe + ".pkg"', '"XP-TUBE-" + safe + ".pkg"'),
        ('std::string("Playtube/")', 'std::string("XP-TUBE/")', 2),
        ('const std::string prefix = "Playtube-";', 'const std::string prefix = "XP-TUBE-";'),
    ],
    "scripts/build-ps4.sh": [
        ('"$ROOT"/build/ps4/pkg/IV0001-PLTB*.pkg', '"$ROOT"/build/ps4/pkg/IV0001-XPTB*.pkg'),
        ('IV0001-PLTB00000_00-PLTB000000100000.pkg', 'IV0001-XPTB00000_00-XPTB000000100000.pkg'),
        ('IV0001-PLTB00001_00-PLTB000010100000.pkg', 'IV0001-XPTB00000_00-XPTB000000100000.pkg'),
    ],
    "src/app/shell/chrome.cpp": [
        ('add<ui::Label>("Playtube", style);', 'gap = ui::theme().spacingS / 2;\n        add<AccentLabel>("XP", style);\n        add<ui::Label>("TUBE", style);'),
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
    ],
        ('const ui::Color kPlayedColor{0xFF, 0x00, 0x33, 0xFF};   // YouTube red: the part played and the head', '// The played portion and head follow XP-TUBE\'s selected accent colour.'),
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

    "src/app/settings.hpp": [
        ('std::string accent = "white";', 'std::string accent = "violet";'),
        ('std::string logo = "playtube";', 'std::string logo = "xp-tube";'),
        ('// the header\'s logo: "playtube", or "youtube" (fetched from YouTube)', '// the header\'s logo: "xp-tube", or "youtube" (fetched from YouTube)'),
    ],
    "src/app/shell/settings_screen.cpp": [
        ('logo == "youtube" ? "YouTube" : "Playtube"', 'logo == "youtube" ? "YouTube" : "XP-TUBE"'),
        ('settings.logo == "youtube" ? "playtube" : "youtube"', 'settings.logo == "youtube" ? "xp-tube" : "youtube"'),
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


# Update the app's shipped translations without changing attribution/license records.
for path in (ROOT / "resources/i18n").glob("*.json"):
    body = path.read_text(encoding="utf-8")
    path.write_text(body.replace("Playtube", "XP-TUBE"), encoding="utf-8")

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
