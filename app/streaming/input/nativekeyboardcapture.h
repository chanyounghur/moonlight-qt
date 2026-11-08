#pragma once
#include "SDL_compat.h"
#include <memory>

// A background child owns both the keyboard hook and right-modifier raw input.
// The foreground process never registers raw keyboards or changes OS layouts.
class NativeKeyboardCapture {
public:
    static constexpr Sint32 EventCode = 0x4D4B4559; // MKEY
    NativeKeyboardCapture();
    ~NativeKeyboardCapture();
    bool setActive(SDL_Window* window, bool active, bool blockCaps = true);
    bool active() const;
    bool owns(const SDL_KeyboardEvent& event) const;
    bool translate(const SDL_UserEvent& event, SDL_KeyboardEvent& key) const;
    bool failed(const SDL_UserEvent& event) const;
    int localToggleState() const;
    static bool hasForeground(SDL_Window* window);
    // Called before Qt/SDL initialization. Returns -1 for ordinary app startup.
    static int runHelper(int argc, char** argv);
private:
    struct Impl;
    std::unique_ptr<Impl> m_Impl;
};
