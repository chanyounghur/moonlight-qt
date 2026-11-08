#include "../app/streaming/input/keymapper.h"
#include <cstdlib>
#include <iostream>
#include <random>

using namespace macinput;
enum Key { A = 4, C = 6, Q = 20, Z = 29, Space = 44, Caps = 57, Tab = 43, Application = 101,
           Ctrl = 224, ShiftKey = 225, AltKey = 226, Win = 227,
           RightCtrl = 228, RightShift = 229, RightAlt = 230, RightWin = 231 };

void check(bool value, const char* message)
{
    if (!value) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}

KeyMapper mapper()
{
    KeyMapper result;
    result.configure({{Ctrl, Control}, {RightCtrl, Control}, {ShiftKey, Shift}, {RightShift, Shift},
                      {AltKey, Alt}, {RightAlt, Alt}, {Win, Meta}, {RightWin, Meta}},
                     {{AltKey, Win}, {Win, AltKey}, {RightAlt, RightWin}, {RightWin, RightAlt},
                      {Application, RightWin}},
                     Z, Control | Alt | Shift);
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
        expect(m.process(Application, true), {{RightWin, true}});
        auto event = m.process(C, true);
        check(event.events[0].modifiers == Meta, "Super/Menu contributes Command modifier flags");
        expect(m.process(C, false), {{C, false}});
        expect(m.process(RightAlt, true), {}); // Two physical keys share Command.
        expect(m.process(Application, false), {});
        event = m.process(Space, true);
        check(event.events[0].modifiers == Meta, "Command remains held by right Alt");
        expect(m.process(RightAlt, false), {{RightWin, false}});
        expect(m.process(Space, false), {{Space, false}});
    }
    {
        auto m = mapper();
        expect(m.process(Caps, true), {{Caps, true}}); // Never synthesize Ctrl+Space.
        expect(m.process(Caps, true), {});
        expect(m.process(Caps, false), {{Caps, false}});
        expect(m.process(Caps, false), {});
    }
    {
        // Every key gets Command without needing a registered shortcut recipe.
        for (int ordinary : {A, C, Q, Space, Tab, Caps}) {
            auto m = mapper();
            expect(m.process(AltKey, true), {{Win, true}});
            auto event = m.process(ordinary, true);
            expect(event, {{ordinary, true}});
            check(event.events[0].modifiers == Meta, "Command role on arbitrary ordinary key");
            expect(m.process(ordinary, false), {{ordinary, false}});
            expect(m.process(AltKey, false), {{Win, false}});
        }
    }
    {
        auto m = mapper();
        expect(m.process(Win, true), {{AltKey, true}});
        auto event = m.process(A, true);
        check(event.events[0].modifiers == Alt, "Windows key becomes Option");
        expect(m.process(Win, false), {{AltKey, false}});
        expect(m.process(A, false), {{A, false}});
    }
    {
        auto m = mapper();
        expect(m.process(RightAlt, true), {{RightWin, true}});
        expect(m.process(RightWin, true), {{RightAlt, true}});
        auto event = m.process(C, true);
        check(event.events[0].modifiers == (Meta | Alt), "right Command+Option roles");
        expect(m.process(RightAlt, false), {{RightWin, false}});
        expect(m.process(RightWin, false), {{RightAlt, false}});
        expect(m.process(C, false), {{C, false}});
    }
    {
        auto m = mapper();
        m.process(AltKey, true);
        expect(m.process(Tab, true), {{Tab, true}});
        expect(m.process(Tab, false), {{Tab, false}});
        m.process(ShiftKey, true);
        auto event = m.process(Tab, true);
        check(event.events[0].modifiers == (Meta | Shift), "reverse app cycling");
        expect(m.process(AltKey, false), {{Win, false}}); // Release even while Tab is down.
        expect(m.process(Tab, false), {{Tab, false}});
    }
    {
        auto m = mapper();
        m.process(Ctrl, true);
        m.process(AltKey, true);
        m.process(ShiftKey, true);
        auto event = m.process(Z, true);
        check(event.releaseCapture, "escape uses physical Alt, not mapped Option");
        expect(event, {{Ctrl, false}, {ShiftKey, false}, {Win, false}});
        expect(m.process(Z, false), {});
        expect(m.process(Ctrl, false), {});
        expect(m.process(AltKey, false), {});
    }
    {
        auto m = mapper();
        m.process(AltKey, true);
        m.process(Caps, true);
        m.clear(); // Adapter raises actual protocol keys on focus loss.
        expect(m.process(AltKey, false), {});
        expect(m.process(Caps, false), {});
        expect(m.process(Caps, true), {{Caps, true}});
    }
    {
        std::mt19937 random(20261108);
        const int keys[] = {A, Caps, Space, Tab, Ctrl, RightCtrl, AltKey, RightAlt,
                            ShiftKey, RightShift, Win, RightWin, Application};
        for (int scenario = 0; scenario < 1000; ++scenario) {
            auto m = mapper();
            std::set<int> host;
            auto replay = [&](const Result& result) {
                for (const auto& event : result.events) {
                    if (event.down) check(host.insert(event.key).second, "duplicate host down");
                    else check(host.erase(event.key) == 1, "orphan host up");
                }
            };
            for (int step = 0; step < 100; ++step)
                replay(m.process(keys[random() % (sizeof(keys) / sizeof(keys[0]))], (random() % 2) != 0));
            for (int key : keys) replay(m.process(key, false));
            check(host.empty(), "all physical releases leave host empty");
        }
    }
    std::cout << "Mac layout and raw Caps Lock tests passed (1000 randomized sequences).\n";
}
