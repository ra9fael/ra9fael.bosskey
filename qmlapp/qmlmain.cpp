// qmlmain.cpp - the entries of BossKeyApp.dll.
//
// winshunBossKeyMain: the plugin itself, as WinShun starts it. Qt goes up on
// this (the GUI) thread; the plugin protocol (winshun.h serve(), the query and
// action mapping in main.cpp) runs on a worker thread and marshals its cover
// requests here (qmlapp/BossQml.cpp). The cover windows are precreated hidden,
// so a show is only showFullScreen + raise.
//
// winshunBossKeyPreview: `bosskey.exe --cover bsod|update|gcc [--all]
// [--windowed] [--screen <index>] [--autohide <sec>] [--en]
// [--app-dir <folder>]` opens the cover directly, no WinShun and no protocol.
// --windowed shows a small window; --screen selects its screen index (0-based).
// --autohide closes it
// after the seconds (the verify scripts' chance to screenshot and leave).
//
// The runtime setup follows WinShun's shared-Qt contract (plugin.json declares
// runtime.qt = "host"): the host sets WS_QT_DIR, the starter passes that folder
// here, and Qt's library paths point there. The covers render on D3D11 (see
// startGui): the black hole scene is a fragment shader, and Qt's software
// rasterizer draws no ShaderEffect at all.

#include "BossQml.h"
#include "../logic.h"
#include "../winshun.h"

#include <QGuiApplication>
#include <QIcon>
#include <QQuickWindow>

#include <QDir>

#include <windows.h>

#include <shellapi.h>

#include <cstdio>
#include <algorithm>
#include <thread>

using namespace Qt::StringLiterals;

namespace {

// The GUI thread's setup, shared by both entries.
void prepareGui(const QString& appDir)
{
    QCoreApplication::setLibraryPaths({appDir}); // platforms\qwindows.dll: WinShun's
    QCoreApplication::setOrganizationName(u"WinShun"_s);
    QCoreApplication::setApplicationName(u"WinShun"_s);
}

void startGui()
{
    // Whatever QT_QPA_PLATFORM the environment carries (a test harness, say)
    // must not decide which platform plugin the cover runs on.
    qunsetenv("QT_QPA_PLATFORM");
    QQuickWindow::setGraphicsApi(QSGRendererInterface::Direct3D11);
    QQuickWindow::setTextRenderType(QQuickWindow::NativeTextRendering);
}

} // namespace

// The plugin: WinShun starts bosskey.exe, the stub loads this and calls here.
extern "C" __declspec(dllexport) int winshunBossKeyMain(const wchar_t* appDirName)
{
    const QString appDir = QString::fromWCharArray(appDirName);
    ::SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOOPENFILEERRORBOX);
    // D3D11 is the one backend that draws the black hole scene's ShaderEffect:
    // Qt's software rasterizer skips it silently. plugin.json raises the sandbox
    // job limit to 1024 MB for the swapchains and texture atlases.
    prepareGui(appDir);
    int argc = 1;
    char name[] = "BossKey";
    char* argv[] = {name, nullptr};
    QGuiApplication app(argc, argv);
    QIcon icon;
    icon.addFile(u":/plugin-icons/app-16.png"_s, QSize(16, 16));
    icon.addFile(u":/plugin-icons/app-20.png"_s, QSize(20, 20));
    icon.addFile(u":/plugin-icons/app-24.png"_s, QSize(24, 24));
    icon.addFile(u":/plugin-icons/app-32.png"_s, QSize(32, 32));
    icon.addFile(u":/plugin-icons/app-40.png"_s, QSize(40, 40));
    icon.addFile(u":/plugin-icons/app-48.png"_s, QSize(48, 48));
    icon.addFile(u":/plugin-icons/app-64.png"_s, QSize(64, 64));
    icon.addFile(u":/plugin-icons/app-256.png"_s, QSize(256, 256));
    app.setWindowIcon(icon);
    startGui();
    QGuiApplication::setQuitOnLastWindowClosed(false); // the cover hides, not quits

    // The cover windows go up hidden now; showing one later is instant.
    bosskey::precreateCover();

    // The exit sequence's end: the controller (GUI thread) asks, this hides.
    QObject::connect(bosskey::coverController(), &bosskey::CoverController::exitRequested,
                     &app, [] { bosskey::hideCover(); });

    // The plugin protocol on a worker thread: serve() blocks on stdin, the GUI
    // event loop runs here. serve() returning (shutdown, stdin closed) is the
    // process's end.
    int rc = 0;
    std::thread serve([&] {
        rc = bosskey::runServeLoop();
        QMetaObject::invokeMethod(&app, &QCoreApplication::quit, Qt::QueuedConnection);
    });
    app.exec();
    serve.join();
    return rc;
}

