// CoverController.h - the cover's state, one per process, on the GUI thread.
// The QML windows bind to it as a context property (`cover`). The sequence
// matcher and the fake percent live in logic.h; this class only wires them to
// timers and key events.
#ifndef BOSSKEY_COVERCONTROLLER_H
#define BOSSKEY_COVERCONTROLLER_H

#include <QElapsedTimer>
#include <QObject>
#include <QTimer>
#include <string>

#include "GccModel.h"

namespace bosskey {

struct CoverSpec;

class CoverController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString scene READ scene NOTIFY sceneChanged)
    Q_PROPERTY(QString exitSequence READ exitSequence NOTIFY exitSequenceChanged)
    Q_PROPERTY(QString project READ project NOTIFY projectChanged)
    Q_PROPERTY(bool zh READ zh NOTIFY zhChanged)
    Q_PROPERTY(int percent READ percent NOTIFY percentChanged)
    Q_PROPERTY(bool shown READ shown NOTIFY shownChanged)
    Q_PROPERTY(GccModel* gccModel READ gccModel CONSTANT)
public:
    explicit CoverController(QObject* parent = nullptr);

    QString scene() const { return m_scene; }
    QString exitSequence() const { return m_exitSequence; }
    QString project() const { return m_project; }
    bool zh() const { return m_zh; }
    int percent() const { return m_percent; }
    bool shown() const { return m_shown; }
    GccModel* gccModel() const { return m_gcc; }

    // Applies the spec (before the windows show); regenerates the gcc log.
    void applySpec(const CoverSpec& spec);
    // Timer state: on while any cover window is visible.
    void startCover();
    void stopCover();

    // A key press from the cover window (QML Keys). True when the sequence
    // completed and the cover must close.
    Q_INVOKABLE bool keyPressed(const QString& text, int key);

signals:
    void exitRequested();
    void sceneChanged();
    void exitSequenceChanged();
    void projectChanged();
    void zhChanged();
    void percentChanged();
    void shownChanged();

private:
    void tick();

    QString m_scene;
    QString m_exitSequence = "deadbeef";
    QString m_project;
    bool m_zh = true;
    int m_autoHideSec = 0;
    int m_percent = 0;
    bool m_shown = false;
    std::string m_buffer; // the rolling key buffer (logic.h's pushKey)
    GccModel* m_gcc = nullptr;
    QTimer m_percentTimer;
    QTimer m_autoHideTimer;
    QElapsedTimer m_elapsed;
};

} // namespace bosskey
#endif // BOSSKEY_COVERCONTROLLER_H
