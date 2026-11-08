#include "capslockcapture.h"
#include <QtGlobal>
#include <cstdint>

#ifdef Q_OS_WIN
#include <SDL_syswm.h>

struct CapsLockCapture::Impl {
    HHOOK hook = nullptr;
    SDL_Window* window = nullptr;
    bool down = false;
    bool ignoreHeld = false;
    Uint32 generation = 0;
    static Impl* current;
    static Uint32 nextGeneration;

    static LRESULT CALLBACK callback(int code, WPARAM message, LPARAM data)
    {
        if (code != HC_ACTION || !current) return CallNextHookEx(nullptr, code, message, data);
        const auto* key = reinterpret_cast<const KBDLLHOOKSTRUCT*>(data);
        if (key->vkCode != VK_CAPITAL) return CallNextHookEx(nullptr, code, message, data);

        auto& self = *current;
        // Obtain the current native window, including after SDL recreates it.
        SDL_SysWMinfo info{};
        SDL_VERSION(&info.version);
        if (!SDL_GetWindowWMInfo(self.window, &info) ||
            GetForegroundWindow() != info.info.win.window) {
            return CallNextHookEx(nullptr, code, message, data);
        }
        const bool pressed = message == WM_KEYDOWN || message == WM_SYSKEYDOWN;
        if (self.ignoreHeld) {
            if (!pressed) self.ignoreHeld = false;
            return CallNextHookEx(nullptr, code, message, data);
        }
        if (pressed == self.down) {
            // Suppress repeat down, but pass an up whose down preceded capture.
            return pressed ? 1 : CallNextHookEx(nullptr, code, message, data);
        }

        SDL_Event event{};
        // SDL2-compat converts keyboard events through SDL3 and does not retain
        // keysym.unused. A user event preserves the capture generation intact.
        event.type = SDL_USEREVENT;
        event.user.timestamp = SDL_GetTicks();
        event.user.windowID = SDL_GetWindowID(self.window);
        event.user.code = CapsLockCapture::EventCode;
        event.user.data1 = reinterpret_cast<void*>(static_cast<uintptr_t>(self.generation));
        event.user.data2 = reinterpret_cast<void*>(static_cast<uintptr_t>(pressed));
        // Queue only: no network I/O or logging inside the low-level hook.
        if (SDL_PushEvent(&event) != 1) {
            self.ignoreHeld = pressed;
            self.down = false;
            return CallNextHookEx(nullptr, code, message, data);
        }
        self.down = pressed;
        return 1; // Windows must not receive this Caps Lock transition.
    }
};

CapsLockCapture::Impl* CapsLockCapture::Impl::current = nullptr;
Uint32 CapsLockCapture::Impl::nextGeneration = 0;
#else
struct CapsLockCapture::Impl {};
#endif

CapsLockCapture::CapsLockCapture() : m_Impl(std::make_unique<Impl>()) {}
CapsLockCapture::~CapsLockCapture() { setActive(nullptr, false); }

bool CapsLockCapture::setActive(SDL_Window* window, bool active)
{
#ifdef Q_OS_WIN
    auto& self = *m_Impl;
    if (!active) {
        if (self.hook) UnhookWindowsHookEx(self.hook);
        if (Impl::current == &self) Impl::current = nullptr;
        self.hook = nullptr;
        self.window = nullptr;
        self.down = false;
        return true;
    }
    self.window = window;
    if (self.hook) return true;
    if (!window || (Impl::current && Impl::current != &self)) return false;
    self.ignoreHeld = (GetAsyncKeyState(VK_CAPITAL) & 0x8000) != 0;
    self.generation = 0xCA000000u | (++Impl::nextGeneration & 0x00FFFFFFu);
    Impl::current = &self;
    self.hook = SetWindowsHookExW(WH_KEYBOARD_LL, Impl::callback, GetModuleHandleW(nullptr), 0);
    if (!self.hook) {
        Impl::current = nullptr;
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "CapsLock capture install failed: %lu", GetLastError());
        return false;
    }
    return true;
#else
    Q_UNUSED(window);
    return !active;
#endif
}

bool CapsLockCapture::active() const
{
#ifdef Q_OS_WIN
    return m_Impl->hook != nullptr;
#else
    return false;
#endif
}

bool CapsLockCapture::owns(const SDL_KeyboardEvent& event) const
{
#ifdef Q_OS_WIN
    return active() && event.keysym.unused == m_Impl->generation;
#else
    Q_UNUSED(event);
    return false;
#endif
}

bool CapsLockCapture::translate(const SDL_UserEvent& source, SDL_KeyboardEvent& target) const
{
#ifdef Q_OS_WIN
    if (!active() || source.code != EventCode ||
        reinterpret_cast<uintptr_t>(source.data1) != m_Impl->generation) return false;
    target = {};
    const bool down = reinterpret_cast<uintptr_t>(source.data2) != 0;
    target.type = down ? SDL_KEYDOWN : SDL_KEYUP;
    target.state = down ? SDL_PRESSED : SDL_RELEASED;
    target.timestamp = source.timestamp;
    target.windowID = source.windowID;
    target.keysym.scancode = SDL_SCANCODE_CAPSLOCK;
    target.keysym.sym = SDLK_CAPSLOCK;
    target.keysym.unused = m_Impl->generation;
    return true;
#else
    Q_UNUSED(source);
    Q_UNUSED(target);
    return false;
#endif
}

int CapsLockCapture::localToggleState() const
{
#ifdef Q_OS_WIN
    return GetKeyState(VK_CAPITAL) & 1;
#else
    return -1;
#endif
}
