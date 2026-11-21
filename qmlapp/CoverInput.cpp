#include "CoverInput.h"
#include "input/KeyboardHook.h"

#include <QMetaObject>
#include <QThread>
#include <windows.h>

#include <future>

namespace bosskey {

CoverInput::~CoverInput() { stop(); }

bool CoverInput::start(const QString& sequence, std::function<void()> exit, QString& error)
{
    stop();
    const unsigned epoch = m_epoch;
    std::promise<DWORD> ready;
    auto result = ready.get_future();
    m_thread = QThread::create([this, sequence = sequence.toStdString(), exit = std::move(exit),
                                epoch, &ready] {
        MSG message;
        ::PeekMessageW(&message, nullptr, WM_USER, WM_USER, PM_NOREMOVE);
        m_threadId = ::GetCurrentThreadId();
        KeyboardHook hook(sequence, [this, epoch, exit] {
            QMetaObject::invokeMethod(this, [this, epoch, exit] {
                if (m_thread && m_epoch == epoch)
                    exit();
            }, Qt::QueuedConnection);
        });
        const DWORD failure = hook.install();
        ready.set_value(failure);
        if (!failure) {
            while (::GetMessageW(&message, nullptr, 0, 0) > 0) {
                ::TranslateMessage(&message);
                ::DispatchMessageW(&message);
            }
            drainKeyboard(hook);
        }
    });
    m_thread->start();
    const DWORD failure = result.get();
    if (failure) {
        stop();
        error = QStringLiteral("keyboard guard: SetWindowsHookExW failed (%1)").arg(failure);
        return false;
    }
    return true;
}

void CoverInput::stop()
{
    ++m_epoch;
    if (!m_thread)
        return;
    ::PostThreadMessageW(m_threadId, WM_QUIT, 0, 0);
    m_thread->wait();
    delete m_thread;
    m_thread = nullptr;
    m_threadId = 0;
}

} // namespace bosskey
