#include "core/version.hpp"

namespace playtube::core {

const char* version() { return PLAYTUBE_VERSION; }

#ifdef PLAYTUBE_DEV
const char* appTitle() { return "Playtube Dev"; }
const char* appFolder() { return "PlaytubeDev"; }
const char* titleId() { return "PLTB00001"; }
#else
const char* appTitle() { return "Playtube"; }
const char* appFolder() { return "Playtube"; }
const char* titleId() { return "PLTB00000"; }
#endif

}  // namespace playtube::core
