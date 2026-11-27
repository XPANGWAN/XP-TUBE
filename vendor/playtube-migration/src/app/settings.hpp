#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "ui/theme.hpp"

namespace playtube::app {

// The user's choices, kept in settings.json in the data folder.
struct Settings {
    std::string accent = "white";  // id in ui::accentPalette()
    std::string language;                // UI language tag, "" = the console's
    std::vector<std::string> recentSearches;  // newest first, at most 8
    int maxHeight = 1080;                     // 360, 480, 720 or 1080
    bool sponsorBlock = true;
    std::vector<std::string> sponsorCategories = {"sponsor", "selfpromo", "interaction"};
    bool customTopics = false;            // false: the home shows the default topics of the UI language
    std::vector<std::string> homeTopics;  // searches shown as rows on the home screen, at most 12
    std::string invidious;                // https address of an Invidious instance, "" = not used
    bool autoplay = true;                 // "Lecture automatique": the next video after a countdown
    int volume = 100;                     // the player's, 0 to 100
    std::string pictureFit = "fit";       // "fit", "fill" or "stretch"
    int pictureSize = 100;                // percent of the screen the picture may take, 80 to 100
    std::string subtitleSize = "normal";  // "small", "normal", "large" or "larger"
    bool subtitleBackground = false;      // a dark box behind the subtitles
    bool dislikes = true;                 // Return YouTube Dislike's counts in the player (it gets the video's id)
    bool dearrow = false;                 // DeArrow's titles and thumbnails on cards (it gets each video's id)
    bool shorts = true;                   // Shorts on the home, channels, feeds and suggestions (search keeps them)
    bool watchHistory = true;             // signed in: videos watched go into the account's YouTube history
    std::string logo = "playtube";        // the header's logo: "playtube", or "youtube" (fetched from YouTube)
};

// The player's scale of a subtitle size (1 for an unknown one).
double subtitleScale(const std::string& size);

// The SponsorBlock categories the settings offer, in their order.
const std::vector<std::string>& sponsorCategoryIds();
// "https://host[/path]" without trailing slash; "" for anything else.
std::string cleanInvidious(std::string address);
// Adds a home topic (trimmed, not twice, at most 12); true when added.
bool topicsFull(const Settings& settings);
bool addTopic(Settings& settings, std::string topic);

// Missing file: defaults. Damaged file: renamed .bad, defaults. Unknown or invalid values: their defaults.
Settings loadSettings(const std::string& path);
bool saveSettings(const std::string& path, const Settings& settings);

// Trimmed; empty terms are ignored; a term already listed moves to the front; at most 8 are kept.
void addRecentSearch(Settings& settings, std::string term);

// The palette entry with this id, else the default (YouTube red).
const ui::AccentChoice& accentById(const std::string& id);

// The UI languages (tag of resources/i18n, name in that language), English first.
const std::vector<std::pair<std::string, std::string>>& uiLanguages();

}  // namespace playtube::app
