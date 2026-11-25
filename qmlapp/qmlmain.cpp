// qmlmain.cpp - the entries of BossKeyApp.dll.
//
// winshunBossKeyMain: the plugin itself, as WinShun starts it. Qt goes up on
// this (the GUI) thread; the plugin protocol (winshun.h serve(), the query and
// action mapping in main.cpp) runs on a worker thread and marshals its cover
// requests here (qmlapp/BossQml.cpp). The cover windows are precreated hidden,
// so a show is only showFullScreen + raise.
//
// winshunBossKeyPreview: `bosskey.exe --cover bsod|update|gcc|matrix|blackhole
// |aquarium [--fish <n>] [--bubbles <n>] [--pace <percent>] [--all]
// [--windowed] [--screen <index>] [--autohide <sec>] [--en]
// [--app-dir <folder>]` opens the cover directly, no WinShun and no protocol.
// --windowed shows a small window; --screen selects its screen index (0-based).
// --enhanced emits a guard-launch JSON diagnostic for the external controller;
// --qml-only isolates the focused QML exit path without a native hook.
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
#include <QElapsedTimer>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <windows.h>

#include <shellapi.h>

#include <cstdio>
#include <algorithm>
#include <thread>
#include <mutex>
#include <vector>

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
    QString metricsPath;
    for (int i = 0; i < args.size(); ++i) {
        const QString& arg = args.at(i);
        if (arg == u"--metrics" && i + 1 < args.size()) {
            metricsPath = args.at(++i);
            continue;
        }
        if (arg == u"--en" || arg == u"--all" || arg == u"--windowed" || arg == u"--app-dir")
            continue;
        if (arg == u"--fish" && i + 1 < args.size()) {
            spec.tank.fish = bosskey::clampInt(args.at(++i).toInt(), bosskey::kAquariumFishMin,
                                               bosskey::kAquariumFishMax);
            continue;
        }
        if (arg == u"--bubbles" && i + 1 < args.size()) {
            spec.tank.bubbles = bosskey::clampInt(args.at(++i).toInt(), bosskey::kAquariumBubblesMin,
                                                  bosskey::kAquariumBubblesMax);
            continue;
        }
        if (arg == u"--pace" && i + 1 < args.size()) {
            spec.tank.speed = bosskey::clampInt(args.at(++i).toInt(), bosskey::kTankSpeedMin,
                                                bosskey::kTankSpeedMax) / 100.0;
            continue;
        }
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
    spec.enhanced = args.contains(u"--enhanced"_s);
    spec.nativeInput = !args.contains(u"--qml-only"_s);
    if (spec.enhanced && !spec.nativeInput) {
        std::fprintf(stderr, "bosskey: --enhanced and --qml-only are separate acceptance modes\n");
        return 2;
    }
    if (!bosskey::isKnownScene(spec.scene)) {
        std::fprintf(stderr, "bosskey: --cover needs bsod | update | gcc | matrix | blackhole | aquarium\n");
        std::fflush(stderr);
        return 2;
    }
    std::string error;
    QElapsedTimer frameClock;
    std::mutex frameMutex;
    std::vector<double> frameIntervals;
    qint64 previousFrame = 0;
    if (!metricsPath.isEmpty()) {
        if (!spec.windowed || spec.autoHideSec <= 12 || spec.autoHideSec > 120) {
            std::fprintf(stderr, "bosskey: --metrics requires --windowed and --autohide 13..120\n");
            return 2;
        }
        frameClock.start();
        for (QWindow* window : QGuiApplication::allWindows()) {
            if (auto* quick = qobject_cast<QQuickWindow*>(window)) {
                QObject::connect(quick, &QQuickWindow::frameSwapped, &app, [&] {
                    const qint64 now = frameClock.nsecsElapsed();
                    const std::lock_guard<std::mutex> lock(frameMutex);
                    if (now >= 12'000'000'000LL && previousFrame >= 12'000'000'000LL)
                        frameIntervals.push_back((now - previousFrame) / 1e6);
                    previousFrame = now;
                }, Qt::DirectConnection);
            }
        }
    }
    bosskey::GuardLaunch launch;
    if (!bosskey::showCover(spec, error, &launch)) {
        std::fprintf(stderr, "bosskey: %s\n", error.c_str());
        std::fflush(stderr);
        return 2;
    }
    if (!launch.program.empty()) {
        std::fprintf(stderr, "bosskey: guard-launch=%s\n", winshun::Json(winshun::Json::Object {
            {"path", launch.program}, {"args", launch.arguments},
        }).dump().c_str());
        std::fflush(stderr);
    }
    const int result = app.exec();
    if (!metricsPath.isEmpty()) {
        QJsonArray intervals;
        {
            const std::lock_guard<std::mutex> lock(frameMutex);
            for (const double interval : frameIntervals)
                intervals.append(interval);
        }
        QFile file(metricsPath);
        if (!file.open(QIODevice::WriteOnly)) {
            std::fprintf(stderr, "bosskey: cannot write preview metrics\n");
            return 3;
        }
        file.write(QJsonDocument(QJsonObject {
            {u"warmup_seconds"_s, 12},
            {u"frame_swapped_intervals_ms"_s, intervals},
        }).toJson());
    }
    return result;
}
