#include "ui/context.hpp"

namespace playtube::ui {

Context& context() {
    static Context instance;
    return instance;
}

}  // namespace playtube::ui
