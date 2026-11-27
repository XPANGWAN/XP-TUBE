#include "app/settings.hpp"

#include <algorithm>
#include <cstdio>

#include "core/file_io.hpp"
#include "core/log.hpp"
#include "core/strings.hpp"
#include "core/utf.hpp"
#include "core/json.hpp"

namespace playtube::app {

namespace {

constexpr std::size_t kMaxFileBytes = 64 * 1024;
constexpr std::size_t kMaxRecent = 8;
constexpr std::size_t kMaxTermBytes = 200;
constexpr std::size_t kMaxTopics = 12;
constexpr std::size_t kMaxTopicBytes = 60;

bool knownAccent(const std::string& id) {
    const auto& palette = ui::accentPalette();
    return std::any_of(palette.begin(), palette.end(), [&](const ui::AccentChoice& c) { return id == c.id; });
}

}  // namespace

Settings loadSettings(const std::string& path) {
    Settings settings;
    std::string text;
    switch (core::readSmallFile(path, kMaxFileBytes, text)) {
    case core::ReadStatus::Missing:
        return settings;
    case core::ReadStatus::Failed:
        PF_LOG_WARN("cannot read %s: default settings", path.c_str());
        return settings;
    case core::ReadStatus::Ok:
        break;
    }
    const auto doc = core::json::Document::parse(text);
    if (!doc || !yyjson_is_obj(doc->root())) {
        PF_LOG_WARN("%s is damaged: kept as .bad, default settings", path.c_str());
        std::rename(path.c_str(), (path + ".bad").c_str());
        return settings;
    }
    yyjson_val* root = doc->root();
    const std::string accent = core::json::string(root, "accent");
    if (knownAccent(accent)) {
        settings.accent = accent;
    }
    settings.language = core::json::string(root, "language");
    for (const std::string& term : core::json::strings(root, "recentSearches")) {
        if (settings.recentSearches.size() < kMaxRecent && !core::trimmed(term).empty()) {
            settings.recentSearches.emplace_back(core::utf8Prefix(term, kMaxTermBytes));
        }
    }
    const std::int64_t height = core::json::integer(root, "maxHeight", settings.maxHeight);
    if (height == 360 || height == 480 || height == 720 || height == 1080) {
        settings.maxHeight = static_cast<int>(height);
    }
    settings.sponsorBlock = core::json::boolean(root, "sponsorBlock", settings.sponsorBlock);
    if (core::json::array(root, "sponsorCategories") != nullptr) {
        settings.sponsorCategories.clear();
        for (const std::string& id : sponsorCategoryIds()) {
            const auto chosen = core::json::strings(root, "sponsorCategories");
            if (std::find(chosen.begin(), chosen.end(), id) != chosen.end()) {
                settings.sponsorCategories.push_back(id);
            }
        }
    }
    if (core::json::array(root, "homeTopics") != nullptr) {
        settings.customTopics = true;
        for (const std::string& topic : core::json::strings(root, "homeTopics")) {
            addTopic(settings, topic);
        }
    }
    settings.invidious = cleanInvidious(core::json::string(root, "invidious"));
    settings.autoplay = core::json::boolean(root, "autoplay", settings.autoplay);
    const std::int64_t volume = core::json::integer(root, "volume", 100);
    settings.volume = volume >= 0 && volume <= 100 ? static_cast<int>(volume) : 100;
    const std::string fit = core::json::string(root, "pictureFit");
    if (fit == "fit" || fit == "fill" || fit == "stretch") {
        settings.pictureFit = fit;
    }
    const std::int64_t size = core::json::integer(root, "pictureSize", 100);
    settings.pictureSize = size >= 80 && size <= 100 ? static_cast<int>(size) : 100;
    const std::string subtitleSize = core::json::string(root, "subtitleSize");
    if (subtitleSize == "small" || subtitleSize == "normal" || subtitleSize == "large" || subtitleSize == "larger") {
        settings.subtitleSize = subtitleSize;
    }
    settings.subtitleBackground = core::json::boolean(root, "subtitleBackground", false);
    settings.dislikes = core::json::boolean(root, "dislikes", true);
    settings.watchHistory = core::json::boolean(root, "watchHistory", true);
    settings.shorts = core::json::boolean(root, "shorts", true);
    settings.dearrow = core::json::boolean(root, "dearrow", false);
    if (core::json::string(root, "logo") == "youtube") {
        settings.logo = "youtube";
    }
    return settings;
}

bool saveSettings(const std::string& path, const Settings& settings) {
    core::json::Writer writer;
    writer.object();
    writer.key("version").value(1);
    writer.key("accent").value(settings.accent);
    writer.key("language").value(settings.language);
    writer.key("recentSearches").array();
    for (const std::string& term : settings.recentSearches) {
        writer.value(term);
    }
    writer.end();
    writer.key("maxHeight").value(settings.maxHeight);
    writer.key("sponsorBlock").value(settings.sponsorBlock);
    writer.key("sponsorCategories").array();
    for (const std::string& id : settings.sponsorCategories) {
        writer.value(id);
    }
    writer.end();
    if (settings.customTopics) {
        writer.key("homeTopics").array();
        for (const std::string& topic : settings.homeTopics) {
            writer.value(topic);
        }
        writer.end();
    }
    writer.key("invidious").value(settings.invidious);
    writer.key("autoplay").value(settings.autoplay);
    writer.key("volume").value(settings.volume);
    writer.key("pictureFit").value(settings.pictureFit);
    writer.key("pictureSize").value(settings.pictureSize);
    writer.key("subtitleSize").value(settings.subtitleSize);
    writer.key("subtitleBackground").value(settings.subtitleBackground);
    writer.key("dislikes").value(settings.dislikes);
    writer.key("watchHistory").value(settings.watchHistory);
    writer.key("shorts").value(settings.shorts);
    writer.key("dearrow").value(settings.dearrow);
    writer.key("logo").value(settings.logo);
    writer.end();
    return core::writeFileAtomic(path, writer.str());
}

void addRecentSearch(Settings& settings, std::string term) {
    term = std::string(core::utf8Prefix(core::trimmed(term), kMaxTermBytes));
    if (term.empty()) {
        return;
    }
    auto& list = settings.recentSearches;
    list.erase(std::remove(list.begin(), list.end(), term), list.end());
    list.insert(list.begin(), std::move(term));
    if (list.size() > kMaxRecent) {
        list.resize(kMaxRecent);
    }
}

const std::vector<std::string>& sponsorCategoryIds() {
    static const std::vector<std::string> ids = {"sponsor", "selfpromo", "interaction",
                                                 "intro",   "outro",     "music_offtopic"};
    return ids;
}

std::string cleanInvidious(std::string address) {
    address = std::string(core::trimmed(address));
    while (!address.empty() && address.back() == '/') {
        address.pop_back();
    }
    const std::string scheme = "https://";
    if (address.size() <= scheme.size() || address.size() > 200 || core::asciiLower(address.substr(0, 8)) != scheme ||
        address.find_first_of(" \t\r\n\"?#@\\") != std::string::npos || address[scheme.size()] == '/') {
        return {};
    }
    return scheme + address.substr(scheme.size());  // the requests join "/api/v1/…" to it
}

bool topicsFull(const Settings& settings) { return settings.homeTopics.size() >= kMaxTopics; }

bool addTopic(Settings& settings, std::string topic) {
    topic = std::string(core::utf8Prefix(core::trimmed(topic), kMaxTopicBytes));
    auto& topics = settings.homeTopics;
    if (topic.empty() || topics.size() >= kMaxTopics ||
        std::find(topics.begin(), topics.end(), topic) != topics.end()) {
        return false;
    }
    topics.push_back(std::move(topic));
    settings.customTopics = true;
    return true;
}

const ui::AccentChoice& accentById(const std::string& id) {
    for (const ui::AccentChoice& choice : ui::accentPalette()) {
        if (id == choice.id) {
            return choice;
        }
    }
    return ui::accentPalette().front();
}

const std::vector<std::pair<std::string, std::string>>& uiLanguages() {
    static const std::vector<std::pair<std::string, std::string>> languages = {
        {"en", "English"},
        {"fr", "Fran\xC3\xA7" "ais"},
        {"de", "Deutsch"},
        {"es", "Espa\xC3\xB1ol"},
        {"pt", "Portugu\xC3\xAAs"},
        {"cs", "\xC4\x8C" "e\xC5\xA1tina"},
        {"tr", "T\xC3\xBCrk\xC3\xA7" "e"},
        {"ru", "\xD0\xA0\xD1\x83\xD1\x81\xD1\x81\xD0\xBA\xD0\xB8\xD0\xB9"},
        {"uk", "\xD0\xA3\xD0\xBA\xD1\x80\xD0\xB0\xD1\x97\xD0\xBD\xD1\x81\xD1\x8C\xD0\xBA\xD0\xB0"},
        {"vi", "Ti\xE1\xBA\xBFng Vi\xE1\xBB\x87t"},
        {"ja", "\xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E"},
        {"ko", "\xED\x95\x9C\xEA\xB5\xAD\xEC\x96\xB4"},
        {"zh-Hans", "\xE7\xAE\x80\xE4\xBD\x93\xE4\xB8\xAD\xE6\x96\x87"},
        {"zh-Hant", "\xE7\xB9\x81\xE9\xAB\x94\xE4\xB8\xAD\xE6\x96\x87"},
    };
    return languages;
}

double subtitleScale(const std::string& size) {
    return size == "small" ? 0.75 : size == "large" ? 1.25 : size == "larger" ? 1.5 : 1.0;
}

}  // namespace playtube::app
