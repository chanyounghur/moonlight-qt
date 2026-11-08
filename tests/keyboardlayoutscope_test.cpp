#define SDL_MAIN_HANDLED
#include "streaming/input/keyboardlayoutscope.h"
#include <SDL_syswm.h>
#include <iostream>
#include <cstdint>

void check(bool ok, const char* message);
HWND nativeWindow(SDL_Window* window);

namespace {
void drain()
{
    SDL_Event event;
    while (SDL_PollEvent(&event)) {}
}

void focusWindow(SDL_Window* window)
{
    SDL_RaiseWindow(window);
    SetForegroundWindow(nativeWindow(window));
    drain();
    check(GetForegroundWindow() == nativeWindow(window), "layout test lost foreground");
}

void sendRightAlt(SDL_Window* window, bool down)
{
    check(GetForegroundWindow() == nativeWindow(window), "lost foreground before modifier input");
    INPUT input{};
    input.type = INPUT_KEYBOARD;
    input.ki.wScan = 0x38;
    input.ki.dwFlags = KEYEVENTF_SCANCODE | KEYEVENTF_EXTENDEDKEY | (down ? 0 : KEYEVENTF_KEYUP);
    check(SendInput(1, &input, sizeof(input)) == 1, "right Alt SendInput failed");
}

void countRightAlt(int& downs, int& ups)
{
    const auto start = SDL_GetTicks();
    do {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if ((event.type == SDL_KEYDOWN || event.type == SDL_KEYUP) &&
                event.key.keysym.scancode == SDL_SCANCODE_RALT) {
                if (event.type == SDL_KEYDOWN) ++downs;
                else ++ups;
            }
        }
        SDL_Delay(5);
    } while (SDL_GetTicks() - start < 150);
}

bool plainUS()
{
    return (reinterpret_cast<uintptr_t>(GetKeyboardLayout(0)) & 0xFFFFFFFFu) == 0x04090409u;
}
}

void testKeyboardLayoutScope(SDL_Window* stream, SDL_Window* other)
{
    focusWindow(stream);
    const auto original = GetKeyboardLayout(0);
    // Test fixture only, on the disposable CI runner. Production NEVER loads layouts.
    const auto us = LoadKeyboardLayoutW(L"00000409", KLF_ACTIVATE);
    const auto korean = LoadKeyboardLayoutW(L"00000412", KLF_ACTIVATE);
    check(us && korean, "CI fixture could not load US/Korean layouts");
    check(ActivateKeyboardLayout(korean, 0) != nullptr, "Korean fixture activation failed");
    drain();
    const int layoutCount = GetKeyboardLayoutList(0, nullptr);
    try {
        KeyboardLayoutScope scope;
        check(scope.setActive(stream, true), "Korean modifier normalization failed");
        check(plainUS(), "capture did not select plain US");
        check(scope.setActive(stream, true), "repeated capture failed");
        drain();

        SDL_SetWindowKeyboardGrab(stream, SDL_TRUE);
        int downs = 0, ups = 0;
        sendRightAlt(stream, true);
        countRightAlt(downs, ups);
        sendRightAlt(stream, false);
        countRightAlt(downs, ups);
        check(downs == 1 && ups == 1, "normalized right Alt must have SDL down AND up");
        SDL_SetWindowKeyboardGrab(stream, SDL_FALSE);

        focusWindow(other);
        check(scope.setActive(nullptr, false), "focus-loss restore failed");
        check(GetKeyboardLayout(0) == korean, "focus loss did not restore Korean layout");
        check(!scope.setActive(stream, true), "background stream must not change layout");
        check(GetKeyboardLayout(0) == korean, "background request changed layout");

        focusWindow(stream);
        check(scope.setActive(stream, true) && plainUS(), "recapture failed");
        check(scope.setActive(nullptr, false), "escape restore failed");
        check(GetKeyboardLayout(0) == korean, "escape did not restore original Korean layout");
        check(GetKeyboardLayoutList(0, nullptr) == layoutCount, "production scope loaded a layout");

        {
            KeyboardLayoutScope teardown;
            check(teardown.setActive(stream, true) && plainUS(), "teardown capture failed");
        }
        check(GetKeyboardLayout(0) == korean, "destructor did not restore layout");

        ActivateKeyboardLayout(us, 0);
        check(scope.setActive(stream, true), "already-US capture failed");
        scope.setActive(nullptr, false);
        check(plainUS(), "already-US capture unexpectedly restored another layout");
        std::cout << "Keyboard layout test passed: right Alt down/up, focus/escape/destructor restore, recapture, no layout installation.\n";
    }
    catch (...) {
        ActivateKeyboardLayout(original, 0);
        throw;
    }
    ActivateKeyboardLayout(original, 0);
}
