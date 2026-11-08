#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <SDL_syswm.h>
#include <windows.h>
#include <iostream>
#include <string>

struct State { volatile LONG ready, done; HWND target; LONG caps, alt, win, c, rawCaps, rawAlt, rawWin, rawC; };
static State* shared;
static bool combined;
static LRESULT CALLBACK hook(int code, WPARAM msg, LPARAM data) {
    if (code == HC_ACTION && GetForegroundWindow() == shared->target) {
        if (combined) {
            MSG queued;
            while (PeekMessageW(&queued, nullptr, WM_INPUT, WM_INPUT, PM_REMOVE)) DispatchMessageW(&queued);
        }
        auto key = reinterpret_cast<KBDLLHOOKSTRUCT*>(data);
        if (combined && (key->vkCode == VK_HANGUL || key->vkCode == VK_RMENU)) {
            std::cout << "PASS rightAlt vk=" << key->vkCode << " scan=" << key->scanCode << " msg=" << msg << std::endl;
            return CallNextHookEx(nullptr,code,msg,data);
        }
        if (combined) std::cout << "HOOK scan=" << key->scanCode << " msg=" << msg << std::endl;
        if (key->vkCode == VK_CAPITAL) { ++shared->caps; return 1; }
        if (key->vkCode == VK_LMENU || key->vkCode == VK_RMENU) { ++shared->alt; return 1; }
        if (key->vkCode == VK_LWIN || key->vkCode == VK_RWIN) { ++shared->win; return 1; }
        if (key->vkCode == 'C') { ++shared->c; return 1; }
    }
    return CallNextHookEx(nullptr, code, msg, data);
}
static LRESULT CALLBACK wnd(HWND window, UINT msg, WPARAM w, LPARAM l) {
    if (msg == WM_INPUT) {
        RAWINPUT input{}; UINT size = sizeof(input);
        if (GetRawInputData(reinterpret_cast<HRAWINPUT>(l), RID_INPUT, &input, &size, sizeof(RAWINPUTHEADER)) != UINT(-1) && input.header.dwType == RIM_TYPEKEYBOARD) {
            if (combined) std::cout << "RAW vk=" << input.data.keyboard.VKey << " scan=" << input.data.keyboard.MakeCode << " flags=" << input.data.keyboard.Flags << std::endl;
            switch (input.data.keyboard.MakeCode) {
            case 0x3a: ++shared->rawCaps; break;
            case 0x38: ++shared->rawAlt; break;
            case 0x5b: ++shared->rawWin; break;
            case 0x2e: ++shared->rawC; break;
            }
        }
    }
    return DefWindowProcW(window, msg, w, l);
}
static void pump() {
    auto start = SDL_GetTicks();
    do {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP)
                std::cout << "SDL " << event.key.keysym.scancode << " " << int(event.key.state) << '\n';
        }
        SDL_Delay(5);
    } while (SDL_GetTicks() - start < 150);
}
int main(int argc, char** argv) {
    if (!GetEnvironmentVariableA("GITHUB_ACTIONS", nullptr, 0)) return 99;
    if (argc == 3) {
        auto map = OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, argv[2]);
        shared = static_cast<State*>(MapViewOfFile(map, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(State)));
        if (!shared) return 2;
        HHOOK installed = nullptr;
        combined = std::string(argv[1]) == "--both";
        if (std::string(argv[1]) == "--hook" || combined) installed = SetWindowsHookExW(WH_KEYBOARD_LL, hook, GetModuleHandleW(nullptr), 0);
        if (std::string(argv[1]) != "--hook") {
            WNDCLASSW cls{}; cls.lpfnWndProc = wnd; cls.hInstance = GetModuleHandleW(nullptr); cls.lpszClassName = L"RawProbe";
            RegisterClassW(&cls);
            auto window = CreateWindowW(cls.lpszClassName, L"", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, cls.hInstance, nullptr);
            RAWINPUTDEVICE device{1,6,RIDEV_INPUTSINK,window};
            if (!RegisterRawInputDevices(&device, 1, sizeof(device))) return 3;
        }
        InterlockedExchange(&shared->ready, 1);
        while (!shared->done) {
            MSG msg;
            while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
            Sleep(1);
        }
        if (installed) UnhookWindowsHookEx(installed);
        return 0;
    }
    SDL_SetMainReady(); SDL_Init(SDL_INIT_VIDEO);
    auto window = SDL_CreateWindow("Input process probe", 100,100,300,150,SDL_WINDOW_SHOWN);
    SDL_SysWMinfo info{}; SDL_VERSION(&info.version); SDL_GetWindowWMInfo(window,&info);
    const std::string name = "Local\\KeyboardProbe-" + std::to_string(GetCurrentProcessId());
    auto map = CreateFileMappingA(INVALID_HANDLE_VALUE,nullptr,PAGE_READWRITE,0,sizeof(State),name.c_str());
    shared = static_cast<State*>(MapViewOfFile(map,FILE_MAP_ALL_ACCESS,0,0,sizeof(State)));
    char exe[MAX_PATH]; GetModuleFileNameA(nullptr,exe,MAX_PATH);
    for (int mode = 0; mode < 3; ++mode) {
        *shared = {}; shared->target = info.info.win.window;
        auto cmd = std::string("\"") + exe + (mode == 0 ? "\" --hook " : mode == 1 ? "\" --raw " : "\" --both ") + name;
        STARTUPINFOA si{}; si.cb=sizeof(si); PROCESS_INFORMATION pi{};
        if (!CreateProcessA(nullptr,cmd.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,nullptr,&si,&pi)) return 4;
        while (!shared->ready) Sleep(5);
        SetForegroundWindow(shared->target); pump();
        if (mode == 2) { LoadKeyboardLayoutW(L"00000412",KLF_ACTIVATE); pump(); }
        SDL_SetHintWithPriority("SDL_WINDOWS_RAW_KEYBOARD_INPUTSINK","0",SDL_HINT_OVERRIDE);
        SDL_SetHintWithPriority("SDL_WINDOWS_RAW_KEYBOARD_EXCLUDE_HOTKEYS","0",SDL_HINT_OVERRIDE);
        SDL_SetHintWithPriority("SDL_WINDOWS_RAW_KEYBOARD",mode == 0 ? "1" : "0",SDL_HINT_OVERRIDE);
        auto mainHook = mode == 1 ? SetWindowsHookExW(WH_KEYBOARD_LL, hook, GetModuleHandleW(nullptr), 0) : nullptr;
        std::cout << "MODE " << mode << " before toggle=" << (GetKeyState(VK_CAPITAL)&1) << '\n';
        for (WORD scan : {WORD(0x3a),WORD(0x38),WORD(0x5b),WORD(0x2e)}) {
            for (bool down : {true,false}) {
                INPUT in{}; in.type=INPUT_KEYBOARD; in.ki.wScan=scan;
                in.ki.dwFlags=KEYEVENTF_SCANCODE | (scan==0x5b || (mode==2 && scan==0x38)?KEYEVENTF_EXTENDEDKEY:0) | (down?0:KEYEVENTF_KEYUP);
                SendInput(1,&in,sizeof(in)); pump();
            }
        }
        std::cout << "MODE " << mode << " hook Caps/Alt/Win/C=" << shared->caps << '/' << shared->alt << '/' << shared->win << '/' << shared->c
                  << " raw=" << shared->rawCaps << '/' << shared->rawAlt << '/' << shared->rawWin << '/' << shared->rawC << " toggle=" << (GetKeyState(VK_CAPITAL)&1) << '\n';
        if (mainHook) UnhookWindowsHookEx(mainHook);
        SDL_SetHintWithPriority("SDL_WINDOWS_RAW_KEYBOARD","0",SDL_HINT_OVERRIDE);
        InterlockedExchange(&shared->done,1);
        WaitForSingleObject(pi.hProcess,5000); CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
    }
    SDL_Quit();
}
