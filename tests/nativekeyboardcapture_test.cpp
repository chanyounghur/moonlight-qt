#define SDL_MAIN_HANDLED
#include "streaming/input/nativekeyboardcapture.h"
#include <SDL_syswm.h>
#include <iostream>
#include <stdexcept>
#include <string>
#include <set>
#include <tlhelp32.h>

void testNativeKeyboardScope(SDL_Window* stream, SDL_Window* other);
int runKeyboardPeer(const char* mode, const char* name);

struct CapsDiagnostic { WPARAM message; DWORD scan, flags; };
static CapsDiagnostic capsDiagnostics[16];
static unsigned capsDiagnosticCount = 0;
static bool failureSeen = false;
static std::set<DWORD> children() {
    std::set<DWORD> result;
    auto snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    PROCESSENTRY32W entry{}; entry.dwSize = sizeof(entry);
    if (snapshot != INVALID_HANDLE_VALUE && Process32FirstW(snapshot, &entry)) {
        do { if (entry.th32ParentProcessID == GetCurrentProcessId()) result.insert(entry.th32ProcessID); }
        while (Process32NextW(snapshot, &entry));
    }
    if (snapshot != INVALID_HANDLE_VALUE) CloseHandle(snapshot);
    return result;
}
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

void pump(NativeKeyboardCapture& capture, int& downs, int& ups)
{
    const auto start = SDL_GetTicks();
    do {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_USEREVENT && capture.failed(event.user)) failureSeen = true;
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

void focus(SDL_Window* window, NativeKeyboardCapture& capture, int& downs, int& ups)
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
    const auto helper = NativeKeyboardCapture::runHelper(argc, argv);
    if (helper >= 0) return helper;
    if (!GetEnvironmentVariableA("GITHUB_ACTIONS", nullptr, 0)) return 99;
    if (argc == 3 && (std::string(argv[1]) == "--raw-peer" || std::string(argv[1]) == "--local-peer"))
        return runKeyboardPeer(argv[1], argv[2]);
    SDL_SetMainReady();
    if (SDL_Init(SDL_INIT_VIDEO) != 0) return 1;
    SDL_StopTextInput();
    auto* stream = SDL_CreateWindow("Caps Lock capture integration test", 80, 80, 360, 160, SDL_WINDOW_SHOWN);
    auto* other = SDL_CreateWindow("Local window", 480, 80, 360, 160, SDL_WINDOW_SHOWN);
    NativeKeyboardCapture capture;
    HHOOK diagnosticHook = nullptr;
    int downs = 0, ups = 0;
    int status = 0;
    const int originalToggle = GetKeyState(VK_CAPITAL) & 1;
    try {
        check(stream && other, "SDL test windows unavailable");
        focus(stream, capture, downs, ups);
        check(capture.setActive(stream, true), "hook installation failed");
        diagnosticHook = SetWindowsHookExW(WH_KEYBOARD_LL, observeCaps, GetModuleHandleW(nullptr), 0);
        sendCaps(stream, true);
        pump(capture, downs, ups);
        sendCaps(stream, true); // Repeated down must not create a second remote press.
        sendCaps(stream, false);
        pump(capture, downs, ups);
        if (downs != 1 || ups != 1) {
            std::cerr << "Caps diagnostic: downs=" << downs << " ups=" << ups
                      << " toggle=" << (GetKeyState(VK_CAPITAL) & 1) << " active=" << capture.active() << '\n';
            for (unsigned i = 0; i < capsDiagnosticCount; ++i)
                std::cerr << "Caps native message=" << capsDiagnostics[i].message << " scan=" << capsDiagnostics[i].scan
                          << " flags=" << capsDiagnostics[i].flags << '\n';
        }
        check(downs == 1 && ups == 1, "expected one raw Caps Lock down/up pair");
        check((GetKeyState(VK_CAPITAL) & 1) == originalToggle, "Windows Caps Lock changed while captured");

        // The hook must pass Caps Lock through when another window is foreground,
        // even before Moonlight's focus-lost event removes the hook.
        focus(other, capture, downs, ups);
        sendCaps(other, true);
        sendCaps(other, false);
        pump(capture, downs, ups);
        check((GetKeyState(VK_CAPITAL) & 1) != originalToggle, "local Caps Lock did not work outside stream");
        check(downs == 1 && ups == 1, "captured input from another foreground window");

        capture.setActive(nullptr, false);
        sendCaps(other, true);
        sendCaps(other, false);
        pump(capture, downs, ups);
        check((GetKeyState(VK_CAPITAL) & 1) == originalToggle, "failed to restore original toggle");
        std::cout << "Native Caps Lock capture test passed: raw down/up, repeat suppression, local toggle isolation.\n";
        testNativeKeyboardScope(stream, other);

        // Normal destruction must reap the private child, and an unexpected
        // child exit must release local capture and notify the input adapter.
        HANDLE normalChild = nullptr;
        {
            NativeKeyboardCapture transient;
            const auto before = children();
            check(transient.setActive(stream, true), "lifecycle capture failed");
            for (auto pid : children()) if (!before.count(pid)) normalChild = OpenProcess(SYNCHRONIZE, FALSE, pid);
            check(normalChild != nullptr, "private helper not found");
            transient.setActive(nullptr, false);
        }
        check(WaitForSingleObject(normalChild, 2000) == WAIT_OBJECT_0, "normal teardown left helper running");
        CloseHandle(normalChild);
        {
            NativeKeyboardCapture doomed;
            const auto before = children();
            check(doomed.setActive(stream, true), "failure capture failed");
            HANDLE child = nullptr;
            for (auto pid : children()) if (!before.count(pid)) child = OpenProcess(PROCESS_TERMINATE | SYNCHRONIZE, FALSE, pid);
            check(child != nullptr, "failure test helper not found");
            check(TerminateProcess(child, 7) != 0, "cannot stop private test child");
            WaitForSingleObject(child, 2000); CloseHandle(child);
            failureSeen = false;
            pump(doomed, downs, ups);
            check(failureSeen && !doomed.active(), "helper failure did not release capture and notify adapter");
            sendCaps(stream, true); sendCaps(stream, false); pump(doomed, downs, ups);
            check((GetKeyState(VK_CAPITAL) & 1) != originalToggle, "helper failure swallowed local Caps Lock");
            sendCaps(stream, true); sendCaps(stream, false); pump(doomed, downs, ups);
        }
        std::cout << "Helper lifecycle passed: automatic teardown, unexpected exit notification, local input restored.\n";
    }
    catch (const std::exception& e) {
        std::cerr << "FAIL: " << e.what() << '\n';
        status = 1;
    }
    capture.setActive(nullptr, false);
    if (diagnosticHook) UnhookWindowsHookEx(diagnosticHook);
    SDL_DestroyWindow(other);
    SDL_DestroyWindow(stream);
    SDL_Quit();
    return status;
}
