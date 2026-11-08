#pragma once

#include "SDL_compat.h"
#include <memory>

// Process-local SDL raw input only. Never changes Windows layouts or IMEs.
class RawKeyboardScope {
public:
    RawKeyboardScope();
    ~RawKeyboardScope();
    bool setActive(SDL_Window* window, bool active);
    static bool hasForeground(SDL_Window* window);
private:
    struct Impl;
    std::unique_ptr<Impl> m_Impl;
};
