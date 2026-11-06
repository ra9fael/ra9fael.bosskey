// qmlmain.cpp - the entries of BossKeyApp.dll.
//
// winshunBossKeyMain: the plugin itself, as WinShun starts it. Qt goes up on
// this (the GUI) thread; the plugin protocol (winshun.h serve(), the query and
// action mapping in main.cpp) runs on a worker thread and marshals its cover
// requests here (qmlapp/BossQml.cpp). The cover windows are precreated hidden,
// so a show is only showFullScreen + raise.
//
// winshunBossKeyPreview: `bosskey.exe --cover bsod|update|gcc [--all]
// [--autohide <sec>] [--en] [--app-dir <folder>]` opens the cover directly, no
// WinShun and no protocol — how the scenes are checked. --autohide closes it
// after the seconds (the verify scripts' chance to screenshot and leave).
//
// The runtime setup mirrors WinShun's winshun.capture (src/capture-plugin/
// PluginMain.cpp): WinShun's Qt, the renderer the host is using.

#include "BossQml.h"
#include "../logic.h"
#include "../winshun.h"

#include <QGuiApplication>
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
void prepareGui(const QString& appDir, bool software)
{
    QCoreApplication::setLibraryPaths({appDir}); // platforms\qwindows.dll: WinShun's
    // The font engine as WinShun has it: FreeType, GDI for the software renderer.
    qputenv("QT_QPA_PLATFORM", software ? "windows:fontengine=gdi" : "windows:fontengine=freetype");
    QCoreApplication::setOrganizationName(u"WinShun"_s);
    QCoreApplication::setApplicationName(u"WinShun"_s);
}

void startGui(bool software)
{
    qunsetenv("QT_QPA_PLATFORM");
    QQuickWindow::setGraphicsApi(software ? QSGRendererInterface::Software : QSGRendererInterface::Direct3D11);
    QQuickWindow::setTextRenderType(QQuickWindow::NativeTextRendering);
}

} // namespace

// The plugin: WinShun starts bosskey.exe, the stub loads this and calls here.
extern "C" __declspec(dllexport) int winshunBossKeyMain(const wchar_t* appDirName)
{
    const QString appDir = QString::fromWCharArray(appDirName);
    ::SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOOPENFILEERRORBOX);
    // The host's renderer when the appearance says software (the strict
    // sandbox); otherwise D3D11. BOSSKEY_RENDERER=software forces it for
    // debugging.
    const bool software = qEnvironmentVariable("BOSSKEY_RENDERER") == "software";
    prepareGui(appDir, software);
    int argc = 1;
    char name[] = "BossKey";
    char* argv[] = {name, nullptr};
    QGuiApplication app(argc, argv);
    startGui(software);
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
    const bool software = qEnvironmentVariable("BOSSKEY_RENDERER") == "software";
    prepareGui(appDir, software);
    int argc = 1;
    char name[] = "BossKey";
    char* argv[] = {name, nullptr};
    QGuiApplication app(argc, argv);
    startGui(software);
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
        if (arg == u"--en" || arg == u"--all" || arg == u"--app-dir")
            continue;
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
    if (!bosskey::isKnownScene(spec.scene)) {
        std::fprintf(stderr, "bosskey: --cover needs bsod | update | gcc\n");
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
