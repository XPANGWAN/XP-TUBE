#include "app/shell/screens.hpp"

namespace playtube::app {

std::unique_ptr<ui::Screen> makeTab(AppSession& session, Tab tab) {
    switch (tab) {
    case Tab::Home:
        return makeHome(session);
    case Tab::Search:
        return makeSearch(session);
    case Tab::Subscriptions:
        return makeSubscriptions(session);
    case Tab::Library:
        return makeLibrary(session);
    case Tab::Settings:
        return makeSettings(session);
    }
    return makeHome(session);
}

}  // namespace playtube::app
