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
        ('add<ui::Label>("Playtube", style);', 'add<ui::Label>("XP-TUBE", style);'),
    ],
    "src/app/shell/settings_screen.cpp": [
        ('logo == "youtube" ? "YouTube" : "Playtube"', 'logo == "youtube" ? "YouTube" : "XP-TUBE"'),
    ],
    "src/app/gallery.cpp": [
        ('"Playtube toolkit"', '"XP-TUBE"'),
    ],
    "src/app/app.cpp": [
        ('PF_LOG_INFO("Playtube %s, language %s, data %s"', 'PF_LOG_INFO("XP-TUBE %s, language %s, data %s"'),
    ],
    "scripts/make-ps4-art.py": [
        ("Playtube", "XP-TUBE"),
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
app.write_text(body.replace(old, "        // XP-TUBE: never read, import, or copy Playtube's settings or account tokens.\\n"), encoding="utf-8")

# Update the app's shipped translations without changing attribution/license records.
for path in (ROOT / "resources/i18n").glob("*.json"):
    body = path.read_text(encoding="utf-8")
    path.write_text(body.replace("Playtube", "XP-TUBE"), encoding="utf-8")

checks = [
    ("src/core/version.cpp", ["PLTB00000", "PLTB00001", "PlaytubeDev", 'return "Playtube";']),
    ("cmake/ps4_pkg.cmake", ["PLTB00000", "PLTB00001"]),
    ("src/app/app.cpp", ["/data/Playtube/config.json", "session.importPrevious("]),
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
