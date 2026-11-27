#pragma once

#include <optional>
#include <string>

namespace playtube::app {

// Each PS4 account using Playtube has its own YouTube sign-in and its own watching progress, in
// "users/<user id>/" of the data folder; the settings stay shared. Without a console user (Linux, tests) the files
// stay at the top of the data folder, as before profiles.

// "users/<id>" for a user id of digits only, "" for none (or an id that is not one).
std::string profileFolder(const std::string& consoleUser);

// The path of a profile's file (`name`) in the data folder, its folder created. The first PS4 account to start a
// version with profiles takes the sign-in made before them: "account.json" (and "progress.json") moved into its
// folder when it has none.
std::optional<std::string> profileFile(const std::string& consoleUser, const std::string& name);

// True when no other PS4 account has a profile yet: only then is the previous Playtube's sign-in imported (it
// would otherwise sign every new account in with the first one's YouTube account).
bool onlyProfile(const std::string& consoleUser);

}  // namespace playtube::app
