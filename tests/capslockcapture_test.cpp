#define SDL_MAIN_HANDLED
#include "streaming/input/capslockcapture.h"
#include <SDL_syswm.h>
#include <iostream>
#include <stdexcept>

void check(bool ok, const char* message)
{
    if (!ok) throw std::runtime_error(message);
}

HWND nativeWindow(SDL_Window* window)
{
    SDL_SysWMinfo info{};
    SDL_VERSION(&info.version);
    check(SDL_GetWindowWMInfo(window, &info) == SDL_TRUE, "native window unavailable");
    return info.info.win.window;
}

void pump(CapsLockCapture& capture, int& downs, int& ups)
{
    const auto start = SDL_GetTicks();
    do {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if ((event.type == SDL_KEYDOWN || event.type == SDL_KEYUP) && capture.owns(event.key)) {
                check(event.key.keysym.scancode == SDL_SCANCODE_CAPSLOCK, "unexpected captured key");
                if (event.type == SDL_KEYDOWN) ++downs;
                else ++ups;
            }
        }
        SDL_Delay(5);
    } while (SDL_GetTicks() - start < 150);
}

void focus(SDL_Window* window, CapsLockCapture& capture, int& downs, int& ups)
{
    SDL_RaiseWindow(window);
    SetForegroundWindow(nativeWindow(window));
    pump(capture, downs, ups);
    check(GetForegroundWindow() == nativeWindow(window), "runner has no interactive foreground desktop");
}

void sendCaps(SDL_Window* window, bool down)
{
    check(GetForegroundWindow() == nativeWindow(window), "lost foreground before test input");
    INPUT input{};
    input.type = INPUT_KEYBOARD;
    input.ki.wVk = VK_CAPITAL;
    input.ki.dwFlags = down ? 0 : KEYEVENTF_KEYUP;
    check(SendInput(1, &input, sizeof(input)) == 1, "SendInput failed");
}

int main()
{
    SDL_SetMainReady();
    if (SDL_Init(SDL_INIT_VIDEO) != 0) return 1;
    auto* stream = SDL_CreateWindow("Caps Lock capture integration test", 80, 80, 360, 160, SDL_WINDOW_SHOWN);
    auto* other = SDL_CreateWindow("Local window", 480, 80, 360, 160, SDL_WINDOW_SHOWN);
    CapsLockCapture capture;
    int downs = 0, ups = 0;
    int status = 0;
    const int originalToggle = GetKeyState(VK_CAPITAL) & 1;
    try {
        check(stream && other, "SDL test windows unavailable");
        focus(stream, capture, downs, ups);
        SDL_SetWindowKeyboardGrab(stream, SDL_TRUE);
        check(capture.setActive(stream, true), "hook installation failed");
        sendCaps(stream, true);
        pump(capture, downs, ups);
        sendCaps(stream, true); // Repeated down must not create a second remote press.
        sendCaps(stream, false);
        pump(capture, downs, ups);
        check(downs == 1 && ups == 1, "expected one raw Caps Lock down/up pair");
        check(capture.localToggleState() == originalToggle, "Windows Caps Lock changed while captured");

        // The hook must pass Caps Lock through when another window is foreground,
        // even before Moonlight's focus-lost event removes the hook.
        SDL_SetWindowKeyboardGrab(stream, SDL_FALSE);
        focus(other, capture, downs, ups);
        sendCaps(other, true);
        sendCaps(other, false);
        pump(capture, downs, ups);
        check(capture.localToggleState() != originalToggle, "local Caps Lock did not work outside stream");
        check(downs == 1 && ups == 1, "captured input from another foreground window");

        capture.setActive(nullptr, false);
        sendCaps(other, true);
        sendCaps(other, false);
        pump(capture, downs, ups);
        check(capture.localToggleState() == originalToggle, "failed to restore original toggle");
        std::cout << "Native Caps Lock capture test passed: raw down/up, repeat suppression, local toggle isolation.\n";
    }
    catch (const std::exception& e) {
        std::cerr << "FAIL: " << e.what() << '\n';
        status = 1;
    }
    capture.setActive(nullptr, false);
    SDL_DestroyWindow(other);
    SDL_DestroyWindow(stream);
    SDL_Quit();
    return status;
}
