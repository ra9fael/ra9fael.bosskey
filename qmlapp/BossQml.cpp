// BossQml.cpp - the cover's window lifecycle, all on the GUI thread.
//
// One QQmlEngine lives for the process (QtQuick comes from WinShun's qml\
// folder). One cover window per screen is created on first use — precreated at
// startup by precreateCover() — and kept hidden, so showCover() is only
// showFullScreen + raise: the cover must be up before the boss walks over.
#include "BossQml.h"
#include "CoverController.h"
#include "DesktopProvider.h"
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

#include <memory>
#include <vector>
#include <cwchar>

using namespace Qt::StringLiterals;

namespace bosskey {

namespace {

QQmlEngine* g_engine = nullptr;
CoverController* g_controller = nullptr;
DesktopProvider* g_desktop = nullptr;
std::vector<QQuickWindow*> g_windows; // one per screen, kept for reuse
struct MinimizedWindow {
    HWND hwnd;
    WINDOWPLACEMENT placement;
};
std::vector<MinimizedWindow> g_minimized; // windows the show-desktop step put down
bool g_desktopCleared = false;            // the desktop is clear and must be put back

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
    g_desktop = new DesktopProvider;
    g_engine->addImageProvider(u"desktop"_s, g_desktop);
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
    return window;
}

// Snapshots the rectangle the cover is about to occupy. Taken while every cover
// window is still hidden: a live grab would capture the cover itself and feed
// it straight back into the lens.
void grabDesktop(QScreen* screen, const QRect& shown)
{
    if (!g_desktop || !g_controller)
        return;
    const QImage image = screen->grabWindow(0, shown.x(), shown.y(), shown.width(), shown.height()).toImage();
    g_desktop->setSnapshot(QGuiApplication::screens().indexOf(screen), image);
    g_controller->bumpDesktopEpoch();
}

// Show desktop exactly as the keyboard shortcut does: Win+D. The shell does the
// minimizing (so elevated windows go down as well), and it remembers how to put
// everything back, so sending it again restores what was up.
void sendWinD()
{
    // Injected hotkeys need air between the events: a shell hotkey that arrives
    // in the same instant as its modifier is regularly missed.
    const auto tap = [](WORD key, DWORD flags) {
        INPUT input = {};
        input.type = INPUT_KEYBOARD;
        input.ki.wVk = key;
        input.ki.dwFlags = flags;
        ::SendInput(1, &input, sizeof(INPUT));
        QThread::msleep(25);
    };
    tap(VK_LWIN, 0);
    tap('D', 0);
    tap('D', KEYEVENTF_KEYUP);
    tap(VK_LWIN, KEYEVENTF_KEYUP);
}

// One top-level window the boss could be reading, put down and remembered.
BOOL CALLBACK minimizeTopLevel(HWND hwnd, LPARAM)
{
    if (!::IsWindowVisible(hwnd) || ::IsIconic(hwnd) || ::GetWindow(hwnd, GW_OWNER))
        return TRUE;
    DWORD pid = 0;
    ::GetWindowThreadProcessId(hwnd, &pid);
    if (pid == ::GetCurrentProcessId())
        return TRUE; // the cover windows and this plugin
    if (::GetWindowLongPtrW(hwnd, GWL_EXSTYLE) & WS_EX_TOOLWINDOW)
        return TRUE;
    wchar_t cls[32] = {};
    ::GetClassNameW(hwnd, cls, 32);
    if (std::wcscmp(cls, L"Progman") == 0 || std::wcscmp(cls, L"WorkerW") == 0
        || std::wcscmp(cls, L"Shell_TrayWnd") == 0
        || std::wcscmp(cls, L"Shell_SecondaryTrayWnd") == 0)
        return TRUE; // the wallpaper, icons and taskbar stay
    WINDOWPLACEMENT placement{};
    placement.length = sizeof(placement);
    if (!::GetWindowPlacement(hwnd, &placement))
        return TRUE;
    if (!::ShowWindow(hwnd, SW_MINIMIZE))
        return TRUE;
    g_minimized.push_back({hwnd, placement});    return TRUE;
}

// Anything the boss could be reading? Only then is Win+D a clear rather than a
// restore of what somebody already minimized.
int windowsUp()
{
    struct State {
        int count;
    } state{0};
    ::EnumWindows([](HWND hwnd, LPARAM data) -> BOOL {
        auto* state = reinterpret_cast<State*>(data);
        if (!::IsWindowVisible(hwnd) || ::IsIconic(hwnd))
            return TRUE;
        if (::GetWindow(hwnd, GW_OWNER))
            return TRUE;
        DWORD pid = 0;
        ::GetWindowThreadProcessId(hwnd, &pid);
        if (pid == ::GetCurrentProcessId())
            return TRUE;
        if (::GetWindowLongPtrW(hwnd, GWL_EXSTYLE) & WS_EX_TOOLWINDOW)
            return TRUE;
        wchar_t cls[32] = {};
        ::GetClassNameW(hwnd, cls, 32);
        const bool shell = std::wcscmp(cls, L"Progman") == 0 || std::wcscmp(cls, L"WorkerW") == 0
            || std::wcscmp(cls, L"Shell_TrayWnd") == 0
            || std::wcscmp(cls, L"Shell_SecondaryTrayWnd") == 0;
        if (!shell)
            ++state->count;
        return TRUE;
    }, reinterpret_cast<LPARAM>(&state));
    return state.count;
}

// The black hole cover sits on the wallpaper: the windows go down before the
// snapshot, so what the lens bends is the desktop and not the boss's editor.
// ShowWindow does the work per window (that is all Win+D does underneath), and
// injected keys back it up for windows UIPI will not let us touch.
void showDesktop()
{
    if (g_desktopCleared)
        return;
    const int before = windowsUp();
    if (before == 0)
        return; // already the desktop: nothing to put down
    ::EnumWindows(minimizeTopLevel, 0);
    for (int i = 0; i < 25 && windowsUp() > 0; ++i)
        QThread::msleep(20);
    const int minimized = static_cast<int>(g_minimized.size());
    if (windowsUp() > 0)
        sendWinD();
    for (int i = 0; i < 25 && windowsUp() > 0; ++i)
        QThread::msleep(20);
    QThread::msleep(150); // the DWM trails the window state by a frame or two
    g_desktopCleared = true;
    // Windows of a higher integrity level refuse both calls (UIPI), so the ones
    // still up after both are the ones this process may not touch.
    winshun::debug("ra9fael.bosskey: show desktop: " + std::to_string(before)
                   + " windows up, " + std::to_string(minimized) + " minimized, "
                   + std::to_string(windowsUp()) + " left");
}

// Back where they were: the placements saved above, last-shown last so the
// window the boss was reading ends up in front again.
void restoreDesktop()
{
    if (!g_desktopCleared)
        return;
    for (auto it = g_minimized.rbegin(); it != g_minimized.rend(); ++it)
        if (::IsWindow(it->hwnd) && ::IsIconic(it->hwnd))
            ::SetWindowPlacement(it->hwnd, &it->placement);
    g_minimized.clear();
    g_desktopCleared = false;
}

bool guiShow(const CoverSpec& spec, std::string& error)
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

    QList<QScreen*> covered;
    // The black hole is a lens over the wallpaper: clear the screen before the
    // snapshot, so what it bends is the desktop and not the windows on it.
    if (spec.scene == QLatin1String(kSceneBlackhole))
        showDesktop();
    for (QScreen* screen : targets) {
        QQuickWindow* window = coverWindow(screen);
        if (!window)
            continue;
        window->setScreen(screen);
        const QRect geometry = screen->geometry();
        const QRect shown = spec.windowed
            ? QRect(geometry.x(), geometry.y(), 480, 270) : geometry;
        window->setGeometry(shown);
        // Only the black hole scene lenses the desktop; the other covers stay
        // off the grab path so showing one remains instant.
        if (spec.scene == QLatin1String(kSceneBlackhole))
            grabDesktop(screen, shown);
        if (spec.windowed) {
            window->show();
        } else {
            window->showFullScreen();
        }
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
    restoreDesktop();
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

