#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "app/shell/nav_bar.hpp"
#include "app/shell/screen.hpp"
#include "app/shell/session.hpp"
#include "ui/application.hpp"
#include "youtube/models.hpp"

namespace playtube::app {

// The screen of a tab of the navigation bar (it replaces the whole stack).
std::unique_ptr<ui::Screen> makeTab(AppSession& session, Tab tab);
// Accueil: the signed-in feeds, then a row per home topic.
std::unique_ptr<ui::Screen> makeHome(AppSession& session);
// Recherche: the field, recent searches, results with more pages; a pasted link plays the video.
std::unique_ptr<ui::Screen> makeSearch(AppSession& session);

// Abonnements: the subscriptions feed (signed in).
std::unique_ptr<ui::Screen> makeSubscriptions(AppSession& session);
// Bibliothèque: history and "watch later" (signed in).
std::unique_ptr<ui::Screen> makeLibrary(AppSession& session);
// "Connectez-vous pour …" and a "Se connecter" button that comes back to `tab` once signed in (the focus stays on
// the tab).
void addSignInInvitation(ShellScreen& screen, ui::Box& column, const std::string& text, Tab tab);

// The default home topics in the UI language.
std::vector<std::string> defaultTopics();
// The topics shown on the home screen (the user's, else the defaults).
std::vector<std::string> homeTopics(const AppSession& session);

// A channel's page (pushed).
void openChannel(AppSession& session, const std::string& channelId);
// The channel of a video (□ on a card); asks YouTube for it when the card does not carry it.
void openChannelOf(ShellScreen& screen, const youtube::Video& video);

// Sign-in by code (pushed); `done` runs once signed in, after the screen closed.
void openSignIn(AppSession& session, std::function<void()> done);

// Plays a video: the player screen is pushed.
void openVideo(AppSession& session, const std::string& videoId);
// The videos of a playlist, in order: plays queue[index], then the ones after it ("À suivre").
using Queue = std::shared_ptr<const std::vector<youtube::Video>>;
void openQueue(AppSession& session, Queue queue, std::size_t index);
std::unique_ptr<ui::Screen> makePlayer(AppSession& session, const std::string& videoId, Queue queue = nullptr,
                                       std::size_t index = 0, bool shorts = false);
// Adds a video to the watch queue (not twice) and says so.
void addToQueue(AppSession& session, const youtube::Video& video);
// Shorts: plays queue[index] upright; ↑ / ↓ the previous / next Short of the queue, △ the menu; a Short loops.
void openShorts(AppSession& session, Queue queue, std::size_t index);

// A playlist's page (pushed): its title and size, "Tout lire", its videos with more pages.
void openPlaylist(AppSession& session, const youtube::Video& playlist);
// A channel's playlists (pushed).
void openChannelPlaylists(AppSession& session, const std::string& channelId, const std::string& channelName);

// Accent colour, language, about.
std::unique_ptr<ui::Screen> makeSettings(AppSession& session);
// The SponsorBlock categories to skip (pushed from the settings).
std::unique_ptr<ui::Screen> makeSponsorCategories(AppSession& session);
// The home topics: add, remove, reorder (pushed from the settings).
std::unique_ptr<ui::Screen> makeTopics(AppSession& session);
// The third-party licences (session.licensesDir), scrolling.
std::unique_ptr<ui::Screen> makeLicenses(AppSession& session);

}  // namespace playtube::app
