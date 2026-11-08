#include "keyboardlayoutscope.h"
#include <QtGlobal>

#ifdef Q_OS_WIN
#include <SDL_syswm.h>
#include <cstdint>
#include <vector>

namespace {
bool isPlainUS(HKL layout)
{
    return (reinterpret_cast<uintptr_t>(layout) & 0xFFFFFFFFu) == 0x04090409u;
}

bool isKorean(HKL layout)
{
    const auto value = reinterpret_cast<uintptr_t>(layout);
    return PRIMARYLANGID(LOWORD(value)) == LANG_KOREAN || HIWORD(value) == 0x0412;
}
}

struct KeyboardLayoutScope::Impl {
    bool requested = false;
    bool result = true;
    HKL previous = nullptr;
    DWORD thread = 0;
};
#else
struct KeyboardLayoutScope::Impl {};
#endif

KeyboardLayoutScope::KeyboardLayoutScope() : m_Impl(std::make_unique<Impl>()) {}
KeyboardLayoutScope::~KeyboardLayoutScope() { setActive(nullptr, false); }

bool KeyboardLayoutScope::setActive(SDL_Window* window, bool active)
{
#ifdef Q_OS_WIN
    auto& self = *m_Impl;
    if (!active) {
        if (self.previous) {
            if (GetCurrentThreadId() != self.thread) {
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Mac keyboard layout restore: wrong thread");
                return false;
            }
            // Respect an explicit input-language change made during capture.
            if (isPlainUS(GetKeyboardLayout(0))) {
                if (!ActivateKeyboardLayout(self.previous, 0)) {
                    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Mac keyboard layout restore failed: %lu", GetLastError());
                    return false;
                }
                SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Mac keyboard layout restored: %p", self.previous);
            }
            self.previous = nullptr;
        }
        self.requested = false;
        self.result = true;
        return true;
    }
    if (self.requested) return self.result;

    SDL_SysWMinfo info{};
    SDL_VERSION(&info.version);
    if (!window || !SDL_GetWindowWMInfo(window, &info) ||
        GetWindowThreadProcessId(info.info.win.window, nullptr) != GetCurrentThreadId() ||
        GetForegroundWindow() != info.info.win.window) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Mac keyboard layout: stream is not focused on this thread");
        return false;
    }

    self.requested = true;
    const auto original = GetKeyboardLayout(0);
    if (!isKorean(original)) return true;

    const int count = GetKeyboardLayoutList(0, nullptr);
    std::vector<HKL> layouts(count > 0 ? count : 0);
    const int actual = GetKeyboardLayoutList(static_cast<int>(layouts.size()), layouts.data());
    HKL us = nullptr;
    for (int i = 0; i < actual && i < static_cast<int>(layouts.size()); ++i) {
        if (isPlainUS(layouts[i])) { us = layouts[i]; break; }
    }
    if (!us) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Mac keyboard layout: US layout not loaded; Korean modifier normalization unavailable");
        self.result = false;
        return false;
    }

    // Flags=0 scopes activation to this thread. Do not use LoadKeyboardLayout,
    // KLF_SETFORPROCESS or KLF_REORDER, which would widen the change's scope.
    const auto previous = ActivateKeyboardLayout(us, 0);
    if (!previous) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Mac keyboard layout activation failed: %lu", GetLastError());
        self.result = false;
        return false;
    }
    self.previous = previous;
    self.thread = GetCurrentThreadId();
    self.result = isPlainUS(GetKeyboardLayout(0));
    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Mac keyboard layout: Korean -> US on stream thread; previous=%p; verified=%d",
                previous, self.result);
    return self.result;
#else
    Q_UNUSED(window);
    return !active;
#endif
}
