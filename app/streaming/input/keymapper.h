#pragma once

// Key roles, not shortcut recipes. No SDL, Qt, or protocol dependency.
#include <map>
#include <set>
#include <utility>
#include <vector>

namespace macinput {
enum Modifier : unsigned { Control = 1, Alt = 2, Shift = 4, Meta = 8 };
struct KeyEvent { int key; bool down; unsigned modifiers; };
struct Result { std::vector<KeyEvent> events; bool releaseCapture = false; };

class KeyMapper {
public:
    void configure(std::map<int, unsigned> modifiers, std::map<int, int> keyMap,
                   int escapeKey, unsigned escapeModifiers)
    {
        clear();
        m_Modifiers = std::move(modifiers);
        m_KeyMap = std::move(keyMap);
        m_EscapeKey = escapeKey;
        m_EscapeModifiers = escapeModifiers;
    }

    Result process(int key, bool down)
    {
        Result result;
        if (down) {
            if (!m_Physical.insert(key).second) return result;
            // Escape is evaluated on physical keys, before the layout mapping.
            if (key == m_EscapeKey && modifiersFor(m_Physical) == m_EscapeModifiers) {
                reconcile({}, result.events);
                clear();
                result.releaseCapture = true;
                return result;
            }
        }
        else if (m_Physical.erase(key) == 0) return result;

        std::set<int> desired;
        for (int physical : m_Physical) {
            const auto mapped = m_KeyMap.find(physical);
            desired.insert(mapped == m_KeyMap.end() ? physical : mapped->second);
        }
        reconcile(desired, result.events);
        return result;
    }

    // The adapter releases actual protocol keys on focus loss/disconnect.
    void clear() { m_Physical.clear(); m_Emitted.clear(); }

private:
    unsigned modifierFor(int key) const
    {
        const auto it = m_Modifiers.find(key);
        return it == m_Modifiers.end() ? 0 : it->second;
    }

    unsigned modifiersFor(const std::set<int>& keys) const
    {
        unsigned modifiers = 0;
        for (int key : keys) modifiers |= modifierFor(key);
        return modifiers;
    }

    void reconcile(const std::set<int>& desired, std::vector<KeyEvent>& events)
    {
        // Release ordinary keys before modifiers and press modifiers first.
        for (bool modifier : {false, true}) {
            const auto previous = m_Emitted;
            for (int key : previous) {
                if ((modifierFor(key) != 0) == modifier && desired.count(key) == 0) {
                    m_Emitted.erase(key);
                    events.push_back({key, false, modifiersFor(m_Emitted)});
                }
            }
        }
        for (bool modifier : {true, false}) {
            for (int key : desired) {
                if ((modifierFor(key) != 0) == modifier && m_Emitted.insert(key).second) {
                    events.push_back({key, true, modifiersFor(m_Emitted)});
                }
            }
        }
    }

    std::map<int, unsigned> m_Modifiers;
    std::map<int, int> m_KeyMap;
    int m_EscapeKey = 0;
    unsigned m_EscapeModifiers = 0;
    std::set<int> m_Physical;
    std::set<int> m_Emitted;
};
} // namespace macinput
