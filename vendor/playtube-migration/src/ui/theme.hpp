#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace playtube::ui {

struct Color {
    std::uint8_t r = 0;
    std::uint8_t g = 0;
    std::uint8_t b = 0;
    std::uint8_t a = 255;
};

constexpr Color withAlpha(Color color, std::uint8_t alpha) { return {color.r, color.g, color.b, alpha}; }

// What a theme's background is (baked into one opaque image, never drawn as a live gradient).
enum class BackgroundKind { Halo, Gradient, Plain, Grain };
// How a screen's backdrop picture is treated when decoded (Shade: the identity's shade from the top).
enum class BackdropKind { Shade, Blur, Tint, Diagonal };
// How a focused card stands out: the accent's glow under it, a ring around it, or a white frame.
enum class FocusStyle { Glow, Ring, Frame };
// The navigation bar's tabs: the current one underlined, in a pill, or only brighter.
enum class TabStyle { Underline, Pill, Plain };

// A theme's typeface: its family (the font faces' prefix) and the files of its three weights in resources/fonts
// (Regular, SemiBold, Bold). Without the UI language's script (Cyrillic, Vietnamese marks) Noto Sans is used.
struct ThemeFont {
    const char* family = "Sora";
    const char* files[3] = {"Sora-Regular.ttf", "Sora-SemiBold.ttf", "Sora-Bold.ttf"};
    bool cyrillic = false;
    bool vietnamese = false;
};

// Design tokens of the Playtube identity (Playfin's look in YouTube red; one look, no theme setting): widgets read these, never
// hard-coded values.
struct Theme {
    std::string id = "projecteur";  // the look (Playtube has one); derived pictures are kept per look
    ThemeFont font;
    BackgroundKind backgroundKind = BackgroundKind::Halo;
    Color gradientTop{0x0E, 0x0E, 0x11, 0xFF};  // Gradient: from the top...
    Color gradientBottom{0x0E, 0x0E, 0x11, 0xFF};  // ...to the bottom
    BackdropKind backdropKind = BackdropKind::Shade;
    float backdropBrightness = 1;
    float backdropSaturation = 1;
    float backdropContrast = 1;
    bool backdropFollowsFocus = false;  // the home and library screens show the focused item's backdrop
    FocusStyle focusStyle = FocusStyle::Glow;
    bool focusSheen = false;  // a light band across the focused card
    float sheenStrength = 0.07f;  // its white at the middle of the band
    bool focusInAccent = true;  // the ring in the accent; else in focusColor
    Color focusColor{0xFF, 0xFF, 0xFF, 0xFF};
    float glowStrength = 0.3f;  // Glow: the accent's alpha under the card
    float cardShadow = 0;  // a soft dark shadow under every card (its strength), 0: none
    float cardBorderWidth = 0;
    Color cardBorder{0, 0, 0, 0};
    TabStyle tabStyle = TabStyle::Underline;
    bool tabPillInAccent = false;  // Pill: the current tab in the accent (else in the text colour)
    Color navBackground{0, 0, 0, 0};  // a pill behind the tabs when not transparent

    Color background{0x07, 0x07, 0x08, 0xFF};  // near black, plain (no halo)
    Color surface{0x15, 0x15, 0x18, 0xFF};
    Color surfaceRaised{0x1D, 0x1D, 0x21, 0xFF};
    Color surfaceFocused{0x29, 0x29, 0x2E, 0xFF};
    Color text{0xF2, 0xF2, 0xF5, 0xFF};
    Color textSecondary{0x9A, 0x9C, 0xA8, 0xFF};
    Color accent{0xFF, 0xFF, 0xFF, 0xFF};  // the accent setting (white by default)
    Color onAccent{0x0A, 0x0A, 0x0C, 0xFF};
    Color focusRing{0xFF, 0xFF, 0xFF, 0xFF};  // follows the accent
    Color control{0x15, 0x15, 0x18, 0xFF};    // buttons and fields
    Color controlFocused{0xFF, 0xFF, 0xFF, 0xFF};
    Color scrim{0x00, 0x00, 0x00, 0xB0};
    Color error{0xE5, 0x48, 0x4D, 0xFF};
    float focusRingWidth = 4;
    float radius = 12;
    float spacingS = 8;
    float spacingM = 16;
    float spacingL = 24;
    float spacingXl = 32;
    float spacingXxl = 48;
    float marginSide = 96;
    float rowGap = 40;
    float cardGap = 28;
    float fontSmall = 20;    // meta: card subtitles, hints
    float fontBody = 24;     // text, buttons
    float fontTitle = 28;    // screen subtitles, player title
    float fontHeading = 30;  // row titles
    float fontLogo = 40;
    float fontDisplay = 84;  // details title
    float focusMs = 250;
    float pushMs = 350;      // a pushed screen fades in and rises
    float popMs = 250;       // the screen revealed by pop, or a replaced one, fades in
    float revealMs = 350;    // rows arriving on a screen
    float revealGapMs = 35;  // between two arriving rows
    float revealRise = 30;
    float scrollMs = 400;
    float fadeMs = 150;
};

// The accent setting's choices.
struct AccentChoice {
    const char* id;
    Color accent;
    Color onAccent;
};
const std::vector<AccentChoice>& accentPalette();

// Sets the accent and what follows it (focus ring, focused controls), and bumps themeGeneration().
void applyAccent(const AccentChoice& choice);
// Changes whenever the theme does: views and images derived from it (the halo) rebuild when it differs.
unsigned themeGeneration();

// Defined in theme.cpp, not inline: the static of an inline function gets a section of its own, and the PS4
// packaging tool (create-fself) dropped that section's contents (the console saw an all-zero theme).
Theme& mutableTheme();
inline const Theme& theme() { return mutableTheme(); }

}  // namespace playtube::ui
