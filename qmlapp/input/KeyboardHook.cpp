#include "KeyboardHook.h"

namespace bosskey {
namespace { thread_local KeyboardHook* active = nullptr; }

KeyboardHook::KeyboardHook(const std::string& sequence, std::function<void()> exit)
    : m_policy(sequence), m_exit(std::move(exit)) {}

KeyboardHook::~KeyboardHook()
{
    if (m_hook)
        ::UnhookWindowsHookEx(m_hook);
    if (active == this)
        active = nullptr;
}

DWORD KeyboardHook::install()
{
    for (unsigned key = 0; key < 256; ++key)
        m_policy.initialDown(key, (::GetAsyncKeyState(static_cast<int>(key)) & 0x8000) != 0);
    HMODULE module = nullptr;
    ::GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                         reinterpret_cast<LPCWSTR>(&keyboard), &module);
    active = this;
    m_hook = ::SetWindowsHookExW(WH_KEYBOARD_LL, keyboard, module, 0);
    return m_hook ? ERROR_SUCCESS : ::GetLastError();
}

LRESULT CALLBACK KeyboardHook::keyboard(int code, WPARAM message, LPARAM data)
{
    if (code != HC_ACTION || !active)
        return ::CallNextHookEx(nullptr, code, message, data);
    const bool pressed = message == WM_KEYDOWN || message == WM_SYSKEYDOWN;
    const bool released = message == WM_KEYUP || message == WM_SYSKEYUP;
    if (!pressed && !released)
        return ::CallNextHookEx(nullptr, code, message, data);
    const auto& event = *reinterpret_cast<const KBDLLHOOKSTRUCT*>(data);
    const auto result = active->m_policy.key(event.vkCode, pressed, (event.flags & LLKHF_ALTDOWN) != 0);
    if (result.exit)
        active->m_exit();
    return result.swallow ? 1 : ::CallNextHookEx(nullptr, code, message, data);
}

void drainKeyboard(KeyboardHook& hook)
{
    hook.drain();
    const ULONGLONG until = ::GetTickCount64() + 1500;
    while (!hook.drained() && ::GetTickCount64() < until) {
        ::MsgWaitForMultipleObjects(0, nullptr, FALSE, 15, QS_ALLINPUT);
        MSG message;
        while (::PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
            ::TranslateMessage(&message);
            ::DispatchMessageW(&message);
        }
    }
}

} // namespace bosskey
