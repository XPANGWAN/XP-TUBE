#include "core/log.hpp"
#include "core/version.hpp"
#include "platform/backend.hpp"
#include "xp/platform/language.hpp"
#include "xp/platform/ps4/system_services.hpp"
#ifdef PLAYTUBE_GPU_PROBE
#include "platform/ps4/gpu_probe_ps4.hpp"
#endif

namespace playtube::platform::backend {

// After SDL_Init, which loads the piglet (OpenGL ES) and system modules the system service needs.
void postInit() {
    // XP owns the PS4 system-service bootstrap; SDL still owns display and input startup.
    if (!xp::platform::ps4::initialize_system_services()) {
        PF_LOG_ERROR("network: cannot load libSceNet, host names will not resolve");
    }
#ifdef PLAYTUBE_GPU_PROBE
    ps4::probeGpu();
#endif
}

std::string defaultDataDir() { return std::string("/data/") + core::appFolder(); }

std::string systemLanguage() {
    return xp::platform::language_from_ps4_id(xp::platform::ps4::system_language_id());
}

// The console's time zone and its summer time setting (now: a date across a change is an hour off).
int utcOffsetMinutes(std::int64_t utcSeconds) {
    return xp::platform::ps4::utc_offset_minutes(utcSeconds);
}

std::string resourceDir() { return "/app0/resources"; }

}  // namespace playtube::platform::backend
