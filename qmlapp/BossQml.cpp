// BossQml.cpp - the cover's window lifecycle, all on the GUI thread.
//
// One QQmlEngine lives for the process (QtQuick comes from WinShun's qml\
// folder). One cover window per screen is created on first use — precreated at
// startup by precreateCover() — and kept hidden, so showCover() is only
// showFullScreen + raise: the cover must be up before the boss walks over.
#include "BossQml.h"
#include "CoverController.h"
#include "../winshun.h"

#include <QGuiApplication>
#include <QCursor>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickWindow>
#include <QScreen>
#include <QThread>

#include "platform/WindowEffects.h"

#include <memory>
#include <vector>

using namespace Qt::StringLiterals;

namespace bosskey {

namespace {

QQmlEngine* g_engine = nullptr;
CoverController* g_controller = nullptr;
std::vector<QQuickWindow*> g_windows; // one per screen, kept for reuse

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
    return window;
}

bool guiShow(const CoverSpec& spec, std::string& error)
{
    ensureEngine();
    g_controller->applySpec(spec);

    QList<QScreen*> targets;
    if (spec.allScreens) {
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

    QList<QScreen*> covered;
    for (QScreen* screen : targets) {
        QQuickWindow* window = coverWindow(screen);
        if (!window)
            continue;
        window->setScreen(screen);
        window->showFullScreen();
        ws::win::bringToFront(window); // a user gesture just happened: take the focus
        covered.push_back(screen);
    }
    if (covered.isEmpty()) {
        error = "no cover window could be shown";
        return false;
    }
    for (QQuickWindow* window : g_windows)
        if (!covered.contains(window->screen()))
            window->hide();
    g_controller->startCover();
    winshun::debug("ra9fael.bosskey: cover shown, scene=" + spec.scene + ", screens="
        + std::to_string(covered.size()));
    return true;
}

void guiHide()
{
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

bool showCover(const CoverSpec& spec, std::string& error)
{
    if (QThread::currentThread() == qGuiApp->thread())
        return guiShow(spec, error);
    std::string guiError;
    QMetaObject::invokeMethod(qGuiApp, [&] { guiShow(spec, guiError); }, Qt::BlockingQueuedConnection);
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
