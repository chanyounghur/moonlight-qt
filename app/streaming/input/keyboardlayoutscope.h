#pragma once

#include "SDL_compat.h"
#include <memory>

// Temporarily bypass Korean 101-key modifier translation on the stream thread.
// Only uses an already loaded US layout: never installs or changes defaults.
class KeyboardLayoutScope {
public:
    KeyboardLayoutScope();
    ~KeyboardLayoutScope();
    bool setActive(SDL_Window* window, bool active);
private:
    struct Impl;
    std::unique_ptr<Impl> m_Impl;
};
