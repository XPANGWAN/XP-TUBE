#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "core/result.hpp"
#include "ui/box.hpp"
#include "ui/label.hpp"
#include "ui/scroll.hpp"
#include "youtube/models.hpp"

namespace playtube::app {

class CommentView;

// The comments of a video in a panel on the right, over the playing video (shown as a modal). ✕ on a comment with
// replies shows them under it (their first page), ✕ again hides them; the next page of comments is asked for when
// the focus comes within 3 comments of the end; ◯ closes the panel. Pages come through `fetch`, which answers on
// the main thread; an answer that arrives after the panel closed is dropped.
class CommentsPanel : public ui::Stack {
public:
    using Done = std::function<void(core::Result<youtube::CommentPage>)>;
    using Fetch = std::function<void(const std::string& continuation, Done done)>;

    CommentsPanel(std::string continuation, Fetch fetch);
    ~CommentsPanel() override;

    int count() const { return static_cast<int>(comments_.size()); }

private:
    void load();
    void show(core::Result<youtube::CommentPage> page);
    void toggleReplies(CommentView& view);
    void focusFirst();
    Done guarded(std::function<void(core::Result<youtube::CommentPage>)> then);

    std::string continuation_;
    Fetch fetch_;
    bool loading_ = false;
    std::shared_ptr<bool> alive_ = std::make_shared<bool>(true);
    ui::Box* card_;
    ui::VScroll* scroll_;
    ui::Box* list_;
    ui::Label* status_;
    std::vector<CommentView*> comments_;
};

}  // namespace playtube::app
