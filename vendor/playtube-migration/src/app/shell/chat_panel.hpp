#pragma once

#include <deque>
#include <functional>
#include <memory>
#include <string>
#include <utility>

#include "core/result.hpp"
#include "core/task.hpp"
#include "ui/box.hpp"
#include "ui/label.hpp"
#include "youtube/models.hpp"

namespace playtube::app {

// A live video's chat in a panel on the right, over the playing video (shown as a modal): the newest messages at
// the bottom, each with its author above it (a Super Chat's amount beside the author, in the accent). While it is
// open, the next page is asked for after YouTube's wait, kept between 1 and 3 s (after 10 s when a page failed);
// messages the moderators removed go away; the last kMaxShown are kept. ◯ closes it. Pages come through `fetch`,
// which answers on the main thread; an answer that arrives after the panel closed is dropped.
class ChatPanel : public ui::Stack {
public:
    using Done = std::function<void(core::Result<youtube::ChatPage>)>;
    using Fetch = std::function<void(const std::string& continuation, Done done)>;
    static constexpr int kMaxShown = 60;
    static constexpr int kPollMs = 3000;
    static constexpr int kRetryMs = 10000;

    ChatPanel(std::string continuation, Fetch fetch);
    ~ChatPanel() override;

    int count() const { return static_cast<int>(shown_.size()); }

private:
    void load();
    void show(core::Result<youtube::ChatPage> page);
    void later(int delayMs);
    void add(const youtube::ChatMessage& message);
    void remove(const std::string& id);

    std::string continuation_;
    Fetch fetch_;
    core::CancelToken token_;
    std::shared_ptr<bool> alive_ = std::make_shared<bool>(true);
    ui::Box* card_;
    ui::Box* list_;
    ui::Label* status_;
    std::deque<std::pair<std::string, ui::View*>> shown_;  // id and view, oldest first
};

}  // namespace playtube::app
