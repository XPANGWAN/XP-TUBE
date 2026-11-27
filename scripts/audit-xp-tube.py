#!/usr/bin/env python3
"""Fail the XP-TUBE build if its install, storage, updater, or boot-log identity regresses."""
from pathlib import Path
import json
import re
import sys

ROOT = Path(__file__).resolve().parents[1] / "upstream"

def read(path: str) -> str:
    target = ROOT / path
    if not target.is_file():
        raise SystemExit(f"[FAIL] required upstream file missing: {path}")
    return target.read_text(encoding="utf-8")

def require(path: str, pattern: str, description: str) -> None:
    if re.search(pattern, read(path), re.MULTILINE) is None:
        raise SystemExit(f"[FAIL] {path}: missing {description}")
    print(f"[PASS] {description}")

def forbid(path: str, pattern: str, description: str) -> None:
    if re.search(pattern, read(path), re.MULTILINE):
        raise SystemExit(f"[FAIL] {path}: found {description}")
    print(f"[PASS] absent: {description}")

require("src/core/version.cpp", r'const char\* appTitle\(\) \{ return "XP-TUBE"; \}', "release title is XP-TUBE")
require("src/core/version.cpp", r'const char\* appFolder\(\) \{ return "XP-TUBE"; \}', "private data folder is XP-TUBE")
require("src/core/version.cpp", r'const char\* titleId\(\) \{ return "XPTB00000"; \}', "package title ID is XPTB00000")
require("cmake/ps4_pkg.cmake", r'set\(title_id "XPTB00000"\)', "package metadata uses XPTB00000")
require("cmake/ps4_pkg.cmake", r'set\(title "XP-TUBE"\)', "package metadata title is XP-TUBE")
require("src/app/update.hpp", r'api\.github\.com/repos/XPANGWAN/XP-TUBE/releases/latest', "updater checks XP-TUBE releases")
require("src/app/update.cpp", r'"XP-TUBE-"', "downloaded update packages use XP-TUBE prefix")
require("src/app/update.cpp", r'std::string\("XP-TUBE/"\)', "updater User-Agent uses XP-TUBE identity")
require("src/app/app.cpp", r'core::dataPath\("settings\.json"\)', "settings resolve under the app data root")
require("src/app/app.cpp", r'profileFile\(consoleUser, "account\.json"\)', "account data remains profile-scoped")
require("src/platform/sdl_platform.cpp", r'core::dataPath\("xp-tube\.log"\)', "runtime log uses XP-TUBE filename")
require("src/core/boot_trace.cpp", r'XP-TUBE boot:', "early boot messages use XP-TUBE identity")
require("src/core/boot_trace.cpp", r'"/data/%s".*appFolder\(\)', "early boot trace uses the app-specific data folder")
require("src/core/boot_trace.cpp", r'"%s/boot\.txt"', "early boot trace writes boot.txt")
forbid("src/app/app.cpp", r'session\.importPrevious\s*\(', "legacy Playtube account/settings import call")
session = read("src/app/shell/session.cpp")
marker = "void AppSession::importPrevious("
if marker not in session:
    sys.exit("[FAIL] legacy importer interface is missing")
importer = session.split(marker, 1)[1].split("\nvoid AppSession::completeAccount()", 1)[0]
if "static_cast<void>(oldConfigPath);" not in importer:
    sys.exit("[FAIL] legacy importer is not an inert no-op")
for forbidden in ("account_.importFromOld(", "core::readSmallFile(", "core::json::"):
    if forbidden in importer:
        sys.exit(f"[FAIL] legacy importer can still access old data: {forbidden}")
print("[PASS] legacy importer implementation is an inert no-op")


forbid("src/app/app.cpp", r'/data/Playtube', "legacy Playtube data path in app startup")
forbid("src/platform/sdl_platform.cpp", r'playtube\.log', "legacy Playtube log filename")
forbid("src/core/boot_trace.cpp", r'playtube boot:', "legacy Playtube boot-log label")
forbid("src/core/version.cpp", r'PLTB0000[01]|return "Playtube', "legacy Playtube package identity")

# Confirm the importer is not called from application startup.
app = read("src/app/app.cpp")
if "session.importPrevious(" in app:
    sys.exit("[FAIL] XP-TUBE startup invokes the legacy importer")
print("[PASS] XP-TUBE startup does not invoke the legacy importer")

# Translation files are user-visible; retain upstream legal notices elsewhere.
translation_files = sorted((ROOT / "resources/i18n").glob("*.json"))
if not translation_files:
    sys.exit("[FAIL] no translation JSON files found")
for translation in translation_files:
    content = translation.read_text(encoding="utf-8")
    try:
        json.loads(content)
    except (UnicodeDecodeError, json.JSONDecodeError) as error:
        sys.exit(f"[FAIL] invalid translation file {translation.relative_to(ROOT)}: {error}")
    if "Playtube" in content:
        sys.exit(f"[FAIL] legacy Playtube branding remains in {translation.relative_to(ROOT)}")
print(f"[PASS] {len(translation_files)} translation files are valid and use XP-TUBE branding")
require("src/app/shell/chrome.cpp", r'add<AccentLabel>.*"XP", style', "XP-TUBE wordmark has a distinct accent XP mark")
require("src/app/shell/chrome.cpp", r'add<ui::Label>.*"TUBE", style', "XP-TUBE wordmark has a separate TUBE label")
require("src/app/shell/chrome.cpp", r'gap = ui::theme\(\)\.spacingS / 2;', "XP-TUBE wordmark has balanced letter-group spacing")
require("src/app/shell/player_screen.cpp", r'ui::nvgColor\(ui::theme\(\)\.accent\)', "player progress follows the selected XP-TUBE accent")
forbid("src/app/shell/player_screen.cpp", r'kPlayedColor|0xFF, 0x00, 0x33', "hard-coded YouTube-red player progress colour")

