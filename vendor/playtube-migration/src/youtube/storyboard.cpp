#include "youtube/storyboard.hpp"

#include <cstdlib>
#include <string>
#include <vector>

namespace playtube::youtube {

namespace {

constexpr int kMaxPictureWidth = 320;
constexpr long long kMaxSheetSide = 4096;
constexpr long long kMaxSheetPixels = 8LL * 1024 * 1024;
constexpr std::size_t kMaxSpec = 8 * 1024;

std::vector<std::string> split(std::string_view text, char separator) {
    std::vector<std::string> parts;
    std::size_t start = 0;
    while (parts.size() < 64) {
        const std::size_t end = text.find(separator, start);
        parts.emplace_back(text.substr(start, end == std::string_view::npos ? std::string_view::npos : end - start));
        if (end == std::string_view::npos) {
            break;
        }
        start = end + 1;
    }
    return parts;
}

// a whole number of up to 9 digits, else -1
long long number(const std::string& text) {
    if (text.empty() || text.size() > 9) {
        return -1;
    }
    for (char c : text) {
        if (c < '0' || c > '9') {
            return -1;
        }
    }
    return std::atoll(text.c_str());
}

std::string replaced(std::string text, const std::string& from, const std::string& to) {
    for (std::size_t at = text.find(from); at != std::string::npos; at = text.find(from, at + to.size())) {
        text.replace(at, from.size(), to);
    }
    return text;
}

}  // namespace

std::string Storyboard::sheetUrl(int sheet) const {
    if (base.rfind("https://", 0) != 0 || sheet < 0) {
        return {};
    }
    std::string url = replaced(base, "$L", std::to_string(level));
    url = replaced(url, "$N", replaced(name, "$M", std::to_string(sheet)));
    if (!sigh.empty()) {
        url += (url.find('?') == std::string::npos ? "?sigh=" : "&sigh=") + sigh;
    }
    return url;
}

std::optional<Storyboard> parseStoryboard(std::string_view spec, int durationSeconds) {
    if (spec.empty() || spec.size() > kMaxSpec) {
        return std::nullopt;
    }
    const std::vector<std::string> parts = split(spec, '|');
    if (parts.size() < 2 || parts[0].rfind("https://", 0) != 0) {
        return std::nullopt;
    }
    std::optional<Storyboard> best;
    for (std::size_t i = 1; i < parts.size(); ++i) {
        const std::vector<std::string> f = split(parts[i], '#');
        if (f.size() < 7) {
            continue;
        }
        Storyboard s;
        const long long width = number(f[0]);
        const long long height = number(f[1]);
        const long long count = number(f[2]);
        const long long columns = number(f[3]);
        const long long rows = number(f[4]);
        long long interval = number(f[5]);
        if (width <= 0 || height <= 0 || count <= 0 || columns <= 0 || rows <= 0 || interval < 0 ||
            width > kMaxPictureWidth || width * columns > kMaxSheetSide || height * rows > kMaxSheetSide ||
            width * columns * height * rows > kMaxSheetPixels) {
            continue;
        }
        if (interval == 0) {  // spread over the video
            if (durationSeconds <= 0) {
                continue;
            }
            interval = static_cast<long long>(durationSeconds) * 1000 / count;
        }
        s.width = static_cast<int>(width);
        s.height = static_cast<int>(height);
        s.count = static_cast<int>(count);
        s.columns = static_cast<int>(columns);
        s.rows = static_cast<int>(rows);
        s.intervalMs = interval > 0 ? interval : 1000;
        s.level = static_cast<int>(i - 1);
        s.base = parts[0];
        s.name = f[6];
        s.sigh = f.size() > 7 ? f[7] : std::string();
        if (!best || s.width > best->width) {
            best = s;
        }
    }
    return best;
}

}  // namespace playtube::youtube
