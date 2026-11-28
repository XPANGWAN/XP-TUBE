#include <SDL.h>

#include <algorithm>
#include <cerrno>
#include <deque>
#include <vector>

#include "core/boot_trace.hpp"
#include "core/data_dir.hpp"
#include "core/log.hpp"
#include "core/version.hpp"
#include "platform/backend.hpp"
#include "platform/platform.hpp"
#include "platform/remote.hpp"
#include "platform/text_input.hpp"
#include "xp/platform/controller_registry.hpp"
#include "xp/platform/controller_input.hpp"

namespace playtube::platform {

namespace {

SDL_Window* gWindow = nullptr;
SDL_GLContext gContext = nullptr;
EventTranslator gTranslator;
KeyRepeat gRepeat;
RemoteButtons gRemote;       // the TV's remote control (the console only)
std::deque<Event> gPending;  // its events not given yet
std::uint32_t gRemoteReadMs = 0xFFFFFFFFu;
std::vector<SDL_GameController*> gControllers;
xp::platform::ControllerRegistry gXpControllers;
xp::platform::ControllerInputTranslator gXpControllerInput;

void openController(int deviceIndex) {
    if (!SDL_IsGameController(deviceIndex)) {
        PF_LOG_WARN("input device %d is not a game controller", deviceIndex);
        return;
    }
    SDL_GameController* controller = SDL_GameControllerOpen(deviceIndex);
    if (controller == nullptr) {
        PF_LOG_WARN("cannot open controller %d: %s", deviceIndex, SDL_GetError());
        return;
    }
    const SDL_JoystickID instance = SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(controller));
    if (instance < 0 || !gXpControllers.connect(static_cast<std::uint32_t>(instance))) {
        PF_LOG_WARN("controller %d could not be registered by XP Platform", deviceIndex);
        SDL_GameControllerClose(controller);
        return;
    }
    PF_LOG_INFO("controller connected: %s", SDL_GameControllerName(controller));
    gControllers.push_back(controller);
}

void closeController(SDL_JoystickID instance) {
    auto it = std::find_if(gControllers.begin(), gControllers.end(), [instance](SDL_GameController* c) {
        return SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(c)) == instance;
    });
    if (it != gControllers.end()) {
        PF_LOG_INFO("controller disconnected");
        gXpControllers.disconnect(static_cast<std::uint32_t>(instance));
        gXpControllerInput.disconnect(static_cast<std::uint32_t>(instance));
        SDL_GameControllerClose(*it);
        gControllers.erase(it);
    }
}

std::optional<Event> translateXpControllerEvent(const SDL_Event& source) {
    using xp::platform::ControllerButton;
    using xp::platform::ControllerAxis;
    using xp::platform::EventType;

    const auto device = static_cast<std::uint32_t>(
        source.type == SDL_CONTROLLERAXISMOTION ? source.caxis.which : source.cbutton.which);
    std::optional<xp::platform::Event> translated;

    if (source.type == SDL_CONTROLLERBUTTONDOWN || source.type == SDL_CONTROLLERBUTTONUP) {
        ControllerButton button = ControllerButton::Unknown;
        switch (source.cbutton.button) {
        case SDL_CONTROLLER_BUTTON_DPAD_UP: button = ControllerButton::Up; break;
        case SDL_CONTROLLER_BUTTON_DPAD_DOWN: button = ControllerButton::Down; break;
        case SDL_CONTROLLER_BUTTON_DPAD_LEFT: button = ControllerButton::Left; break;
        case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: button = ControllerButton::Right; break;
        case SDL_CONTROLLER_BUTTON_A: button = ControllerButton::Confirm; break;
        case SDL_CONTROLLER_BUTTON_B: button = ControllerButton::Cancel; break;
        case SDL_CONTROLLER_BUTTON_Y: button = ControllerButton::Options; break;
        case SDL_CONTROLLER_BUTTON_X: button = ControllerButton::Context; break;
        case SDL_CONTROLLER_BUTTON_LEFTSHOULDER: button = ControllerButton::L1; break;
        case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: button = ControllerButton::R1; break;
        case SDL_CONTROLLER_BUTTON_START: button = ControllerButton::Menu; break;
        case SDL_CONTROLLER_BUTTON_TOUCHPAD: button = ControllerButton::Touchpad; break;
        default: break;
        }
        translated = gXpControllerInput.button(device, button,
                                                source.type == SDL_CONTROLLERBUTTONDOWN,
                                                SDL_GetTicks());
    } else if (source.type == SDL_CONTROLLERAXISMOTION) {
        ControllerAxis axis;
        if (source.caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERLEFT) {
            axis = ControllerAxis::LeftTrigger;
        } else if (source.caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERRIGHT) {
            axis = ControllerAxis::RightTrigger;
        } else {
            return std::nullopt;
        }
        translated = gXpControllerInput.axis(device, axis, source.caxis.value, SDL_GetTicks());
    }

    if (!translated) return std::nullopt;
    Action action;
    switch (translated->button) {
    case ControllerButton::Up: action = Action::Up; break;
    case ControllerButton::Down: action = Action::Down; break;
    case ControllerButton::Left: action = Action::Left; break;
    case ControllerButton::Right: action = Action::Right; break;
    case ControllerButton::Confirm: action = Action::Confirm; break;
    case ControllerButton::Cancel: action = Action::Back; break;
    case ControllerButton::Options: action = Action::Options; break;
    case ControllerButton::Context: action = Action::Context; break;
    case ControllerButton::L1: action = Action::L1; break;
    case ControllerButton::R1: action = Action::R1; break;
    case ControllerButton::L2: action = Action::L2; break;
    case ControllerButton::R2: action = Action::R2; break;
    case ControllerButton::Menu: action = Action::Menu; break;
    case ControllerButton::Touchpad: action = Action::Touchpad; break;
    default: return std::nullopt;
    }
    const auto type = translated->type == EventType::ControllerButtonDown
                          ? Event::Type::ActionDown : Event::Type::ActionUp;
    return actionEvent(type, action);
}

}  // namespace

