#pragma once

#include "KeyPolicy.h"
#include <functional>

namespace bosskey {

// Construct, pump and destroy on one thread; callbacks must not block.
class KeyboardHook {
public:
    KeyboardHook(const std::string& sequence, std::function<void()> exit);
    ~KeyboardHook();
    DWORD install();
    void drain() { m_policy.drain(); }
    bool drained() const { return m_policy.drained(); }

private:
    static LRESULT CALLBACK keyboard(int code, WPARAM message, LPARAM data);
    KeyPolicy m_policy;
    std::function<void()> m_exit;
    HHOOK m_hook = nullptr;
};

// Continue suppressing repeats/releases of swallowed keys for at most 1.5s.
void drainKeyboard(KeyboardHook& hook);

} // namespace bosskey
