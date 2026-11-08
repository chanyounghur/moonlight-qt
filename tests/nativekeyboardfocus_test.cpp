#define SDL_MAIN_HANDLED
#include "streaming/input/nativekeyboardcapture.h"
#include <SDL_syswm.h>
#include <iostream>
#include <string>
#include <stdexcept>
#include <cstdint>
#include <vector>

void check(bool ok, const char* message);
HWND nativeWindow(SDL_Window* window);

namespace {
struct ProbeState {
    HWND window;
    HKL layout;
    volatile LONG ready, done;
    volatile LONG altDown, altUp, ctrlDown, ctrlUp, cDown, cUp;
};

NativeKeyboardCapture* recordingScope = nullptr;
std::vector<unsigned> sequence;
void record(const SDL_Event& source, ProbeState& state)
{
    SDL_Event event = source;
    if (recordingScope) {
        SDL_KeyboardEvent key{};
        if (event.type != SDL_USEREVENT || !recordingScope->translate(event.user, key)) return;
        event.key = key;
    }
    if (event.type != SDL_KEYDOWN && event.type != SDL_KEYUP) return;
    const bool down = event.type == SDL_KEYDOWN;
    sequence.push_back(static_cast<unsigned>(event.key.keysym.scancode) | (down ? 1024 : 0));
    switch (event.key.keysym.scancode) {
    case SDL_SCANCODE_RALT: InterlockedIncrement(down ? &state.altDown : &state.altUp); break;
    case SDL_SCANCODE_LCTRL: InterlockedIncrement(down ? &state.ctrlDown : &state.ctrlUp); break;
    case SDL_SCANCODE_C: InterlockedIncrement(down ? &state.cDown : &state.cUp); break;
    default: break;
    }
}

void pump(ProbeState* state = nullptr, Uint32 duration = 150)
{
    const auto start = SDL_GetTicks();
    do {
        SDL_Event event;
        while (SDL_PollEvent(&event)) if (state) record(event, *state);
        SDL_Delay(5);
    } while (SDL_GetTicks() - start < duration);
}

void sendScan(HWND window, WORD scan, bool extended, bool down)
{
    check(GetForegroundWindow() == window, "lost foreground before raw test input");
    INPUT input{};
    input.type = INPUT_KEYBOARD;
    input.ki.wScan = scan;
    input.ki.dwFlags = KEYEVENTF_SCANCODE | (extended ? KEYEVENTF_EXTENDEDKEY : 0) | (down ? 0 : KEYEVENTF_KEYUP);
    check(SendInput(1, &input, sizeof(input)) == 1, "scan-code SendInput failed");
}

struct Peer {
    HANDLE mapping = nullptr;
    ProbeState* state = nullptr;
    PROCESS_INFORMATION process{};
    Peer(const char* mode, unsigned index) {
        const auto name = "Local\\MoonlightRawTest-" + std::to_string(GetCurrentProcessId()) + "-" + std::to_string(index);
        mapping = CreateFileMappingA(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, sizeof(ProbeState), name.c_str());
        check(mapping != nullptr, "probe mapping failed");
        state = static_cast<ProbeState*>(MapViewOfFile(mapping, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(ProbeState)));
        check(state != nullptr, "probe mapping view failed");
        *state = {};
        char exe[MAX_PATH];
        check(GetModuleFileNameA(nullptr, exe, MAX_PATH) != 0, "test path unavailable");
        auto command = std::string("\"") + exe + "\" " + mode + " " + name;
        STARTUPINFOA startup{};
        startup.cb = sizeof(startup);
        check(CreateProcessA(nullptr, command.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW,
                             nullptr, nullptr, &startup, &process), "external probe launch failed");
        AllowSetForegroundWindow(process.dwProcessId);
        const auto started = SDL_GetTicks();
        while (!state->ready && SDL_GetTicks() - started < 10000) pump(nullptr, 10);
        check(state->ready && state->window, "external probe failed to initialize");
    }
    ~Peer() {
        if (state) InterlockedExchange(&state->done, 1);
        if (process.hProcess) { WaitForSingleObject(process.hProcess, 5000); CloseHandle(process.hProcess); }
        if (process.hThread) CloseHandle(process.hThread);
        if (state) UnmapViewOfFile(state);
        if (mapping) CloseHandle(mapping);
    }
};

void foreground(HWND window)
{
    SetForegroundWindow(window);
    pump();
    check(GetForegroundWindow() == window, "test foreground transition failed");
}
}

