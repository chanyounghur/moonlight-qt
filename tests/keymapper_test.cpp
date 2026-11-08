#include "../app/streaming/input/keymapper.h"

#include <cstdlib>
#include <iostream>
#include <random>
#include <string>

using namespace macinput;
enum Key { A = 4, Z = 29, Space = 44, Caps = 57, Tab = 43,
           Ctrl = 224, ShiftKey = 225, AltKey = 226, Cmd = 227,
           RightCtrl = 228, RightAlt = 230 };

void check(bool value, const char* message)
{
    if (!value) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

KeyMapper mapper()
{
    KeyMapper result;
    result.configure({{Ctrl, Control}, {RightCtrl, Control}, {ShiftKey, Shift},
                      {AltKey, Alt}, {RightAlt, Alt}, {Cmd, Meta}},
                     {{Caps, 0, {Ctrl, Space}}, {Space, Alt, {Cmd, Space}},
                      {Tab, Alt, {Cmd, Tab}}}, Z, Control | Alt | Shift);
    return result;
}

void expect(const Result& result, std::initializer_list<std::pair<int, bool>> expected)
{
    check(result.events.size() == expected.size(), "event count");
    size_t index = 0;
    for (const auto& event : expected) {
        check(result.events[index].key == event.first && result.events[index].down == event.second,
              "ordered event");
        ++index;
    }
}

int main()
{
    {
        auto m = mapper();
        expect(m.process(A, true), {{A, true}});
        expect(m.process(A, true), {});
        expect(m.process(A, false), {{A, false}});
        expect(m.process(A, false), {});
    }
    {
        auto m = mapper();
        auto result = m.process(Caps, true);
        expect(result, {{Ctrl, true}, {Space, true}});
        check(result.events.back().modifiers == Control, "Caps sends Control with Space");
        expect(m.process(Caps, true), {});
        expect(m.process(Caps, false), {{Space, false}, {Ctrl, false}});
    }
    {
        auto m = mapper();
        expect(m.process(AltKey, true), {{AltKey, true}});
        auto result = m.process(Space, true);
        expect(result, {{AltKey, false}, {Cmd, true}, {Space, true}});
        check(result.events.back().modifiers == Meta, "Option must not leak into Spotlight");
        expect(m.process(Space, false), {{Space, false}, {Cmd, false}, {AltKey, true}});
        expect(m.process(AltKey, false), {{AltKey, false}});
    }
    {
        auto m = mapper();
        m.process(RightAlt, true);
        expect(m.process(Space, true), {{RightAlt, false}, {Cmd, true}, {Space, true}});
        expect(m.process(RightAlt, false), {});
        expect(m.process(Space, false), {{Space, false}, {Cmd, false}});
    }
    {
        auto m = mapper();
        m.process(AltKey, true);
        m.process(Tab, true);
        expect(m.process(Space, true), {{Space, true}});
        expect(m.process(Tab, false), {{Tab, false}}); // Command is shared.
        expect(m.process(AltKey, false), {});
        expect(m.process(Space, false), {{Space, false}, {Cmd, false}});
    }
    {
        auto m = mapper();
        m.process(Caps, true);
        expect(m.process(Ctrl, true), {}); // Physical and synthesized Control overlap.
        expect(m.process(Caps, false), {{Space, false}});
        expect(m.process(Ctrl, false), {{Ctrl, false}});
    }
    {
        auto m = mapper();
        m.process(ShiftKey, true);
        expect(m.process(Caps, true), {{Caps, true}}); // Exact modifiers, no accidental mapping.
        m.clear();
        expect(m.process(Caps, false), {});
        expect(m.process(ShiftKey, false), {});
        expect(m.process(A, true), {{A, true}});
    }
    {
        auto m = mapper();
        m.process(Caps, true);
        m.clear(); // Adapter raises protocol keys during focus loss.
        expect(m.process(Caps, false), {});
        expect(m.process(Caps, true), {{Ctrl, true}, {Space, true}});
    }
    {
        auto m = mapper();
        m.process(Ctrl, true);
        m.process(AltKey, true);
        m.process(ShiftKey, true);
        auto result = m.process(Z, true);
        check(result.releaseCapture, "escape releases capture");
        expect(result, {{Ctrl, false}, {ShiftKey, false}, {AltKey, false}});
        expect(m.process(Z, false), {});
        expect(m.process(Ctrl, false), {});
        expect(m.process(AltKey, false), {});
        expect(m.process(ShiftKey, false), {});
    }
    {
        // Exercise overlapping mappings and arbitrary release ordering. Replay
        // the emitted stream as a host: no duplicate down, orphan up, or stuck key.
        std::mt19937 random(20261108);
        const int keys[] = {A, Caps, Space, Tab, Ctrl, RightCtrl, AltKey, RightAlt, ShiftKey, Cmd};
        for (int scenario = 0; scenario < 1000; ++scenario) {
            auto m = mapper();
            std::set<int> host;
            auto replay = [&](const Result& result) {
                for (const auto& event : result.events) {
                    if (event.down) check(host.insert(event.key).second, "duplicate host key down");
                    else check(host.erase(event.key) == 1, "orphan host key up");
                }
            };
            for (int step = 0; step < 100; ++step) {
                replay(m.process(keys[random() % 10], (random() % 2) != 0));
            }
            for (int key : keys) replay(m.process(key, false));
            check(host.empty(), "all physical releases leave host empty");
        }
    }
    std::cout << "Key mapper tests passed (including 1000 randomized sequences).\n";
}
