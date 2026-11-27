#include "ui/view.hpp"

#include <nanovg.h>

#include <algorithm>

#include "ui/animation.hpp"
#include "ui/context.hpp"
#include "ui/draw.hpp"
#include "ui/focus.hpp"

namespace playtube::ui {

namespace {
float gDrawAlpha = 1;  // product of the opacities of the views being drawn (main thread)
// What can show of the view being drawn, in its own coordinates: the clipping ancestors' areas (main thread). A
// child entirely outside it is not drawn, whatever its depth (the rows of a long page below the screen).
Rect gVisible{-1e7f, -1e7f, 2e7f, 2e7f};
// views may draw past their frame (a focused card grows, glows, casts its shadow): kept within this margin
constexpr float kCullMargin = 120;

Rect intersection(const Rect& a, const Rect& b) {
    const float x = std::max(a.x, b.x);
    const float y = std::max(a.y, b.y);
    return {x, y, std::max(0.0f, std::min(a.right(), b.right()) - x), std::max(0.0f, std::min(a.bottom(), b.bottom()) - y)};
}
}  // namespace

View::~View() {
    Context& ctx = context();
    if (ctx.focus != nullptr) {
        ctx.focus->forget(this);
    }
    if (ctx.animator != nullptr) {
        ctx.animator->cancel(this);
    }
}

View* View::addChild(std::unique_ptr<View> child) {
    View* raw = adopt(std::move(child));
    markDirty();
    return raw;
}

std::unique_ptr<View> View::takeChild(View* child) {
    auto taken = release(child);
    if (taken) {
        markDirty();
    }
    return taken;
}

View* View::adopt(std::unique_ptr<View> child) {
    child->parent_ = this;
    children_.push_back(std::move(child));
    return children_.back().get();
}

std::unique_ptr<View> View::release(View* child) {
    auto it = std::find_if(children_.begin(), children_.end(),
                           [child](const std::unique_ptr<View>& entry) { return entry.get() == child; });
    if (it == children_.end()) {
        return nullptr;
    }
    std::unique_ptr<View> taken = std::move(*it);
    children_.erase(it);
    taken->parent_ = nullptr;
    return taken;
}

void View::clearChildren() {
    children_.clear();
    markDirty();
}

bool View::isAncestorOf(const View* view) const {
    for (const View* v = view; v != nullptr; v = v->parent_) {
        if (v == this) {
            return true;
        }
    }
    return false;
}

bool View::containsPointer(const View* view) const {
    if (view == this) {
        return true;
    }
    for (const auto& child : children_) {
        if (child->containsPointer(view)) {
            return true;
        }
    }
    return false;
}

Rect View::absoluteFrame() const {
    Rect rect = frame;
    for (const View* p = parent_; p != nullptr; p = p->parent_) {
        const Point offset = p->contentOffset();
        rect.x += p->frame.x + offset.x;
        rect.y += p->frame.y + offset.y;
    }
    return rect;
}

Size resolveSize(View& view, Size available, TextMetrics& metrics) {
    const Size avail{std::max(0.0f, available.width - view.margin.horizontal()),
                     std::max(0.0f, available.height - view.margin.vertical())};
    Size natural;
    if (view.width.mode == SizeMode::Fit || view.height.mode == SizeMode::Fit) {
        const Size measureSpace{view.width.mode == SizeMode::Fixed ? view.width.value : avail.width,
                                view.height.mode == SizeMode::Fixed ? view.height.value : avail.height};
        natural = view.measure(measureSpace, metrics);
    }
    Size size;
    switch (view.width.mode) {
    case SizeMode::Fixed:
        size.width = view.width.value;
        break;
    case SizeMode::Fill:
        size.width = avail.width;
        break;
    case SizeMode::Fit:
        size.width = natural.width;
        break;
    }
    switch (view.height.mode) {
    case SizeMode::Fixed:
        size.height = view.height.value;
        break;
    case SizeMode::Fill:
        size.height = avail.height;
        break;
    case SizeMode::Fit:
        size.height = natural.height;
        break;
    }
    return size;
}

Size View::measure(Size available, TextMetrics& metrics) {
    const Size inner{std::max(0.0f, available.width - padding.horizontal()),
                     std::max(0.0f, available.height - padding.vertical())};
    Size content;
    for (const auto& child : children_) {
        if (!child->visible) {
            continue;
        }
        const Size size = resolveSize(*child, inner, metrics);
        content.width = std::max(content.width, size.width + child->margin.horizontal());
        content.height = std::max(content.height, size.height + child->margin.vertical());
    }
    return {content.width + padding.horizontal(), content.height + padding.vertical()};
}

void View::place(const Rect& rect, TextMetrics& metrics) {
    frame = rect;
    layout(metrics);
    dirty_ = false;
}

void View::layout(TextMetrics& metrics) {
    const Size inner{std::max(0.0f, frame.width - padding.horizontal()),
                     std::max(0.0f, frame.height - padding.vertical())};
    for (const auto& child : children_) {
        if (!child->visible) {
            continue;
        }
        const Size size = resolveSize(*child, inner, metrics);
        child->place({padding.left + child->margin.left, padding.top + child->margin.top, size.width, size.height},
                     metrics);
    }
}

void View::draw(NVGcontext* vg) const {
    drawBackground(vg);
    drawChildren(vg);
}

void View::drawBackground(NVGcontext* vg) const {
    if (background.a == 0) {
        return;
    }
    nvgBeginPath(vg);
    if (cornerRadius > 0) {
        nvgRoundedRect(vg, 0, 0, frame.width, frame.height, cornerRadius);
    } else {
        nvgRect(vg, 0, 0, frame.width, frame.height);
    }
    nvgFillColor(vg, nvgColor(background));
    nvgFill(vg);
}

void View::onThemeChanged() {
    for (const auto& child : children_) {
        child->onThemeChanged();
    }
    markDirty();
}

void View::drawChildren(NVGcontext* vg, const View* last) const {
    const Point offset = contentOffset();
    const Rect visibleArea{-offset.x, -offset.y, frame.width, frame.height};
    const Rect shown = clip ? intersection(gVisible, Rect{0, 0, frame.width, frame.height}) : gVisible;
    const auto drawChild = [&](const View& child) {
        if (!child.visible || (clip && !child.frame.intersects(visibleArea))) {
            return;
        }
        const Rect placed{child.frame.x + offset.x, child.frame.y + offset.y + child.shiftY, child.frame.width,
                          child.frame.height};
        const Rect reach{placed.x - kCullMargin, placed.y - kCullMargin, placed.width + 2 * kCullMargin,
                         placed.height + 2 * kCullMargin};
        if (!reach.intersects(shown)) {
            return;  // off screen (a clipping ancestor hides it all)
        }
        const Rect outer = gVisible;
        gVisible = Rect{shown.x - placed.x, shown.y - placed.y, shown.width, shown.height};
        struct Restore {
            Rect value;
            ~Restore() { gVisible = value; }
        } restore{outer};
        if (child.opacity <= 0) {
            return;
        }
        nvgSave(vg);
        const float inherited = gDrawAlpha;
        if (child.opacity < 1) {
            gDrawAlpha *= child.opacity;
            nvgGlobalAlpha(vg, gDrawAlpha);
        }
        nvgTranslate(vg, child.frame.x + offset.x, child.frame.y + offset.y + child.shiftY);
        if (child.clip) {
            nvgIntersectScissor(vg, 0, 0, child.frame.width, child.frame.height);
        }
        child.draw(vg);
        gDrawAlpha = inherited;
        nvgRestore(vg);
    };
    for (const auto& child : children_) {
        if (child.get() != last) {
            drawChild(*child);
        }
    }
    if (last != nullptr && last->parent() == this) {
        drawChild(*last);
    }
}

View* View::focusEntry() {
    if (canFocus()) {
        return this;
    }
    if (!visible) {
        return nullptr;
    }
    if (remembered != nullptr && remembered != this && containsPointer(remembered) && remembered->canFocus()) {
        return remembered;
    }
    for (const auto& child : children_) {
        if (!child->visible) {
            continue;
        }
        if (View* entry = child->focusEntry()) {
            return entry;
        }
    }
    return nullptr;
}

void View::bind(platform::Action action, std::string label, std::function<bool()> handler,
                std::function<bool()> shown) {
    bindings_.push_back(ActionBinding{action, std::move(label), std::move(handler), std::move(shown)});
}

void View::markDirty() {
    for (View* v = this; v != nullptr; v = v->parent_) {
        v->dirty_ = true;
    }
}

}  // namespace playtube::ui
