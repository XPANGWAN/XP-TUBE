#include "xp/platform/language.hpp"

#include <cstddef>

namespace xp::platform {
namespace {
constexpr const char* kPs4Languages[] = {
    "ja-JP", "en-US", "fr-FR", "es-ES", "de-DE", "it-IT", "nl-NL",
    "pt-PT", "ru-RU", "ko-KR", "zh-Hant", "zh-Hans", "fi-FI",
    "sv-SE", "da-DK", "nb-NO", "pl-PL", "pt-BR", "en-GB", "tr-TR",
    "es-419", "ar", "fr-CA", "cs-CZ", "hu-HU", "el-GR", "ro-RO",
    "th-TH", "vi-VN", "id-ID", "uk-UA"
};
}
std::string language_from_ps4_id(int id) {
    if (id < 0 || static_cast<std::size_t>(id) >= sizeof(kPs4Languages) / sizeof(kPs4Languages[0])) {
        return "en-US";
    }
    return kPs4Languages[id];
}
} // namespace xp::platform
