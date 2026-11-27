#include "platform/text_input.hpp"

namespace playtube::platform {

bool systemKeyboardAvailable() { return false; }

std::string consoleUserId() { return {}; }
bool openSystemKeyboard(const TextInputRequest& request) {
    (void)request;
    return false;
}
KeyboardStatus pollSystemKeyboard(std::string& text) {
    (void)text;
    return KeyboardStatus::Idle;
}

}  // namespace playtube::platform
