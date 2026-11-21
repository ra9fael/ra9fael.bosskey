#pragma once

#include <windows.h>
#include <algorithm>
#include <array>
#include <string>
#include <utility>

namespace bosskey {

// State is limited to key-down flags and a sequence prefix length.
class KeyPolicy {
public:
    explicit KeyPolicy(std::string sequence) : m_sequence(std::move(sequence)) {}

    struct Result { bool swallow = false; bool exit = false; };
    void initialDown(unsigned key, bool down) { m_down.at(key) = down; }
    void drain() { m_draining = true; }
    bool drained() const
    {
        return std::none_of(m_swallowed.begin(), m_swallowed.end(), [](bool down) { return down; });
    }

    Result key(unsigned key, bool pressed, bool altDown = false)
    {
        if (key >= m_down.size())
            return {};
        const bool repeat = m_down[key];
        m_down[key] = pressed;
        if (!pressed) {
            const bool swallowed = m_swallowed[key];
            m_swallowed[key] = false;
            return {swallowed, false};
        }
        if (m_draining)
            return {m_swallowed[key], false};
        const bool ctrl = m_down[VK_LCONTROL] || m_down[VK_RCONTROL] || m_down[VK_CONTROL];
        const bool shift = m_down[VK_LSHIFT] || m_down[VK_RSHIFT] || m_down[VK_SHIFT];
        const bool alt = m_down[VK_LMENU] || m_down[VK_RMENU] || m_down[VK_MENU] || altDown;
        const bool win = m_down[VK_LWIN] || m_down[VK_RWIN];
        const bool alnum = (key >= 'A' && key <= 'Z') || (key >= '0' && key <= '9')
            || (key >= VK_NUMPAD0 && key <= VK_NUMPAD9);
        const bool shortcut = key == VK_LWIN || key == VK_RWIN || key == VK_APPS || win
            || (alt && (key == VK_TAB || key == VK_ESCAPE || key == VK_F4 || key == VK_SPACE))
            || (ctrl && key == VK_ESCAPE) || (shift && key == VK_F10);
        if (!shortcut && !alnum && !m_swallowed[key])
            return {};
        m_swallowed[key] = true;
        bool exit = false;
        if (alnum && !ctrl && !alt && !win && !repeat && !m_exited) {
            const char c = key >= VK_NUMPAD0 && key <= VK_NUMPAD9
                ? static_cast<char>('0' + key - VK_NUMPAD0)
                : static_cast<char>(key >= 'A' && key <= 'Z' ? key + ('a' - 'A') : key);
            while (m_matched && m_sequence[m_matched] != c) {
                size_t prefix = m_matched - 1;
                while (prefix && m_sequence.compare(0, prefix, m_sequence, m_matched - prefix, prefix) != 0)
                    --prefix;
                m_matched = prefix;
            }
            if (!m_sequence.empty() && m_sequence[m_matched] == c)
                ++m_matched;
            exit = !m_sequence.empty() && m_matched == m_sequence.size();
            if (exit) {
                m_matched = 0;
                m_exited = true;
            }
        }
        return {true, exit};
    }

private:
    std::array<bool, 256> m_down {};
    std::array<bool, 256> m_swallowed {};
    std::string m_sequence;
    size_t m_matched = 0;
    bool m_draining = false;
    bool m_exited = false;
};

} // namespace bosskey
