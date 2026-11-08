#include "nativekeyboardcapture.h"
#include <cstdint>
#include <string>
#include <cstdlib>
#ifdef _WIN32
#include <SDL_syswm.h>
#include <array>
#include <atomic>
#include <string>
#include <thread>
#include <vector>
#include "windows_scancodes.h"

namespace {
constexpr LONG Capacity = 1024;
constexpr unsigned Down = 1u << 10;
constexpr unsigned Fault = 1u << 11;
volatile LONG nextGeneration = 0;
struct Packet { Uint32 generation, windowId, key; };
struct Shared {
    volatile LONG enabled, generation, ready, fault, head, tail, heartbeat;
    DWORD parentId;
    HWND target;
    Uint32 windowId;
    bool blockCaps;
    Packet packets[Capacity];
};
LONG read(volatile LONG& value) { return InterlockedCompareExchange(&value, 0, 0); }
void close(HANDLE& handle) { if (handle) CloseHandle(handle); handle = nullptr; }
SDL_Scancode scan(DWORD code, DWORD vk, bool extended) {
    if (vk == VK_PAUSE) return SDL_SCANCODE_PAUSE;
    if (vk == VK_SNAPSHOT) return SDL_SCANCODE_PRINTSCREEN;
    if (!code) {
        const auto mapped = MapVirtualKeyW(vk, MAPVK_VK_TO_VSC_EX);
        code = mapped & 0xff;
        extended = extended || (mapped & 0xff00) == 0xe000;
    }
    if (code > 0x7f) return SDL_SCANCODE_UNKNOWN;
    return windows_scancode_table[code | (extended ? 0x80 : 0)];
}
struct Helper {
    Shared* shared;
    HANDLE notification;
    LONG generation = 0;
    std::array<bool, SDL_NUM_SCANCODES> pressed{};
    std::array<bool, 256> inherited{};
    HWND rawWindow = nullptr;
    static Helper* current;

    bool foreground() const {
        DWORD owner = 0;
        return read(shared->enabled) && GetForegroundWindow() == shared->target &&
            GetWindowThreadProcessId(shared->target, &owner) && owner == shared->parentId;
    }
    void synchronize() {
        const auto epoch = read(shared->generation);
        if (generation == epoch) return;
        generation = epoch;
        pressed.fill(false);
        for (unsigned i = 0; i < inherited.size(); ++i)
            inherited[i] = (GetAsyncKeyState(static_cast<int>(i)) & 0x8000) != 0;
    }
    bool publish(SDL_Scancode key, bool down) {
        if (key == SDL_SCANCODE_UNKNOWN || key >= SDL_NUM_SCANCODES) return false;
        if (pressed[key] == down) return true;
        const auto head = read(shared->head);
        const auto next = (head + 1) % Capacity;
        if (next == read(shared->tail)) {
            InterlockedExchange(&shared->fault, 1);
            InterlockedExchange(&shared->enabled, 0);
            SetEvent(notification);
            return false;
        }
        shared->packets[head] = {static_cast<Uint32>(generation), shared->windowId,
                                 static_cast<Uint32>(key) | (down ? Down : 0)};
        InterlockedExchange(&shared->head, next);
        pressed[key] = down;
        SetEvent(notification);
        return true;
    }
    void drainRaw() {
        // Sent hook callbacks can precede queued WM_INPUT. Drain earlier raw
        // right-modifier transitions before appending the next intercepted key.
        MSG msg;
        while (PeekMessageW(&msg, rawWindow, WM_INPUT, WM_INPUT, PM_REMOVE | PM_QS_INPUT))
            DispatchMessageW(&msg);
    }
    static LRESULT CALLBACK hook(int code, WPARAM message, LPARAM data) {
        auto& self = *current;
        if (code != HC_ACTION || !self.foreground()) return CallNextHookEx(nullptr, code, message, data);
        self.synchronize();
        self.drainRaw();
        const auto& key = *reinterpret_cast<KBDLLHOOKSTRUCT*>(data);
        const bool down = message == WM_KEYDOWN || message == WM_SYSKEYDOWN;
        // SDL uses this same exclusion for Windows' synthetic AltGr Control.
        if (key.scanCode == 0x21d) return 1;
        if (key.vkCode < 256 && self.inherited[key.vkCode]) {
            if (!down) self.inherited[key.vkCode] = false;
            return CallNextHookEx(nullptr, code, message, data);
        }
        // Korean keyboard translation can collapse right Alt/Ctrl into one-shot
        // VK_HANGUL/VK_HANJA. Raw input supplies their actual physical breaks.
        // The foreground stream has text input disabled; ordinary SDL duplicates
        // are ignored by the adapter. All shortcut partner keys are intercepted.
        if (key.vkCode == VK_HANGUL || key.vkCode == VK_RMENU ||
            key.vkCode == VK_HANJA || key.vkCode == VK_RCONTROL)
            return CallNextHookEx(nullptr, code, message, data);
        const auto mapped = scan(key.scanCode, key.vkCode, (key.flags & LLKHF_EXTENDED) != 0);
        if (!self.publish(mapped, down)) return CallNextHookEx(nullptr, code, message, data);
        // Pause is a make-only key on physical keyboards.
        if (key.vkCode == VK_PAUSE && down) self.publish(mapped, false);
        if (key.vkCode == VK_CAPITAL && !self.shared->blockCaps)
            return CallNextHookEx(nullptr, code, message, data);
        return 1;
    }
    static LRESULT CALLBACK windowProc(HWND window, UINT message, WPARAM w, LPARAM l) {
        if (message == WM_INPUT && current && current->foreground()) {
            auto& self = *current;
            self.synchronize();
            RAWINPUT input{}; UINT size = sizeof(input);
            if (GetRawInputData(reinterpret_cast<HRAWINPUT>(l), RID_INPUT, &input, &size,
                                sizeof(RAWINPUTHEADER)) != UINT(-1) && input.header.dwType == RIM_TYPEKEYBOARD) {
                const auto& key = input.data.keyboard;
                if ((key.MakeCode == 0x38 || key.MakeCode == 0x1d) && (key.Flags & RI_KEY_E0)) {
                    const bool down = !(key.Flags & RI_KEY_BREAK);
                    const auto vk = key.MakeCode == 0x38 ? VK_RMENU : VK_RCONTROL;
                    const auto mapped = key.MakeCode == 0x38 ? SDL_SCANCODE_RALT : SDL_SCANCODE_RCTRL;
                    if (!self.inherited[vk]) self.publish(mapped, down);
                    else if (!down) self.inherited[vk] = false;
                }
            }
        }
        return DefWindowProcW(window, message, w, l);
    }
};
Helper* Helper::current = nullptr;
}