int runKeyboardPeer(const char* mode, const char* name)
{
    const bool raw = std::string(mode) == "--raw-peer";
    auto mapping = OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, name);
    if (!mapping) return 2;
    auto* state = static_cast<ProbeState*>(MapViewOfFile(mapping, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(ProbeState)));
    if (!state) return 3;
    SDL_SetMainReady();
    if (SDL_Init(SDL_INIT_VIDEO) != 0) return 4;
    auto* window = SDL_CreateWindow(raw ? "Second raw stream" : "Other Windows app", 550, raw ? 300 : 500, 350, 160, SDL_WINDOW_SHOWN);
    if (!window) return 5;
    // Test fixture only; production must never load or activate an OS layout.
    auto korean = LoadKeyboardLayoutW(L"00000412", KLF_ACTIVATE);
    if (!korean || !ActivateKeyboardLayout(korean, 0)) return 6;
    NativeKeyboardCapture scope;
    recordingScope = raw ? &scope : nullptr;
    SDL_StopTextInput();
    state->window = nativeWindow(window);
    state->layout = GetKeyboardLayout(0);
    InterlockedExchange(&state->ready, 1);
    while (!state->done) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (raw && event.type == SDL_WINDOWEVENT) {
                if (event.window.event == SDL_WINDOWEVENT_FOCUS_GAINED) {
                    scope.setActive(window, true);
                } else if (event.window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
                    scope.setActive(nullptr, false);
                }
            }
            record(event, *state);
        }
        state->layout = GetKeyboardLayout(0);
        SDL_Delay(5);
    }
    scope.setActive(nullptr, false);
    SDL_DestroyWindow(window);
    SDL_Quit();
    UnmapViewOfFile(state);
    CloseHandle(mapping);
    return 0;
}

