#include "EnhancedInput.h"
#include "input/GuardProtocol.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <QThread>
#include <QUuid>
#include <sddl.h>

namespace bosskey {
using namespace guard;

struct EnhancedInput::Session {
    Handle cancel {::CreateEventW(nullptr, TRUE, FALSE, nullptr)};
    Handle pipe;
};

EnhancedInput::EnhancedInput(QObject* parent) : QObject(parent) {}
EnhancedInput::~EnhancedInput() { stop(); }

bool EnhancedInput::start(const QString& sequence, GuardLaunch& launch,
                         std::function<void(bool, unsigned long)> ready,
                         std::function<void()> exit, std::function<void()> lost, QString& error)
{
    stop();
    const QString program = QCoreApplication::applicationDirPath() + QStringLiteral("/bosskey-guard.exe");
    if (!QFileInfo::exists(program) || !validSequence(sequence.toStdWString())) {
        error = QStringLiteral("enhanced input helper unavailable or sequence too long");
        return false;
    }
    auto session = std::make_unique<Session>();
    PSECURITY_DESCRIPTOR descriptor = nullptr;
    if (!::ConvertStringSecurityDescriptorToSecurityDescriptorW(
            L"D:(A;;GA;;;OW)(A;;GA;;;BA)(A;;GA;;;SY)S:(ML;;NW;;;LW)",
            SDDL_REVISION_1, &descriptor, nullptr)) {
        error = QStringLiteral("input pipe security: %1").arg(::GetLastError());
        return false;
    }
    SECURITY_ATTRIBUTES attributes {sizeof attributes, descriptor, FALSE};
    const QString name = QUuid::createUuid().toString(QUuid::WithoutBraces);
    session->pipe.reset(::CreateNamedPipeW(pipeName(name.toStdWString()).c_str(),
        PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED | FILE_FLAG_FIRST_PIPE_INSTANCE,
        PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS,
        1, 256, 256, 0, &attributes));
    ::LocalFree(descriptor);
    if (!session->pipe || !session->cancel) {
        error = QStringLiteral("input pipe: %1").arg(::GetLastError());
        return false;
    }
    launch.program = program.toStdString();
    launch.arguments = "--session " + name.toStdString() + " --owner " + std::to_string(::GetCurrentProcessId())
        + " --sequence " + sequence.toStdString();
    m_session = std::move(session);
    const unsigned epoch = m_epoch;
    Session* state = m_session.get();
    m_thread = QThread::create([this, state, epoch, ready, exit, lost] {
        const auto deliver = [this, epoch](std::function<void()> callback) {
            QMetaObject::invokeMethod(this, [this, epoch, callback] {
                if (m_epoch == epoch && m_thread)
                    callback();
            }, Qt::QueuedConnection);
        };
        const auto wait = [state](HANDLE event, DWORD timeout) {
            const HANDLE handles[] = {state->cancel.get(), event};
            return ::WaitForMultipleObjects(2, handles, FALSE, timeout);
        };
        bool connected = false;
        {
            Read connection(state->pipe.get());
            connected = connection.connect() && wait(connection.event(), startupTimeout) == WAIT_OBJECT_0 + 1
                && connection.finish(true);
        }
        if (!connected) {
            deliver([ready] { ready(false, ERROR_TIMEOUT); });
            return;
        }
        Packet packet {};
        bool active = false;
        {
            Read input(state->pipe.get());
            if (input.begin(packet) && wait(input.event(), 10000) == WAIT_OBJECT_0 + 1 && input.finish()
                && packet.kind == Kind::Ready) {
                ULONG pid = 0;
                if (::GetNamedPipeClientProcessId(state->pipe.get(), &pid)) {
                    Handle process(::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid));
                    active = process && elevated(process.get());
                }
            }
        }
        if (!active) {
            write(state->pipe.get(), Kind::Stop);
            const DWORD failure = packet.kind == Kind::Failed ? packet.error : ERROR_ACCESS_DENIED;
            deliver([ready, failure] { ready(false, failure); });
            return;
        }
        deliver([ready] { ready(true, 0); });
        bool stopping = false;
        for (;;) {
            Read input(state->pipe.get());
            if (!input.begin(packet))
                break;
            const DWORD which = wait(input.event(), INFINITE);
            if (which == WAIT_OBJECT_0) {
                stopping = true;
                write(state->pipe.get(), Kind::Stop);
                if (::WaitForSingleObject(input.event(), 2500) == WAIT_OBJECT_0)
                    input.finish();
                break;
            }
            if (which != WAIT_OBJECT_0 + 1 || !input.finish())
                break;
            if (packet.kind == Kind::Exit)
                deliver(exit);
            else
                break;
        }
        // Closing the pipe also stops the helper if the GUI disappears.
        if (!stopping)
            deliver(lost);
    });
    m_thread->start();
    return true;
}

void EnhancedInput::stop()
{
    ++m_epoch;
    if (!m_thread)
        return;
    ::SetEvent(m_session->cancel.get());
    m_thread->wait();
    delete m_thread;
    m_thread = nullptr;
    m_session.reset();
}

} // namespace bosskey
