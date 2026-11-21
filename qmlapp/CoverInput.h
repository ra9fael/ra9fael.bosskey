#ifndef BOSSKEY_COVERINPUT_H
#define BOSSKEY_COVERINPUT_H

#include <QObject>
#include <QString>
#include <functional>

class QThread;

namespace bosskey {

// A message-pumping thread keeps the low-level hook independent of rendering.
class CoverInput final : public QObject {
public:
    explicit CoverInput(QObject* parent = nullptr) : QObject(parent) {}
    ~CoverInput() override;
    // Installation success does not grant access to higher-integrity windows.
    bool start(const QString& sequence, std::function<void()> exit, QString& error);
    void stop();

private:
    QThread* m_thread = nullptr;
    unsigned long m_threadId = 0;
    unsigned m_epoch = 0;
};

} // namespace bosskey
#endif
