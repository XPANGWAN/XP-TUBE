#include <memory>
#include <string>

#include "app/shell/chrome.hpp"
#include "app/shell/screen.hpp"
#include "app/shell/screens.hpp"
#include "core/log.hpp"
#include "ui/i18n.hpp"
#include "ui/widgets.hpp"

namespace playtube::app {

namespace {

// Sign-in by code: the code to type on another device, then polls every `interval` seconds until the user is
// done (a new code when it expires).
class SignInScreen : public ShellScreen {
public:
    SignInScreen(AppSession& session, std::function<void()> done) : ShellScreen(session), done_(std::move(done)) {
        auto* column = add<ui::Box>(ui::Axis::Column);
        column->width = ui::SizeSpec::fill();
        column->padding = {120, 160, 0, 160};
        column->gap = ui::theme().spacingL;
        column->add<ui::Label>(ui::tr("signin.title"), ui::FontStyle{ui::theme().fontLogo, ui::FontWeight::Bold});
        steps_ = column->add<ui::Label>("", titleText(ui::FontWeight::Regular));
        steps_->color = ui::theme().textSecondary;
        steps_->maxLines = 3;
        steps_->width = ui::SizeSpec::fixed(1400);
        address_ = column->add<ui::Label>("", ui::FontStyle{48, ui::FontWeight::SemiBold});
        code_ = column->add<ui::Label>("", ui::FontStyle{ui::theme().fontDisplay * 1.4f, ui::FontWeight::Bold});
        code_->color = ui::theme().accent;
        auto* status = column->add<ui::Box>(ui::Axis::Row);
        status->gap = ui::theme().spacingM;
        status->crossAlign = ui::CrossAlign::Center;
        spinner_ = status->add<ui::Spinner>();
        status_ = status->add<ui::Label>(ui::tr("shell.loading"), bodyText());
        status_->color = ui::theme().textSecondary;
        retry_ = column->add<ui::Button>(ui::tr("ui.retry"), [this] { later([this] { start(); }); });
        retry_->visible = false;
        start();
    }

private:
    void start() {
        code_->setText({});
        address_->setText({});
        retry_->visible = false;
        spinner_->visible = true;
        status_->setText(ui::tr("shell.loading"));
        ++generation_;
        AppSession* shared = &session_;
        const core::CancelToken cancel = token_;
        run<core::Result<youtube::DeviceCode>>([shared, cancel] { return shared->auth().start(cancel); },
                                               [this](core::Result<youtube::DeviceCode> code) { showCode(code); });
    }

    void showCode(const core::Result<youtube::DeviceCode>& code) {
        if (!code.ok()) {
            fail(errorText(code.error()));
            return;
        }
        code_->setText(code.value().userCode);
        address_->setText(code.value().verificationUrl);
        steps_->setText(ui::tr("signin.steps"));
        status_->setText(ui::tr("signin.waiting"));
        deviceCode_ = code.value().deviceCode;
        intervalMs_ = static_cast<float>(code.value().interval) * 1000.0f;
        leftMs_ = static_cast<float>(code.value().expiresIn) * 1000.0f;
        schedulePoll();
    }

    void schedulePoll() {
        const unsigned generation = generation_;
        leftMs_ -= intervalMs_;
        after(intervalMs_, [this, generation] {
            if (generation == generation_) {
                poll();
            }
        });
    }

    void poll() {
        if (leftMs_ <= 0) {
            PF_LOG_INFO("sign-in: the code expired, a new one");
            start();
            return;
        }
        AppSession* shared = &session_;
        const std::string device = deviceCode_;
        const core::CancelToken cancel = token_;
        const unsigned generation = generation_;
        run<core::Result<youtube::Token>>(
            [shared, device, cancel] { return shared->auth().poll(device, cancel); },
            [this, generation](core::Result<youtube::Token> token) {
                if (generation != generation_) {
                    return;
                }
                if (!token.ok()) {
                    if (token.error().kind == core::ErrorKind::Unauthorized) {
                        start();  // refused or expired: a new code
                    } else {
                        fail(errorText(token.error()));
                    }
                    return;
                }
                if (token.value().pending) {
                    if (token.value().slowDown) {
                        intervalMs_ += 5000;  // asked to poll less often
                    }
                    schedulePoll();
                    return;
                }
                signedIn(token.value());
            });
    }

    // signed in at once (leaving the screen now keeps the sign-in); the name follows
    void signedIn(const youtube::Token& token) {
        status_->setText(ui::tr("signin.done"));
        session_.account().signIn(token, {}, session_.now());
        PF_LOG_INFO("sign-in: done");
        AppSession* shared = &session_;
        const std::string access = token.accessToken;
        run<bool>(
            [shared, access] {
                // not cancelled with the screen: the name is kept even when the user left
                const auto name = shared->youtube().accountName(access);
                if (name.ok()) {
                    shared->account().setName(name.value());
                }
                return name.ok();
            },
            [this](bool) {
                auto done = done_;
                if (ui::Application* app = ui::Application::current()) {
                    app->pop();  // this screen is gone after this line
                }
                if (done) {
                    done();
                }
            });
    }

    void fail(const std::string& text) {
        spinner_->visible = false;
        status_->setText(text);
        retry_->visible = true;
        focusIfEmpty(retry_);
    }

    std::function<void()> done_;
    ui::Label* steps_;
    ui::Label* address_;
    ui::Label* code_;
    ui::Spinner* spinner_;
    ui::Label* status_;
    ui::Button* retry_;
    std::string deviceCode_;
    float intervalMs_ = 5000;
    float leftMs_ = 0;
    unsigned generation_ = 0;
};

}  // namespace

void openSignIn(AppSession& session, std::function<void()> done) {
    if (ui::Application* app = ui::Application::current()) {
        app->push(std::make_unique<SignInScreen>(session, std::move(done)));
    }
}

}  // namespace playtube::app