struct NativeKeyboardCapture::Impl {
    HANDLE mapping = nullptr, notification = nullptr, stop = nullptr, parent = nullptr, process = nullptr;
    Shared* shared = nullptr;
    std::thread receiver;
    Uint32 generation = 0;
    std::atomic<bool> broken{false};
    ~Impl() {
        if (shared) InterlockedExchange(&shared->enabled, 0);
        if (stop) SetEvent(stop);
        if (receiver.joinable()) receiver.join();
        if (process && WaitForSingleObject(process, 2000) == WAIT_TIMEOUT) {
            TerminateProcess(process, 1); // Only our private child, already disabled.
            WaitForSingleObject(process, 1000);
        }
        if (shared) UnmapViewOfFile(shared);
        close(process); close(parent); close(stop); close(notification); close(mapping);
    }
    bool event(const Packet& packet) {
        SDL_Event event{};
        event.type = SDL_USEREVENT;
        event.user.code = EventCode;
        event.user.windowID = packet.windowId;
        event.user.data1 = reinterpret_cast<void*>(static_cast<uintptr_t>(packet.generation));
        event.user.data2 = reinterpret_cast<void*>(static_cast<uintptr_t>(packet.key));
        if (SDL_PushEvent(&event) != 1) {
            broken = true;
            InterlockedExchange(&shared->fault, 1);
            InterlockedExchange(&shared->enabled, 0);
            return false;
        }
        return true;
    }
    bool start() {
        SECURITY_ATTRIBUTES security{sizeof(security), nullptr, TRUE};
        mapping = CreateFileMappingW(INVALID_HANDLE_VALUE, &security, PAGE_READWRITE, 0, sizeof(Shared), nullptr);
        notification = CreateEventW(&security, FALSE, FALSE, nullptr);
        stop = CreateEventW(&security, TRUE, FALSE, nullptr);
        if (!mapping || !notification || !stop ||
            !DuplicateHandle(GetCurrentProcess(), GetCurrentProcess(), GetCurrentProcess(), &parent,
                             SYNCHRONIZE, TRUE, 0)) return false;
        shared = static_cast<Shared*>(MapViewOfFile(mapping, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(Shared)));
        if (!shared) return false;
        *shared = {};
        shared->parentId = GetCurrentProcessId();
        std::array<HANDLE,4> handles{mapping, notification, stop, parent};
        SIZE_T size = 0;
        InitializeProcThreadAttributeList(nullptr, 1, 0, &size);
        std::vector<unsigned char> attributes(size);
        STARTUPINFOEXW startup{}; startup.StartupInfo.cb = sizeof(startup);
        startup.lpAttributeList = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(attributes.data());
        if (!InitializeProcThreadAttributeList(startup.lpAttributeList, 1, 0, &size)) return false;
        const bool listed = UpdateProcThreadAttribute(startup.lpAttributeList, 0,
                PROC_THREAD_ATTRIBUTE_HANDLE_LIST, handles.data(), sizeof(handles), nullptr, nullptr) != FALSE;
        wchar_t executable[32768];
        const auto length = GetModuleFileNameW(nullptr, executable, 32768);
        std::wstring command = L"\"" + std::wstring(executable, length) + L"\" --moonlight-keyboard-helper";
        for (auto handle : handles) command += L" " + std::to_wstring(reinterpret_cast<uintptr_t>(handle));
        PROCESS_INFORMATION child{};
        const bool created = listed && length && CreateProcessW(executable, command.data(), nullptr, nullptr, TRUE,
            CREATE_NO_WINDOW | EXTENDED_STARTUPINFO_PRESENT, nullptr, nullptr, &startup.StartupInfo, &child);
        DeleteProcThreadAttributeList(startup.lpAttributeList);
        if (!created) return false;
        process = child.hProcess; CloseHandle(child.hThread);
        HANDLE initial[]{process, notification};
        if (WaitForMultipleObjects(2, initial, FALSE, 3000) != WAIT_OBJECT_0 + 1 || read(shared->ready) != 1) return false;
        receiver = std::thread([this] {
            HANDLE wait[]{stop, process, notification};
            while (true) {
                const auto result = WaitForMultipleObjects(3, wait, FALSE, 100);
                if (result == WAIT_OBJECT_0) break;
                if (result == WAIT_OBJECT_0 + 1 || read(shared->fault) ||
                    static_cast<DWORD>(GetTickCount() - static_cast<DWORD>(read(shared->heartbeat))) > 1500) {
                    broken = true;
                    InterlockedExchange(&shared->enabled, 0);
                    // If SDL's queue filled, retry the release notification
                    // after it drains. Never leave remote modifiers held merely
                    // because the first failure notification could not fit.
                    const Packet failure{static_cast<Uint32>(read(shared->generation)), shared->windowId, Fault};
                    while (!event(failure) && WaitForSingleObject(stop, 50) == WAIT_TIMEOUT) {}
                    break;
                }
                auto tail = read(shared->tail);
                while (tail != read(shared->head)) {
                    const auto packet = shared->packets[tail];
                    tail = (tail + 1) % Capacity;
                    InterlockedExchange(&shared->tail, tail);
                    if (!event(packet)) break;
                }
            }
        });
        return true;
    }
};
#else
struct NativeKeyboardCapture::Impl {};
#endif

