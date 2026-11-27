#include "ui/scroll.hpp"

#include <algorithm>

#include "ui/animation.hpp"
#include "ui/box.hpp"
#include "ui/context.hpp"

namespace playtube::ui {

VScroll::VScroll() {
    clip = true;
    width = SizeSpec::fill();
    height = SizeSpec::fill();
}

void VScroll::setContent(std::unique_ptr<View> content) {
    clearChildren();
    addChild(std::move(content));
}

View* VScroll::content() const { return children().empty() ? nullptr : children().front().get(); }

Size VScroll::measure(Size available, TextMetrics& metrics) {
    View* view = content();
    if (maxHeight <= 0 || view == nullptr) {
        return available;
    }
    const float innerWidth = std::max(0.0f, available.width - padding.horizontal());
    const Size size = resolveSize(*view, {innerWidth, 1.0e6f}, metrics);
    return {available.width, std::min(maxHeight, size.height + view->margin.vertical() + padding.vertical())};
}

void VScroll::layout(TextMetrics& metrics) {
    View* view = content();
    if (view == nullptr) {
        return;
    }
    const float innerWidth = std::max(0.0f, frame.width - padding.horizontal());
    const Size size = resolveSize(*view, {innerWidth, 1.0e6f}, metrics);
    contentHeight_ = size.height + view->margin.vertical();
    view->place({padding.left + view->margin.left, padding.top + view->margin.top, innerWidth - view->margin.horizontal(),
                 size.height},
                metrics);
    offset_ = std::clamp(offset_, 0.0f, maxOffset());
}

float VScroll::maxOffset() const {
    return std::max(0.0f, contentHeight_ + padding.vertical() - frame.height);
}

void VScroll::scrollTo(float target) {
    target = std::clamp(target, 0.0f, maxOffset());
    if (Animator* animator = context().animator) {
        animator->animate(&offset_, target, theme().scrollMs, Easing::EaseOutCubic, this);
    } else {
        offset_ = target;
    }
}

void VScroll::onDescendantFocused(View* view) {
    View* body = content();
    if (body == nullptr || view == nullptr || !body->isAncestorOf(view)) {
        return;
    }
    if (policy == Policy::Leading) {
        if (view == body) {
            scrollTo(0);
            return;
        }
        // The row is the focused view's ancestor among the content's rows. Rows may sit in a column of their own
        // (a list of rows filled again on reload): a column holding a single row that can take the focus is
        // entered to find it.
        View* container = body;
        float top = 0;
        bool focusableAbove = false;  // a row above it can take the focus: what is above it (titles) may go
        while (true) {
            View* child = view;
            while (child->parent() != nullptr && child->parent() != container) {
                child = child->parent();
            }
            int focusableRows = 0;
            for (const auto& sibling : container->children()) {
                if (sibling->visible && sibling.get() != header_ && sibling->focusEntry() != nullptr) {
                    ++focusableRows;
                    if (sibling.get() != child && sibling->frame.y < child->frame.y) {
                        focusableAbove = true;
                    }
                }
            }
            auto* column = dynamic_cast<Box*>(child);
            top += child->frame.y;
            if (child == view || focusableRows > 1 || column == nullptr || column->axis != Axis::Column) {
                break;  // `child` is the row
            }
            container = child;
        }
        scrollTo(focusableAbove ? top - leadingInset : 0);
        return;
    }
    // view's rectangle in content coordinates
    float top = 0;
    for (const View* v = view; v != nullptr && v != body; v = v->parent()) {
        top += v->frame.y + (v->parent() != nullptr && v->parent() != body ? v->parent()->contentOffset().y : 0);
    }
    const float bottom = top + view->frame.height;
    const float viewport = frame.height - padding.vertical();
    if (top - nearestMargin < offset_) {
        scrollTo(top - nearestMargin);
    } else if (bottom + nearestBottomMargin > offset_ + viewport) {
        scrollTo(bottom + nearestBottomMargin - viewport);
    }
}

}  // namespace playtube::ui
