#define SDL_MAIN_HANDLED
#include "streaming/input/capslockcapture.h"
#include "streaming/input/rawkeyboardscope.h"
#include <SDL_syswm.h>
#include <iostream>
#include <stdexcept>
#include <string>

void testRawKeyboardScope(SDL_Window* stream, SDL_Window* other);
int runKeyboardPeer(const char* mode, const char* name);

struct CapsDiagnostic { WPARAM message; DWORD scan, flags; };
static CapsDiagnostic capsDiagnostics[16];
static unsigned capsDiagnosticCount = 0;
static LRESULT CALLBACK observeCaps(int code, WPARAM message, LPARAM param)
{
    if (code == HC_ACTION) {
        const auto* key = reinterpret_cast<KBDLLHOOKSTRUCT*>(param);
        if (key->vkCode == VK_CAPITAL && capsDiagnosticCount < 16)
            capsDiagnostics[capsDiagnosticCount++] = {message, key->scanCode, key->flags};
    }
    return CallNextHookEx(nullptr, code, message, param);
}

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
            SDL_KeyboardEvent key{};
            if (event.type == SDL_USEREVENT && capture.translate(event.user, key)) {
                check(key.keysym.scancode == SDL_SCANCODE_CAPSLOCK && capture.owns(key), "unexpected captured key");
                if (key.type == SDL_KEYDOWN) ++downs;
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

int main(int argc, char** argv)
{
    if (argc == 3 && (std::string(argv[1]) == "--raw-peer" || std::string(argv[1]) == "--local-peer"))
        return runKeyboardPeer(argv[1], argv[2]);
    SDL_SetMainReady();
    if (SDL_Init(SDL_INIT_VIDEO) != 0) return 1;
    auto* stream = SDL_CreateWindow("Caps Lock capture integration test", 80, 80, 360, 160, SDL_WINDOW_SHOWN);
    auto* other = SDL_CreateWindow("Local window", 480, 80, 360, 160, SDL_WINDOW_SHOWN);
    CapsLockCapture capture;
    RawKeyboardScope raw;
    HHOOK diagnosticHook = nullptr;
    int downs = 0, ups = 0;
    int status = 0;
    const int originalToggle = GetKeyState(VK_CAPITAL) & 1;
    try {
        check(stream && other, "SDL test windows unavailable");
        focus(stream, capture, downs, ups);
        check(raw.setActive(stream, true), "raw input unavailable for Caps Lock test");
        SDL_SetWindowKeyboardGrab(stream, SDL_TRUE);
        check(capture.setActive(stream, true), "hook installation failed");
        diagnosticHook = SetWindowsHookExW(WH_KEYBOARD_LL, observeCaps, GetModuleHandleW(nullptr), 0);
        sendCaps(stream, true);
        pump(capture, downs, ups);
        sendCaps(stream, true); // Repeated down must not create a second remote press.
        sendCaps(stream, false);
        pump(capture, downs, ups);
        if (downs != 1 || ups != 1) {
            std::cerr << "Caps diagnostic: downs=" << downs << " ups=" << ups
                      << " toggle=" << capture.localToggleState() << " active=" << capture.active() << '\n';
            for (unsigned i = 0; i < capsDiagnosticCount; ++i)
                std::cerr << "Caps native message=" << capsDiagnostics[i].message << " scan=" << capsDiagnostics[i].scan
                          << " flags=" << capsDiagnostics[i].flags << '\n';
        }
        check(downs == 1 && ups == 1, "expected one raw Caps Lock down/up pair");
        check(capture.localToggleState() == originalToggle, "Windows Caps Lock changed while captured");

        // The hook must pass Caps Lock through when another window is foreground,
        // even before Moonlight's focus-lost event removes the hook.
        SDL_SetWindowKeyboardGrab(stream, SDL_FALSE);
        raw.setActive(nullptr, false);
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
        testRawKeyboardScope(stream, other);
    }
    catch (const std::exception& e) {
        std::cerr << "FAIL: " << e.what() << '\n';
        status = 1;
    }
    capture.setActive(nullptr, false);
    if (diagnosticHook) UnhookWindowsHookEx(diagnosticHook);
    raw.setActive(nullptr, false);
    SDL_DestroyWindow(other);
    SDL_DestroyWindow(stream);
    SDL_Quit();
    return status;
}