NativeKeyboardCapture::NativeKeyboardCapture() : m_Impl(std::make_unique<Impl>()) {}
NativeKeyboardCapture::~NativeKeyboardCapture() = default;
bool NativeKeyboardCapture::hasForeground(SDL_Window* window) {
    if (!window || !(SDL_GetWindowFlags(window) & SDL_WINDOW_INPUT_FOCUS)) return false;
#ifdef _WIN32
    SDL_SysWMinfo info{}; SDL_VERSION(&info.version);
    return SDL_GetWindowWMInfo(window, &info) && GetForegroundWindow() == info.info.win.window;
#else
    return true;
#endif
}
bool NativeKeyboardCapture::setActive(SDL_Window* window, bool enabled, bool blockCaps) {
#ifdef _WIN32
    auto& self = *m_Impl;
    if (!enabled) {
        if (self.shared) { InterlockedExchange(&self.shared->enabled, 0); InterlockedExchange(&self.shared->generation, InterlockedIncrement(&nextGeneration)); }
        self.generation = 0;
        return true;
    }
    if (!hasForeground(window) || self.broken) return false;
    SDL_SysWMinfo info{}; SDL_VERSION(&info.version);
    if (!SDL_GetWindowWMInfo(window, &info)) return false;
    if (active() && self.shared->target == info.info.win.window) return true;
    if (!self.process && !self.start()) { self.broken = true; return false; }
    InterlockedExchange(&self.shared->enabled, 0);
    self.shared->target = info.info.win.window;
    self.shared->windowId = SDL_GetWindowID(window);
    self.shared->blockCaps = blockCaps;
    self.generation = static_cast<Uint32>(InterlockedIncrement(&nextGeneration));
    InterlockedExchange(&self.shared->generation, static_cast<LONG>(self.generation));
    InterlockedExchange(&self.shared->enabled, 1);
    return true;
#else
    (void)window; (void)blockCaps;
    return !enabled;
#endif
}
bool NativeKeyboardCapture::active() const {
#ifdef _WIN32
    return m_Impl->generation && m_Impl->shared && !m_Impl->broken && read(m_Impl->shared->enabled);
#else
    return false;
#endif
}
int NativeKeyboardCapture::localToggleState() const {
#ifdef _WIN32
    return GetKeyState(VK_CAPITAL) & 1;
#else
    return -1;
#endif
}
bool NativeKeyboardCapture::owns(const SDL_KeyboardEvent& event) const {
#ifdef _WIN32
    return m_Impl->generation && event.keysym.unused == m_Impl->generation;
#else
    (void)event; return false;
#endif
}
bool NativeKeyboardCapture::failed(const SDL_UserEvent& event) const {
#ifdef _WIN32
    return m_Impl->generation && event.code == EventCode &&
        reinterpret_cast<uintptr_t>(event.data1) == m_Impl->generation &&
        (reinterpret_cast<uintptr_t>(event.data2) & Fault);
#else
    (void)event; return false;
#endif
}
bool NativeKeyboardCapture::translate(const SDL_UserEvent& event, SDL_KeyboardEvent& key) const {
#ifdef _WIN32
    if (!active() || event.code != EventCode || reinterpret_cast<uintptr_t>(event.data1) != m_Impl->generation) return false;
    const auto data = static_cast<unsigned>(reinterpret_cast<uintptr_t>(event.data2));
    const auto code = data & 0x3ff;
    if (data & Fault || code == 0 || code >= SDL_NUM_SCANCODES) return false;
    key = {};
    key.type = data & Down ? SDL_KEYDOWN : SDL_KEYUP;
    key.state = data & Down ? SDL_PRESSED : SDL_RELEASED;
    key.windowID = event.windowID;
    key.keysym.scancode = static_cast<SDL_Scancode>(code);
    key.keysym.sym = SDL_GetKeyFromScancode(key.keysym.scancode);
    key.keysym.unused = m_Impl->generation;
    return true;
#else
    (void)event; (void)key; return false;
#endif
}
int NativeKeyboardCapture::runHelper(int argc, char** argv) {
    if (argc < 2 || std::string(argv[1]) != "--moonlight-keyboard-helper") return -1;
#ifdef _WIN32
    if (argc != 6) return 2;
    std::array<HANDLE,4> handles{};
    for (unsigned i = 0; i < handles.size(); ++i) {
        char* end = nullptr;
        const auto number = std::strtoull(argv[i+2], &end, 10);
        if (!number || !end || *end) return 2;
        handles[i] = reinterpret_cast<HANDLE>(static_cast<uintptr_t>(number));
    }
    auto* shared = static_cast<Shared*>(MapViewOfFile(handles[0], FILE_MAP_ALL_ACCESS, 0, 0, sizeof(Shared)));
    if (!shared || !shared->parentId || WaitForSingleObject(handles[3], 0) != WAIT_TIMEOUT) return 3;
    Helper helper{shared, handles[1]}; Helper::current = &helper;
    WNDCLASSW cls{}; cls.lpfnWndProc = Helper::windowProc; cls.hInstance = GetModuleHandleW(nullptr);
    cls.lpszClassName = L"MoonlightKeyboardInput";
    if (!RegisterClassW(&cls)) return 4;
    helper.rawWindow = CreateWindowW(cls.lpszClassName, L"", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, cls.hInstance, nullptr);
    RAWINPUTDEVICE device{1, 6, RIDEV_INPUTSINK, helper.rawWindow};
    if (!helper.rawWindow || !RegisterRawInputDevices(&device, 1, sizeof(device))) return 5;
    const auto hook = SetWindowsHookExW(WH_KEYBOARD_LL, Helper::hook, cls.hInstance, 0);
    if (!hook) return 6;
    InterlockedExchange(&shared->heartbeat, static_cast<LONG>(GetTickCount()));
    InterlockedExchange(&shared->ready, 1); SetEvent(handles[1]);
    HANDLE wait[]{handles[2], handles[3]};
    for (;;) {
        const auto result = MsgWaitForMultipleObjects(2, wait, FALSE, 20, QS_ALLINPUT);
        if (result < WAIT_OBJECT_0 + 2 || result == WAIT_FAILED) break;
        InterlockedExchange(&shared->heartbeat, static_cast<LONG>(GetTickCount()));
        if (helper.foreground()) helper.synchronize();
        MSG msg;
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    }
    InterlockedExchange(&shared->enabled, 0);
    UnhookWindowsHookEx(hook);
    device.dwFlags = RIDEV_REMOVE; device.hwndTarget = nullptr;
    RegisterRawInputDevices(&device, 1, sizeof(device));
    DestroyWindow(helper.rawWindow);
    UnmapViewOfFile(shared);
    for (auto handle : handles) CloseHandle(handle);
    return 0;
#else
    (void)argc; (void)argv; return 2;
#endif
}
