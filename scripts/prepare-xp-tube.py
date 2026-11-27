#!/usr/bin/env python3
"""Apply the small, auditable XP-TUBE identity patch to a pinned Playtube checkout.

The upstream checkout is cloned at an exact commit by CI. Every replacement is
counted and fails closed if upstream source no longer matches the reviewed baseline.
"""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
FILES = {
    "src/core/version.cpp": [
        ('return "Playtube Dev";', 'return "XP-TUBE";'),
        ('return "PlaytubeDev";', 'return "XP-TUBE";'),
        ('return "PLTB00001";', 'return "XPTB00000";'),
        ('return "Playtube";', 'return "XP-TUBE";'),
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
        ('std::string("Playtube/")', 'std::string("XP-TUBE/")'),
        ('const std::string prefix = "Playtube-";', 'const std::string prefix = "XP-TUBE-";'),
    ],
    "scripts/build-ps4.sh": [
        ('"$ROOT"/build/ps4/pkg/IV0001-PLTB*.pkg', '"$ROOT"/build/ps4/pkg/IV0001-XPTB*.pkg'),
        ('IV0001-PLTB00000_00-PLTB000000100000.pkg', 'IV0001-XPTB00000_00-XPTB000000100000.pkg'),
        ('IV0001-PLTB00001_00-PLTB000010100000.pkg', 'IV0001-XPTB00000_00-XPTB000000100000.pkg'),
    ],
}
for rel, replacements in FILES.items():
    path = ROOT / rel
    text = path.read_text(encoding="utf-8")
    for old, new in replacements:
        count = text.count(old)
        if count != 1:
            raise SystemExit(f"{rel}: expected exactly one occurrence of {old!r}, found {count}")
        text = text.replace(old, new)
    path.write_text(text, encoding="utf-8")

app = ROOT / "src/app/app.cpp"
text = app.read_text(encoding="utf-8")
old = '''#ifdef PLAYTUBE_PS4
        if (onlyProfile(consoleUser)) {
            session.importPrevious("/data/Playtube/config.json");  // the previous Playtube's settings, only read
        }
#endif
'''
if text.count(old) != 1:
    raise SystemExit("src/app/app.cpp: expected the single legacy Playtube migration block")
app.write_text(text.replace(old, "        // XP-TUBE: never read, import, or copy Playtube's settings or account tokens.\n"), encoding="utf-8")

# Assert that the generated runtime source and package recipe have no old
# release/development identifiers or legacy data migration path.
checks = [
    ("src/core/version.cpp", ["PLTB00000", "PLTB00001", "PlaytubeDev", "return \"Playtube\";"]),
    ("cmake/ps4_pkg.cmake", ["PLTB00000", "PLTB00001"]),
    ("src/app/app.cpp", ["/data/Playtube/config.json", "session.importPrevious("]),
    ("src/app/update.hpp", ["LowganDev/PlaytubePS4/releases"]),
    ("src/app/update.cpp", ['"Playtube-"', 'std::string("Playtube/")']),
    ("scripts/build-ps4.sh", ["IV0001-PLTB"]),
]
for rel, forbidden in checks:
    body = (ROOT / rel).read_text(encoding="utf-8")
    for value in forbidden:
        if value in body:
            raise SystemExit(f"{rel}: forbidden legacy identity remains: {value}")

print("XP-TUBE identity patch applied; legacy app identity and first-run account migration checks passed.")
