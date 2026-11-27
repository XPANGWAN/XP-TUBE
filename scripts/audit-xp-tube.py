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
require("src/app/shell/session.cpp", r'void AppSession::importPrevious\\(const std::string& oldConfigPath\\) \\{[\\s\\S]*?static_cast<void>\\(oldConfigPath\\);[\\s\\S]*?\\}', "legacy importer implementation is an inert no-op")
forbid("src/app/shell/session.cpp", r'account_\\.importFromOld\\s*\\(', "legacy account-token importer")
forbid("src/app/shell/session.cpp", r'core::readSmallFile\\(oldConfigPath', "legacy settings-file read")

forbid("src/app/app.cpp", r'/data/Playtube', "legacy Playtube data path in app startup")
forbid("src/platform/sdl_platform.cpp", r'playtube\.log', "legacy Playtube log filename")
forbid("src/core/boot_trace.cpp", r'playtube boot:', "legacy Playtube boot-log label")
forbid("src/core/version.cpp", r'PLTB0000[01]|return "Playtube', "legacy Playtube package identity")

# The importer may remain in upstream for compatibility, but XP-TUBE must not call it.
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
print("XP-TUBE independence audit passed.")
