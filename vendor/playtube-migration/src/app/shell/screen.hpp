#pragma once

#include <functional>
#include <utility>

#include "app/shell/session.hpp"
#include "core/task.hpp"
#include "ui/application.hpp"

namespace playtube::app {

// Shell screens start network jobs with run(): on the worker pool, result on the main thread, dropped when the
// screen is destroyed (cancel token). In test mode jobs run inline. A `done` callback may replace or pop the
// screen: nothing touches the screen after it.
class ShellScreen : public ui::Screen {
public:
    explicit ShellScreen(AppSession& session) : session_(session) {}
    ~ShellScreen() override { token_.cancel(); }

    AppSession& session() { return session_; }
    // Cancelled when the screen goes away: pass it to network calls so they stop early.
    const core::CancelToken& cancelToken() const { return token_; }

    template <typename T>
    void run(std::function<T()> job, std::function<void(T)> done) {
        ui::Application* app = ui::Application::current();
        if (session_.synchronous || app == nullptr) {
            done(job());
            return;
        }
        core::runAsync<T>(app->pool(), app->mainQueue(), std::move(job), std::move(done), token_);
    }

    // run() on the session's transfer thread (a long download that must not hold the pool)
    template <typename T>
    void runTransfer(std::function<T()> job, std::function<void(T)> done) {
        ui::Application* app = ui::Application::current();
        if (session_.synchronous || app == nullptr) {
            done(job());
            return;
        }
        core::runAsync<T>(session_.transfers(), app->mainQueue(), std::move(job), std::move(done), token_);
    }

    // next frame (after the screen is shown and focused)
    void later(std::function<void()> action) {
        ui::Application* app = ui::Application::current();
        if (app == nullptr) {
            return;
        }
        core::CancelToken token = token_;
        app->mainQueue().post([token, action = std::move(action)] {
            if (!token.cancelled()) {
                action();
            }
        });
    }

    // after `delayMs` of frame time, unless the screen is gone
    void after(float delayMs, std::function<void()> action) {
        if (ui::Application* app = ui::Application::current()) {
            app->after(delayMs, std::move(action), token_);
        }
    }

    // Where the focus starts (the screen's tab, its field): content that arrives takes the focus only while it is
    // still there. Also the first focus of the screen.
    void setStart(ui::View* view) {
        start_ = view;
        remembered = view;
    }
    ui::View* start() const { return start_; }

    // before `subtree` is emptied or hidden: a focus inside it goes to the start
    void moveFocusOff(ui::View* subtree) {
        ui::Application* app = ui::Application::current();
        if (app != nullptr && subtree->isAncestorOf(app->focus().focused()) && start_ != nullptr &&
            !subtree->isAncestorOf(start_)) {
            app->focus().focus(start_);
        }
    }

    // after content arrived: focus it (`target`, else the screen's first focusable view) when nothing of this screen
    // has the focus (and no dialog is open)
    void focusIfEmpty(ui::View* target = nullptr) {
        ui::Application* app = ui::Application::current();
        if (app == nullptr || app->hasModal() || app->top() != this) {
            return;
        }
        ui::View* focused = app->focus().focused();
        if (focused == nullptr || !isAncestorOf(focused) || focused == this) {
            if (target == nullptr || !app->focus().focus(target)) {
                app->focus().focus(this);
            }
        }
    }

    // after content arrived: focus it while the focus is still at the start, or nowhere in this screen
    void focusContent(ui::View* content) {
        ui::Application* app = ui::Application::current();
        if (app == nullptr || app->hasModal() || app->top() != this) {
            return;
        }
        ui::View* focused = app->focus().focused();
        if (focused == nullptr || focused == start_ || focused == this || !isAncestorOf(focused)) {
            app->focus().focus(content);
        }
    }

protected:
    AppSession& session_;
    core::CancelToken token_;
    ui::View* start_ = nullptr;
};

}  // namespace playtube::app