void testNativeKeyboardScope(SDL_Window* stream, SDL_Window* other)
{
    (void)other;
    const auto original = GetKeyboardLayout(0);
    foreground(nativeWindow(stream));
    const auto korean = LoadKeyboardLayoutW(L"00000412", KLF_ACTIVATE);
    check(korean && ActivateKeyboardLayout(korean, 0), "Korean test fixture unavailable");
    pump();
    NativeKeyboardCapture scope;
    recordingScope = &scope;
    try {
        Peer second("--raw-peer", 1);
        Peer local("--local-peer", 2);
        foreground(nativeWindow(stream));
        ActivateKeyboardLayout(korean, 0); // Fixture, before capture starts.
        pump();
        const auto layoutCount = GetKeyboardLayoutList(0, nullptr);
        check(scope.setActive(stream, true), "raw capture activation failed");
        pump();
        check(GetKeyboardLayout(0) == korean, "raw capture changed Korean keyboard layout");
        ProbeState received{};
        sendScan(nativeWindow(stream), 0x38, true, true);
        pump(&received);
        sendScan(nativeWindow(stream), 0x38, true, false);
        pump(&received);
        check(received.altDown == 1 && received.altUp == 1, "raw right Alt needs one down AND up under Korean layout");

        // No sleeps between events: a raw right-Alt release must precede the
        // following intercepted letter, even when sent in one Windows batch.
        sequence.clear();
        std::vector<INPUT> burst;
        std::vector<unsigned> expected;
        for (int repeat = 0; repeat < 100; ++repeat) {
            const auto modifier = repeat & 1 ? SDL_SCANCODE_RCTRL : SDL_SCANCODE_RALT;
            for (const auto code : {unsigned(modifier)|1024, unsigned(SDL_SCANCODE_C)|1024,
                                   unsigned(SDL_SCANCODE_C), unsigned(modifier),
                                   unsigned(SDL_SCANCODE_C)|1024, unsigned(SDL_SCANCODE_C)}) {
                const bool modifierKey = (code & 1023) == unsigned(modifier);
                INPUT input{}; input.type = INPUT_KEYBOARD;
                input.ki.wScan = modifierKey ? (modifier == SDL_SCANCODE_RALT ? 0x38 : 0x1d) : 0x2e;
                input.ki.dwFlags = KEYEVENTF_SCANCODE | (modifierKey ? KEYEVENTF_EXTENDEDKEY : 0) | (code & 1024 ? 0 : KEYEVENTF_KEYUP);
                burst.push_back(input); expected.push_back(code);
            }
        }
        check(SendInput(static_cast<UINT>(burst.size()), burst.data(), sizeof(INPUT)) == burst.size(), "burst SendInput failed");
        pump(&received, 1000);
        if (sequence != expected) {
            std::cerr << "Ordered keyboard burst got " << sequence.size() << " expected " << expected.size() << '\n';
            for (size_t i = 0; i < sequence.size() && i < expected.size(); ++i) {
                if (sequence[i] != expected[i]) { std::cerr << "First mismatch " << i << ": " << sequence[i] << " != " << expected[i] << '\n'; break; }
            }
        }
        check(sequence == expected, "right Alt/Ctrl and ordinary keys were lost, duplicated, or reordered");

        sequence.clear();
        INPUT pause{}; pause.type = INPUT_KEYBOARD; pause.ki.wVk = VK_PAUSE;
        check(SendInput(1, &pause, sizeof(pause)) == 1, "Pause injection failed");
        sendScan(nativeWindow(stream), 0x2e, false, true);
        sendScan(nativeWindow(stream), 0x2e, false, false);
        pump(&received);
        check(sequence == std::vector<unsigned>{unsigned(SDL_SCANCODE_PAUSE)|1024, unsigned(SDL_SCANCODE_PAUSE),
                                                unsigned(SDL_SCANCODE_C)|1024, unsigned(SDL_SCANCODE_C)},
              "make-only Pause remained held or reordered the following key");

        // Windows shortcuts must remain in the stream, including Alt+Tab.
        sendScan(nativeWindow(stream), 0x38, false, true);
        sendScan(nativeWindow(stream), 0x0f, false, true);
        sendScan(nativeWindow(stream), 0x0f, false, false);
        sendScan(nativeWindow(stream), 0x38, false, false);
        sendScan(nativeWindow(stream), 0x5b, true, true);
        sendScan(nativeWindow(stream), 0x5b, true, false);
        pump(&received);
        check(GetForegroundWindow() == nativeWindow(stream), "Alt+Tab or Windows key escaped capture");

        // Leave capture before transferring focus, as an explicit escape would.
        scope.setActive(nullptr, false);
        foreground(second.state->window);
        check(second.state->layout == korean, "second stream lost Korean layout");
        sendScan(second.state->window, 0x38, true, true);
        pump();
        sendScan(second.state->window, 0x38, true, false);
        pump();
        check(second.state->altDown == 1 && second.state->altUp == 1, "second stream raw right Alt failed");

        // The second process handles actual focus loss while the local app gains focus.
        foreground(local.state->window);
        check(local.state->layout == korean, "other Windows app lost Korean layout");
        sendScan(local.state->window, 0x1D, false, true);
        pump();
        sendScan(local.state->window, 0x2E, false, true);
        sendScan(local.state->window, 0x2E, false, false);
        sendScan(local.state->window, 0x1D, false, false);
        pump();
        check(local.state->ctrlDown == 1 && local.state->ctrlUp == 1 && local.state->cDown == 1 && local.state->cUp == 1,
              "background streams swallowed local Ctrl+C");
        check(second.state->ctrlDown == 0 && second.state->cDown == 0, "inactive stream received local keys");
        check(!scope.setActive(stream, true), "background stream activated raw capture");
        check(GetKeyboardLayout(0) == korean && second.state->layout == korean && local.state->layout == korean,
              "cross-process focus cycle changed a keyboard layout");
        check(GetKeyboardLayoutList(0, nullptr) == layoutCount, "raw capture installed an OS layout");
        foreground(nativeWindow(stream));
        check(scope.setActive(stream, true), "raw recapture failed");
        scope.setActive(nullptr, false);
        check(GetKeyboardLayout(0) == korean, "recapture/escape changed layout");
        std::cout << "Raw keyboard test passed: Korean right Alt down/up, two stream processes, independent Windows app, foreground isolation, no OS layout changes.\n";
    } catch (...) {
        scope.setActive(nullptr, false);
        ActivateKeyboardLayout(original, 0);
        throw;
    }
    ActivateKeyboardLayout(original, 0);
}
