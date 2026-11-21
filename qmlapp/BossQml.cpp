// BossQml.cpp - the cover's window lifecycle, all on the GUI thread.
//
// One QQmlEngine lives for the process (QtQuick comes from WinShun's qml\
// folder). One cover window per screen is created on first use — precreated at
// startup by precreateCover() — and kept hidden. Enhanced starts resolve the
// independent guard's consent/readiness before showing those windows.
#include "BossQml.h"
#include "CoverController.h"
#include "CoverInput.h"
#include "../logic.h"
#include "../winshun.h"

#include <QGuiApplication>
#include <QCursor>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickWindow>
#include <QScreen>
#include <QThread>

#include "platform/WindowEffects.h"

#include <utility>
#include <vector>

using namespace Qt::StringLiterals;

namespace bosskey {

namespace {

QQmlEngine* g_engine = nullptr;
CoverController* g_controller = nullptr;
CoverInput* g_input = nullptr;
EnhancedInput* g_guard = nullptr;
bool g_starting = false;
std::vector<QQuickWindow*> g_windows; // one per screen, kept for reuse

void guiHide();

QString appDir()
{
    return qEnvironmentVariable("WS_APP_DIR"); // set by the stub
}

QQmlEngine* ensureEngine()
{
    if (g_engine)
        return g_engine;
    g_engine = new QQmlEngine;
    g_engine->addImportPath(appDir() + u"/qml"_s); // QtQuick: WinShun's
    g_controller = new CoverController;
    g_input = new CoverInput(qGuiApp);
    g_guard = new EnhancedInput(qGuiApp);
    QObject::connect(qGuiApp, &QCoreApplication::aboutToQuit, g_input, &CoverInput::stop);
    QObject::connect(qGuiApp, &QCoreApplication::aboutToQuit, g_guard, &EnhancedInput::stop);
    return g_engine;
}

// The cover window for a screen, created on first use (frameless, on top,
// outside the taskbar and alt-tab via Qt.Tool).
QQuickWindow* coverWindow(QScreen* screen)
{
    for (QQuickWindow* window : g_windows)
        if (window->screen() == screen)
            return window;
    QQmlComponent component(g_engine, QUrl(u"qrc:/qt/qml/BossKey/BossCover.qml"_s),
                            QQmlComponent::PreferSynchronous);
    if (component.isError()) {
        winshun::debug("ra9fael.bosskey: BossCover.qml: " + component.errorString().toStdString());
        return nullptr;
    }
    QObject* object = component.createWithInitialProperties({
        {u"cover"_s, QVariant::fromValue<QObject*>(g_controller)},
        {u"screenIndex"_s, QGuiApplication::screens().indexOf(screen)},
    });
    auto* window = qobject_cast<QQuickWindow*>(object);
    if (!window) {
        winshun::debug("ra9fael.bosskey: BossCover.qml is not a window: " + component.errorString().toStdString());
        delete object;
        return nullptr;
    }
    window->setScreen(screen);
    window->create(); // native handle now, so showFullScreen() is one call
    g_windows.push_back(window);
    QObject::connect(window, &QWindow::visibleChanged, g_controller, [](bool visible) {
        if (!visible) {
            for (const QQuickWindow* cover : g_windows)
                if (cover->isVisible())
                    return;
            g_input->stop();
            g_guard->stop();
            g_controller->stopCover();
        }
    });
    return window;
}

bool startOrdinary(QString& error)
{
    return g_input->start(g_controller->exitSequence(), [] {
        winshun::debug("bosskey: exit source=native mode=ordinary");
        emit g_controller->exitRequested();
    }, error);
}

bool guiShowReady(const CoverSpec& spec, bool enhanced, std::string& error)
{
    ensureEngine();
    g_controller->applySpec(spec);

    QList<QScreen*> targets;
    if (spec.windowed && spec.previewScreen >= 0) {
        const QList<QScreen*> screens = QGuiApplication::screens();
        if (spec.previewScreen < screens.size())
            targets.push_back(screens.at(spec.previewScreen));
    } else if (spec.allScreens) {
        targets = QGuiApplication::screens();
    } else if (QScreen* at = QGuiApplication::screenAt(QCursor::pos())) {
        targets.push_back(at);
    } else if (QScreen* primary = QGuiApplication::primaryScreen()) {
        targets.push_back(primary);
    }
    if (targets.isEmpty()) {
        error = "no screen";
        return false;
    }

    QString inputError;
    if (!enhanced && spec.nativeInput && !startOrdinary(inputError)) {
        guiHide();
        error = inputError.toStdString();
        return false;
    }

    QList<QScreen*> covered;
    for (QScreen* screen : targets) {
        QQuickWindow* window = coverWindow(screen);
        if (!window)
            continue;
        window->setScreen(screen);
        const QRect geometry = screen->geometry();
        const QRect shown = spec.windowed
            ? QRect(geometry.x(), geometry.y(), 480, 270) : geometry;
        window->setGeometry(shown);
        if (spec.windowed) {
            window->show();
        } else {
            window->showFullScreen();
        }
        ws::win::bringToFront(window); // a user gesture just happened: take the focus
        covered.push_back(screen);
    }
    if (covered.isEmpty()) {
        g_input->stop();
        g_guard->stop();
        error = "no cover window could be shown";
        return false;
    }
    for (QQuickWindow* window : g_windows)
        if (!covered.contains(window->screen()))
            window->hide();
    g_controller->startCover();
    winshun::debug("ra9fael.bosskey: cover shown, scene=" + spec.scene + ", screens="
        + std::to_string(covered.size()) + ", protection=" + (enhanced ? "enhanced" : "ordinary"));
    return true;
}

bool guiShow(const CoverSpec& spec, std::string& error, GuardLaunch* launch)
{
    ensureEngine();
    if (g_starting || coverVisible()) {
        error = "a cover is already active or awaiting consent";
        return false;
    }
    if (!spec.enhanced || !launch)
        return guiShowReady(spec, false, error);
    g_starting = true;
    QString inputError;
    if (g_guard->start(QString::fromStdString(spec.exitSequence), *launch,
            [spec](bool enhanced, unsigned long failure) {
                g_starting = false;
                if (!enhanced) {
                    winshun::debug("bosskey: enhanced input unavailable (" + std::to_string(failure)
                        + "); using ordinary mode");
                    g_guard->stop();
                }
                std::string showError;
                if (!guiShowReady(spec, enhanced, showError)) {
                    guiHide();
                    winshun::debug("bosskey: cover failed: " + showError);
                    emit g_controller->exitRequested();
                }
            }, [] {
                winshun::debug("bosskey: exit source=native mode=enhanced");
                emit g_controller->exitRequested();
            }, [] {
                g_guard->stop();
                QString failure;
                if (!startOrdinary(failure))
                    guiHide();
                winshun::debug("bosskey: enhanced input disconnected; using ordinary mode");
            }, inputError))
        return true;
    g_starting = false;
    winshun::debug("bosskey: " + inputError.toStdString() + "; using ordinary mode");
    return guiShowReady(spec, false, error);
}

void guiHide()
{
    g_starting = false;
    if (g_guard)
        g_guard->stop();
    if (g_input)
        g_input->stop();
    for (QQuickWindow* window : g_windows)
        window->hide();
    if (g_controller)
        g_controller->stopCover();
}

} // namespace

// Called from qmlmain once the GUI is up: builds the engine, the controller
// and one hidden window per screen, so the first cover is instant.
void precreateCover()
{
    ensureEngine();
    for (QScreen* screen : QGuiApplication::screens())
        coverWindow(screen);
}

bool showCover(const CoverSpec& spec, std::string& error, GuardLaunch* launch)
{
    if (QThread::currentThread() == qGuiApp->thread())
        return guiShow(spec, error, launch);
    std::string guiError;
    QMetaObject::invokeMethod(qGuiApp, [&] { guiShow(spec, guiError, launch); }, Qt::BlockingQueuedConnection);
    error = std::move(guiError);
    return error.empty();
}

void hideCover()
{
    if (!g_engine)
        return;
    if (QThread::currentThread() == qGuiApp->thread()) {
        guiHide();
        return;
    }
    QMetaObject::invokeMethod(qGuiApp, [] { guiHide(); }, Qt::BlockingQueuedConnection);
}

bool coverVisible()
{
    if (!g_engine)
        return false;
    bool visible = false;
    if (QThread::currentThread() == qGuiApp->thread()) {
        for (const QQuickWindow* window : g_windows)
            visible = visible || window->isVisible();
        return visible;
    }
    QMetaObject::invokeMethod(qGuiApp, [&] {
        for (const QQuickWindow* window : g_windows)
            visible = visible || window->isVisible();
    }, Qt::BlockingQueuedConnection);
    return visible;
}

CoverController* coverController()
{
    return g_controller;
}

} // namespace bosskey
