#include "rawkeyboardscope.h"
#include <string>
#include <array>
#ifdef _WIN32
#include <SDL_syswm.h>
#endif

namespace {
constexpr const char* hints[] = {
    "SDL_WINDOWS_RAW_KEYBOARD", "SDL_WINDOWS_RAW_KEYBOARD_INPUTSINK",
    "SDL_WINDOWS_RAW_KEYBOARD_EXCLUDE_HOTKEYS"
};
constexpr const char* values[] = {"1", "0", "0"};
}

struct RawKeyboardScope::Impl {
    bool active = false;
    struct Hint { bool present = false; std::string value; };
    std::array<Hint, 3> previous;
};

RawKeyboardScope::RawKeyboardScope() : m_Impl(std::make_unique<Impl>()) {}
RawKeyboardScope::~RawKeyboardScope() { setActive(nullptr, false); }

bool RawKeyboardScope::hasForeground(SDL_Window* window)
{
    if (!window || !(SDL_GetWindowFlags(window) & SDL_WINDOW_INPUT_FOCUS)) return false;
#ifdef _WIN32
    SDL_SysWMinfo info{};
    SDL_VERSION(&info.version);
    return SDL_GetWindowWMInfo(window, &info) && GetForegroundWindow() == info.info.win.window;
#else
    return true;
#endif
}

bool RawKeyboardScope::setActive(SDL_Window* window, bool active)
{
    auto& self = *m_Impl;
    if (!active) {
        if (self.active) {
            // Stop raw capture before restoring any background-input preference.
            SDL_SetHintWithPriority(hints[0], "0", SDL_HINT_OVERRIDE);
            for (int i = 2; i >= 0; --i) {
                SDL_SetHintWithPriority(hints[i], self.previous[i].present ? self.previous[i].value.c_str() : nullptr,
                                        SDL_HINT_OVERRIDE);
            }
            self.active = false;
            SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Mac raw keyboard off; Windows layout unchanged");
        }
        return true;
    }
    if (!hasForeground(window)) return false;
    if (self.active) return true;
#ifdef _WIN32
    SDL_version version;
    SDL_GetVersion(&version);
    if (!(version.major == 2 && version.minor >= 30 && version.patch >= 50) && !SDL_GetHint("SDL3_VERSION")) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Mac raw keyboard requires SDL3/sdl2-compat; keeping normal input without changing Windows layout");
        return false;
    }
    for (size_t i = 0; i < self.previous.size(); ++i) {
        const char* value = SDL_GetHint(hints[i]);
        self.previous[i] = {value != nullptr, value ? value : ""};
    }
    // Configure foreground-only delivery before enabling raw input.
    for (int i = 2; i >= 0; --i) SDL_SetHintWithPriority(hints[i], values[i], SDL_HINT_OVERRIDE);
    self.active = true;
    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Mac raw keyboard on; foreground only; Windows layout unchanged");
    return true;
#else
    return false;
#endif
}
