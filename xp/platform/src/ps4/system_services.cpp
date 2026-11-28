#include "xp/platform/ps4/system_services.hpp"

#include <cstddef>
#include <cstdint>

#include <orbis/Net.h>
#include <orbis/Sysmodule.h>
#include <orbis/SystemService.h>

namespace xp::platform::ps4 {
bool initialize_system_services() noexcept {
    sceSystemServiceHideSplashScreen();
    if (static_cast<std::int32_t>(sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_NET)) < 0) return false;
    return sceNetInit() >= 0;
}
int system_language_id() noexcept {
    std::int32_t id = 1;
    if (sceSystemServiceParamGetInt(ORBIS_SYSTEM_SERVICE_PARAM_ID_LANG, &id) < 0) return 1;
    return static_cast<int>(id);
}
int utc_offset_minutes(std::int64_t utc_seconds) noexcept {
    (void)utc_seconds;
    std::int32_t zone = 0, summer = 0;
    if (sceSystemServiceParamGetInt(ORBIS_SYSTEM_SERVICE_PARAM_ID_TIME_ZONE, &zone) < 0) zone = 0;
    if (sceSystemServiceParamGetInt(ORBIS_SYSTEM_SERVICE_PARAM_ID_SUMMERTIME, &summer) < 0) summer = 0;
    return static_cast<int>(zone + (summer != 0 ? 60 : 0));
}
} // namespace xp::platform::ps4
