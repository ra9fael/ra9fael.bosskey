#pragma once

#include <windows.h>
#include <string>

namespace bosskey::guard {

enum class Kind : DWORD { Ready = 1, Exit, Failed, Stopped, Stop };
struct Packet { Kind kind; DWORD error = 0; };
constexpr DWORD startupTimeout = 120000;

class Handle {
public:
    explicit Handle(HANDLE value = nullptr) : m_value(value) {}
    ~Handle() { reset(); }
    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;
    Handle(Handle&& other) noexcept : m_value(other.m_value) { other.m_value = nullptr; }
    Handle& operator=(Handle&& other) noexcept
    {
        if (this != &other) {
            reset(other.m_value);
            other.m_value = nullptr;
        }
        return *this;
    }
    HANDLE get() const { return m_value; }
    explicit operator bool() const { return m_value && m_value != INVALID_HANDLE_VALUE; }
    void reset(HANDLE value = nullptr)
    {
        if (*this)
            ::CloseHandle(m_value);
        m_value = value;
    }
private:
    HANDLE m_value;
};

inline std::wstring pipeName(const std::wstring& session)
{
    return L"\\\\.\\pipe\\WinShun.BossKey.Input." + session;
}

inline bool validSession(const std::wstring& text)
{
    return text.size() == 36 && text.find_first_not_of(L"0123456789abcdef-") == std::wstring::npos;
}

inline bool validSequence(const std::wstring& text)
{
    return !text.empty() && text.size() <= 64
        && text.find_first_not_of(L"abcdefghijklmnopqrstuvwxyz0123456789") == std::wstring::npos;
}

inline bool elevated(HANDLE process)
{
    HANDLE raw = nullptr;
    if (!::OpenProcessToken(process, TOKEN_QUERY, &raw))
        return false;
    Handle token(raw);
    TOKEN_ELEVATION elevation {};
    DWORD size = 0;
    return ::GetTokenInformation(token.get(), TokenElevation, &elevation, sizeof elevation, &size)
        && elevation.TokenIsElevated;
}

// Every operation owns and settles its OVERLAPPED before its storage goes away.
class Read {
public:
    explicit Read(HANDLE pipe) : m_pipe(pipe), m_event(::CreateEventW(nullptr, TRUE, FALSE, nullptr))
    { m_overlapped.hEvent = m_event.get(); }
    ~Read()
    {
        if (m_pending) {
            ::CancelIoEx(m_pipe, &m_overlapped);
            DWORD ignored = 0;
            ::GetOverlappedResult(m_pipe, &m_overlapped, &ignored, TRUE);
        }
    }
    bool connect()
    {
        const BOOL connected = ::ConnectNamedPipe(m_pipe, &m_overlapped);
        const DWORD error = connected ? ERROR_SUCCESS : ::GetLastError();
        m_pending = !connected && error == ERROR_IO_PENDING;
        if (connected || error == ERROR_PIPE_CONNECTED)
            ::SetEvent(m_event.get());
        return connected || m_pending || error == ERROR_PIPE_CONNECTED;
    }
    bool begin(Packet& packet)
    {
        ::ResetEvent(m_event.get());
        DWORD got = 0;
        const BOOL read = ::ReadFile(m_pipe, &packet, sizeof packet, &got, &m_overlapped);
        m_pending = !read && ::GetLastError() == ERROR_IO_PENDING;
        if (read)
            ::SetEvent(m_event.get());
        m_immediateBytes = got;
        return m_pending || (read && got == sizeof packet);
    }
    bool finish(bool connection = false)
    {
        DWORD got = m_immediateBytes;
        if (m_pending) {
            m_pending = false;
            if (!::GetOverlappedResult(m_pipe, &m_overlapped, &got, FALSE))
                return false;
        }
        return connection || got == sizeof(Packet);
    }
    HANDLE event() const { return m_event.get(); }
private:
    HANDLE m_pipe;
    Handle m_event;
    OVERLAPPED m_overlapped {};
    bool m_pending = false;
    DWORD m_immediateBytes = 0;
};

inline bool write(HANDLE pipe, Kind kind, DWORD error = 0)
{
    const Packet packet {kind, error};
    Handle event(::CreateEventW(nullptr, TRUE, FALSE, nullptr));
    OVERLAPPED operation {};
    operation.hEvent = event.get();
    DWORD count = 0;
    if (::WriteFile(pipe, &packet, sizeof packet, &count, &operation))
        return count == sizeof packet;
    if (::GetLastError() != ERROR_IO_PENDING)
        return false;
    const bool signaled = ::WaitForSingleObject(event.get(), 1000) == WAIT_OBJECT_0;
    if (!signaled)
        ::CancelIoEx(pipe, &operation);
    const bool completed = ::GetOverlappedResult(pipe, &operation, &count, TRUE) != FALSE;
    return signaled && completed && count == sizeof packet;
}

inline void reportFailure(HANDLE pipe, DWORD error)
{
    if (!write(pipe, Kind::Failed, error))
        return;
    Packet packet {};
    Read reply(pipe);
    if (reply.begin(packet) && ::WaitForSingleObject(reply.event(), 2500) == WAIT_OBJECT_0)
        reply.finish();
}

} // namespace bosskey::guard