# Protect the tested Playtube-derived experience while XP-TUBE is rebranded.
require("src/app/shell/nav_bar.cpp", r"Tab::Home[\s\S]*Tab::Search[\s\S]*Tab::Subscriptions[\s\S]*Tab::Library", "home, search, subscriptions, and library navigation remain available")
require("src/app/shell/search_screen.cpp", r"ui::TextField", "controller-friendly search text field remains available")
require("src/app/shell/search_screen.cpp", r"filtersButton_", "search filters remain available")
require("src/app/shell/search_screen.cpp", r"youtube\(\)\.suggestions", "search suggestions remain connected")
require("src/app/shell/search_screen.cpp", r"youtube\(\)\.searchMore", "search result pagination remains connected")
require("src/app/shell/search_screen.cpp", r"youtube::searchParams\(filters_\)", "search filter selections are passed to YouTube queries")
require("src/app/shell/search_screen.cpp", r"generation == generation_", "stale search responses are ignored after a newer search")
require("src/app/shell/search_screen.cpp", r"youtube\(\)\.suggestions", "search suggestions remain asynchronous and connected")
require("src/app/shell/search_screen.cpp", r"youtube::videoIdFrom", "pasted YouTube video links remain supported")
require("src/app/shell/channel_screen.cpp", r"avatar_->setUrl\(channel\.avatar\)", "channel avatar and identity remain visible")
require("src/app/shell/channel_screen.cpp", r"youtube\(\)\.channelTab", "channel content tabs remain connected")
require("src/app/shell/channel_screen.cpp", r"youtube\(\)\.setSubscribed", "channel subscribe and unsubscribe actions remain connected")
require("src/app/shell/channel_screen.cpp", r"tabGeneration_", "channel tab requests are protected against stale responses")
require("src/app/shell/channel_screen.cpp", r"youtube\(\)\.channelMore", "channel video pagination remains connected")
require("src/app/shell/row_screens.cpp", r"homeTopics\(session_\)", "home topic rows remain available")
require("src/app/shell/row_screens.cpp", r'"FEhistory"', "watch history remains available in the library")
require("src/app/shell/row_screens.cpp", r'"VLWL"', "watch later remains available in the library")
require("src/app/shell/row_screens.cpp", r'"FEplaylist_aggregation"', "playlists remain available in the library")
require("src/app/shell/video_pages.cpp", r"list_\.onNearEnd", "video lists retain automatic pagination")
require("src/app/shell/video_pages.cpp", r"generation != generation_", "stale pagination responses cannot overwrite a replaced video list")
require("src/app/shell/player_screen.cpp", r"bind\(platform::Action::Confirm", "player retains controller confirm and pause handling")
require("src/app/shell/player_screen.cpp", r"bind\(platform::Action::Left", "player retains controller left and right seeking")
require("src/app/shell/player_screen.cpp", r"bind\(platform::Action::L1", "player retains previous-track or seek controller shortcut")
require("src/app/shell/player_screen.cpp", r"bind\(platform::Action::R1", "player retains next-track or seek controller shortcut")
require("src/app/shell/player_screen.cpp", r"bind\(platform::Action::Options", "player retains the options overlay shortcut")
require("src/app/shell/player_screen.cpp", r"bind\(platform::Action::Menu", "player retains the menu overlay shortcut")
require("src/app/shell/player_screen.cpp", r"ui::nvgColor\(ui::theme\(\)\.accent\)", "player progress uses the XP-TUBE theme accent")
require("src/app/shell/cards.cpp", r"ui::theme\(\)\.accent", "video-card focus styling follows the selected theme accent")


require("src/app/settings.hpp", r'std::string logo = "xp-tube"', "XP-TUBE is the default saved header-logo identity")
require("src/app/shell/settings_screen.cpp", r'settings\.logo == "youtube" \? "xp-tube" : "youtube"', "logo selector stores XP-TUBE or YouTube explicitly")
require("src/app/shell/settings_screen.cpp", r'logo == "youtube" \? "YouTube" : "XP-TUBE"', "settings screen displays the correct XP-TUBE and YouTube logo names")
require("src/app/shell/chrome.cpp", r'add<ui::Label>.*"TUBE", style', "header displays the XP-TUBE wordmark")
forbid("src/app/shell/chrome.cpp", r'add<ui::Label>\("Playtube"', "legacy Playtube header wordmark")
require("src/ui/theme.hpp", r'std::string id = "xp-tube"', "derived visual assets use the XP-TUBE theme cache identity")
forbid("src/app/settings.hpp", r'std::string logo = "playtube"', "legacy Playtube logo preference as the default")
forbid("src/app/settings.hpp", r'logo: "playtube"', "stale Playtube logo preference documentation")
forbid("src/app/shell/settings_screen.cpp", r'newer Playtube is published', "stale Playtube updater banner documentation")
forbid("src/app/shell/settings_screen.cpp", r'\? "playtube" : "youtube"', "legacy Playtube logo preference in the selector")
forbid("src/ui/theme.hpp", r'projecteur|Playtube identity', "legacy theme identity")

print("XP-TUBE independence audit passed.")
