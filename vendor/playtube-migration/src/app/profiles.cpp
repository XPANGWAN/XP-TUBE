#include "app/profiles.hpp"

#include <dirent.h>
#include <sys/stat.h>

#include <cstdio>

#include "core/data_dir.hpp"
#include "core/log.hpp"

namespace playtube::app {

std::string profileFolder(const std::string& consoleUser) {
    if (consoleUser.empty() || consoleUser.size() > 20) {
        return {};
    }
    for (char c : consoleUser) {
        if (c < '0' || c > '9') {
            return {};
        }
    }
    return "users/" + consoleUser;
}

std::optional<std::string> profileFile(const std::string& consoleUser, const std::string& name) {
    const std::string folder = profileFolder(consoleUser);
    if (folder.empty()) {
        return core::dataPath(name);
    }
    const auto directory = core::dataPath(folder);
    const auto path = core::dataPath(folder + "/" + name);
    if (!directory || !path || !core::makeDirs(*directory)) {
        return core::dataPath(name);  // the shared file rather than nothing
    }
    struct stat info;
    const auto shared = core::dataPath(name);
    if (shared && ::stat(path->c_str(), &info) != 0 && ::stat(shared->c_str(), &info) == 0) {
        // made before profiles: it belongs to the first account that starts this version
        if (std::rename(shared->c_str(), path->c_str()) == 0) {
            PF_LOG_INFO("profiles: %s is now user %s's", name.c_str(), consoleUser.c_str());
        }
    }
    return path;
}

bool onlyProfile(const std::string& consoleUser) {
    const auto users = core::dataPath("users");
    if (!users) {
        return true;
    }
    DIR* listing = ::opendir(users->c_str());
    if (listing == nullptr) {
        return true;
    }
    bool only = true;
    while (const dirent* entry = ::readdir(listing)) {
        const std::string name = entry->d_name;
        if (name != "." && name != ".." && name != consoleUser) {
            only = false;
        }
    }
    ::closedir(listing);
    return only;
}

}  // namespace playtube::app