// The standalone check: one cover, no WinShun. The stub passes WinShun's
// folder as --app-dir; --cover names the scene, --all covers every screen.
extern "C" __declspec(dllexport) int winshunBossKeyPreview(const wchar_t* appDirName)
{
    const QString appDir = QString::fromWCharArray(appDirName);
    ::SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOOPENFILEERRORBOX);
    QStringList args;
    int count = 0;
    if (LPWSTR* wide = ::CommandLineToArgvW(::GetCommandLineW(), &count)) {
        for (int i = 1; i < count; ++i)
            args.push_back(QString::fromWCharArray(wide[i]));
        ::LocalFree(wide);
    }
    prepareGui(appDir);
    int argc = 1;
    char name[] = "BossKey";
    char* argv[] = {name, nullptr};
    QGuiApplication app(argc, argv);
    QIcon icon;
    icon.addFile(u":/plugin-icons/app-16.png"_s, QSize(16, 16));
    icon.addFile(u":/plugin-icons/app-20.png"_s, QSize(20, 20));
    icon.addFile(u":/plugin-icons/app-24.png"_s, QSize(24, 24));
    icon.addFile(u":/plugin-icons/app-32.png"_s, QSize(32, 32));
    icon.addFile(u":/plugin-icons/app-40.png"_s, QSize(40, 40));
    icon.addFile(u":/plugin-icons/app-48.png"_s, QSize(48, 48));
    icon.addFile(u":/plugin-icons/app-64.png"_s, QSize(64, 64));
    icon.addFile(u":/plugin-icons/app-256.png"_s, QSize(256, 256));
    app.setWindowIcon(icon);
    startGui();
    QGuiApplication::setQuitOnLastWindowClosed(false);
    bosskey::precreateCover();
    QObject::connect(bosskey::coverController(), &bosskey::CoverController::exitRequested,
                     &app, [&app] {
                         bosskey::hideCover();
                         QCoreApplication::quit(); // the preview ends with the cover
                     });

    bosskey::CoverSpec spec;
    spec.exitSequence = "deadbeef";
    spec.gccProject = "winshun-core";
    spec.autoHideSec = 0;
    spec.zh = !args.contains(u"--en"_s);
    QString scene;
    for (int i = 0; i < args.size(); ++i) {
        const QString& arg = args.at(i);
        if (arg == u"--en" || arg == u"--all" || arg == u"--windowed" || arg == u"--app-dir")
            continue;
        if (arg == u"--screen" && i + 1 < args.size()) {
            spec.previewScreen = args.at(++i).toInt();
            continue;
        }
        if (arg == u"--autohide" && i + 1 < args.size()) {
            spec.autoHideSec = std::max(0, args.at(++i).toInt());
            continue;
        }
        if (arg == u"--cover" && i + 1 < args.size()) {
            scene = args.at(++i);
            continue;
        }
    }
    spec.scene = scene.toStdString();
    spec.allScreens = args.contains(u"--all"_s);
    spec.windowed = args.contains(u"--windowed"_s);
    if (!bosskey::isKnownScene(spec.scene)) {
        std::fprintf(stderr, "bosskey: --cover needs bsod | update | gcc | matrix | blackhole\n");
        std::fflush(stderr);
        return 2;
    }
    std::string error;
    if (!bosskey::showCover(spec, error)) {
        std::fprintf(stderr, "bosskey: %s\n", error.c_str());
        std::fflush(stderr);
        return 2;
    }
    return app.exec();
}
