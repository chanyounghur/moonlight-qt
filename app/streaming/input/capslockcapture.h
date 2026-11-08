#pragma once

#include "SDL_compat.h"
#include <memory>

// Supplements SDL's modifier capture with Caps Lock only. It never changes the
// existing Windows toggle state and only intercepts the focused stream window.
class CapsLockCapture {
public:
    CapsLockCapture();
    ~CapsLockCapture();
    bool setActive(SDL_Window* window, bool active);
    bool active() const;
    bool owns(const SDL_KeyboardEvent& event) const;
    int localToggleState() const;
private:
    struct Impl;
    std::unique_ptr<Impl> m_Impl;
};
