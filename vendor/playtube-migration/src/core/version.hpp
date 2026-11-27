#pragma once

namespace playtube::core {

// Version of this build ("major.minor.patch"), from the CMake project version.
const char* version();

// The app's identity: the development app (PLAYTUBE_DEV) is installed beside the release app and never shares its
// title id or data folder.
const char* appTitle();   // "Playtube Dev" / "Playtube"
const char* appFolder();  // data folder name: "PlaytubeDev" / "Playtube"
const char* titleId();    // "PLTB00001" / "PLTB00000"

}  // namespace playtube::core
