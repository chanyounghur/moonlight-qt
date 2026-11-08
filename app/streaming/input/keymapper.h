#pragma once

// Platform-independent key-state engine. Keys are opaque integers supplied by
// the adapter; no SDL, Qt, Windows, or Moonlight protocol dependency lives here.
#include <map>
#include <set>
#include <utility>
#include <vector>

namespace macinput {
enum Modifier : unsigned { Control = 1, Alt = 2, Shift = 4, Meta = 8 };

struct Rule {
    int trigger;
    unsigned modifiers;
    std::vector<int> output;
};

struct KeyEvent {
    int key;
    bool down;
    unsigned modifiers;
};

struct Result {
    std::vector<KeyEvent> events;
    bool releaseCapture = false;
};

class KeyMapper {
public:
    void configure(std::map<int, unsigned> modifiers, std::vector<Rule> rules,
                   int escapeKey, unsigned escapeModifiers)
    {
        clear();
        m_Modifiers = std::move(modifiers);
        m_Rules = std::move(rules);
        m_EscapeKey = escapeKey;
        m_EscapeModifiers = escapeModifiers;
    }

    Result process(int key, bool down)
    {
        Result result;
        if (down) {
            if (!m_Physical.insert(key).second) {
                return result; // Repeat/duplicate down must not retrigger a rule.
            }
            const auto mods = modifiersFor(m_Physical);
            if (key == m_EscapeKey && mods == m_EscapeModifiers) {
                reconcile({}, result.events);
                clear();
                result.releaseCapture = true;
                return result;
            }
            for (const auto& rule : m_Rules) {
                if (rule.trigger == key && rule.modifiers == mods) {
                    m_Active.emplace(key, rule);
                    break;
                }
            }
        }
        else {
            if (m_Physical.erase(key) == 0) {
                return result; // Includes late ups after a focus change/reset.
            }
            m_Active.erase(key);
        }

        // Bind a mapping until its trigger is released, even if the user lets
        // go of Alt first. Shared output keys have set/refcount semantics.
        unsigned suppressedModifiers = 0;
        for (const auto& binding : m_Active) {
            suppressedModifiers |= binding.second.modifiers;
        }
        std::set<int> desired;
        for (int physical : m_Physical) {
            if (m_Active.count(physical) == 0 &&
                !(modifierFor(physical) & suppressedModifiers)) {
                desired.insert(physical);
            }
        }
        for (const auto& binding : m_Active) {
            desired.insert(binding.second.output.begin(), binding.second.output.end());
        }
        reconcile(desired, result.events);
        return result;
    }

    // The adapter releases actual protocol keys on focus loss/disconnect.
    void clear()
    {
        m_Physical.clear();
        m_Active.clear();
        m_Emitted.clear();
    }

private:
    unsigned modifierFor(int key) const
    {
        const auto it = m_Modifiers.find(key);
        return it == m_Modifiers.end() ? 0 : it->second;
    }

    unsigned modifiersFor(const std::set<int>& keys) const
    {
        unsigned modifiers = 0;
        for (int key : keys) {
            modifiers |= modifierFor(key);
        }
        return modifiers;
    }

    void reconcile(const std::set<int>& desired, std::vector<KeyEvent>& events)
    {
        // Release regular keys before modifiers; press modifiers before keys.
        // In particular, lift the already-forwarded Alt before Command+Space.
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
    std::vector<Rule> m_Rules;
    int m_EscapeKey = 0;
    unsigned m_EscapeModifiers = 0;
    std::set<int> m_Physical;
    std::set<int> m_Emitted;
    std::map<int, Rule> m_Active;
};
} // namespace macinput
