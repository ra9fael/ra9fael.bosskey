#pragma once

#include <QObject>
#include <QString>
#include <functional>
#include <memory>

class QThread;

namespace bosskey {

struct GuardLaunch {
    std::string program;
    std::string arguments;
};

class EnhancedInput final : public QObject {
public:
    explicit EnhancedInput(QObject* parent = nullptr);
    ~EnhancedInput() override;
    // Returns immediately. The host launches GuardLaunch outside the sandbox.
    bool start(const QString& sequence, GuardLaunch& launch,
               std::function<void(bool, unsigned long)> ready,
               std::function<void()> exit, std::function<void()> lost, QString& error);
    void stop();
private:
    struct Session;
    std::unique_ptr<Session> m_session;
    QThread* m_thread = nullptr;
    unsigned m_epoch = 0;
};

} // namespace bosskey