bool init(const Config& config) {
    // the log comes first: any later failure, SDL included, must reach the file (the only log readable on the PS4)
    const std::string dataDir = config.dataDirOverride.empty() ? backend::defaultDataDir() : config.dataDirOverride;
    const bool dataDirOk = core::makeDirs(dataDir);
    core::bootMark("data folder ok", dataDirOk ? 1 : 0);
    if (dataDirOk) {
        core::setDataRoot(dataDir);
        if (const auto logPath = core::dataPath("playtube.log"); !logPath || !log::openFile(*logPath)) {
            PF_LOG_WARN("cannot open the log file, logging to stderr only");
        }
    } else {
        // no data root: dataPath() gives nothing, so nothing is written anywhere; the app still runs
        PF_LOG_ERROR("cannot create the data directory %s (errno %d): nothing will be saved", dataDir.c_str(), errno);
    }
    PF_LOG_INFO("startup: SDL");
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) != 0) {
        PF_LOG_ERROR("SDL_Init failed: %s", SDL_GetError());
        return false;
    }
    PF_LOG_INFO("startup: window");

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    const Uint32 flags = SDL_WINDOW_OPENGL | (config.hidden ? SDL_WINDOW_HIDDEN : 0u);
    gWindow = SDL_CreateWindow(core::appTitle(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, config.width,
                               config.height, flags);
    if (gWindow == nullptr) {
        PF_LOG_ERROR("SDL_CreateWindow failed: %s", SDL_GetError());
        return false;
    }
    PF_LOG_INFO("startup: OpenGL ES context");
    gContext = SDL_GL_CreateContext(gWindow);
    if (gContext == nullptr) {
        PF_LOG_ERROR("SDL_GL_CreateContext failed: %s", SDL_GetError());
        return false;
    }
    SDL_GL_MakeCurrent(gWindow, gContext);
    SDL_GL_SetSwapInterval(1);
    SDL_StopTextInput();  // desktop SDL starts with text input on
    backend::postInit();
    return true;
}

void shutdown() {
    for (SDL_GameController* controller : gControllers) {
        const SDL_JoystickID instance = SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(controller));
        if (instance >= 0) {
            gXpControllers.disconnect(static_cast<std::uint32_t>(instance));
            gXpControllerInput.disconnect(static_cast<std::uint32_t>(instance));
        }
        SDL_GameControllerClose(controller);
    }
    gControllers.clear();
    if (gContext != nullptr) {
        SDL_GL_DeleteContext(gContext);
        gContext = nullptr;
    }
    if (gWindow != nullptr) {
        SDL_DestroyWindow(gWindow);
        gWindow = nullptr;
    }
    SDL_Quit();
}

bool pollEvent(Event& out, std::uint32_t nowMs) {
    SDL_Event event;
    while (SDL_PollEvent(&event) != 0) {
        if (event.type == SDL_CONTROLLERDEVICEADDED) {
            openController(event.cdevice.which);
        } else if (event.type == SDL_CONTROLLERDEVICEREMOVED) {
            closeController(event.cdevice.which);
        }
        const auto translated = (event.type == SDL_CONTROLLERBUTTONDOWN ||
                                 event.type == SDL_CONTROLLERBUTTONUP ||
                                 event.type == SDL_CONTROLLERAXISMOTION)
                                    ? translateXpControllerEvent(event)
                                    : gTranslator.translate(event);
        if (!translated) {
            continue;
        }
        if (translated->type == Event::Type::ActionDown) {
            gRepeat.press(translated->action, nowMs);
        } else if (translated->type == Event::Type::ActionUp) {
            gRepeat.release(translated->action);
        }
        out = *translated;
        return true;
    }
    // the remote control, read once a poll time (pollEvent is called until it has nothing more)
    if (gPending.empty() && nowMs != gRemoteReadMs) {
        gRemoteReadMs = nowMs;
        std::uint32_t buttons = 0;
        std::uint8_t key = 0;
        for (Event& fromRemote :
             backend::readRemote(buttons, key) ? gRemote.update(buttons, key) : gRemote.release()) {
            gPending.push_back(std::move(fromRemote));
        }
    }
    if (!gPending.empty()) {
        out = std::move(gPending.front());
        gPending.pop_front();
        if (out.type == Event::Type::ActionDown) {
            gRepeat.press(out.action, nowMs);
        } else if (out.type == Event::Type::ActionUp) {
            gRepeat.release(out.action);
        }
        return true;
    }
    if (const auto repeated = gRepeat.poll(nowMs)) {
        out = actionEvent(Event::Type::ActionDown, *repeated);
        return true;
    }
    return false;
}

void swap() { SDL_GL_SwapWindow(gWindow); }

Size drawableSize() {
    Size size;
    SDL_GL_GetDrawableSize(gWindow, &size.width, &size.height);
    return size;
}

std::uint32_t ticksMs() { return SDL_GetTicks(); }

void setTextEvents(bool on) {
    gTranslator.setTextMode(on);
    if (on) {
        SDL_StartTextInput();
    } else {
        SDL_StopTextInput();
    }
}

std::string systemLanguage() { return backend::systemLanguage(); }
int utcOffsetMinutes(std::int64_t utcSeconds) { return backend::utcOffsetMinutes(utcSeconds); }

std::string resourceDir() { return backend::resourceDir(); }

}  // namespace playtube::platform
