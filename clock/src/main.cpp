// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// vela-clock: Vela's Clock, the one of Windows 11's (Alarms & Clock): timers,
// alarms, a stopwatch and the world's clocks.
//
//   vela-clock
//
// Closing the window while something is armed leaves the process running
// without one, so it still rings; it ends when nothing is armed any more.
// Starting it again brings the window back.

#include "clock.h"
#include "controls.h"
#include "iconprovider.h"
#include "language.h"

#include <QGuiApplication>
#include <QIcon>
#include <QLocalServer>
#include <QLocalSocket>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QTimer>
#include <QtQml/QQmlExtensionPlugin>

Q_IMPORT_QML_PLUGIN(Vela_ControlsPlugin)

int main(int argc, char* argv[])
{
    if (qEnvironmentVariableIsEmpty("QT_QUICK_DEFAULT_TEXT_RENDER_TYPE")) {
        QQuickWindow::setTextRenderType(QQuickWindow::NativeTextRendering);
    }
    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("vela-clock"));
    QGuiApplication::setOrganizationName(QStringLiteral("Vela"));

    // One instance: a second start asks the first to show its window.
    const QString serverName = QStringLiteral("vela-clock-%1").arg(qEnvironmentVariable("USER"));
    QLocalSocket probe;
    probe.connectToServer(serverName);
    if (probe.waitForConnected(300)) {
        probe.write("show\n");
        probe.waitForBytesWritten(300);
        return 0;
    }
    QLocalServer::removeServer(serverName);
    QLocalServer server;
    server.listen(serverName);

    QQmlApplicationEngine* qmlEngine = nullptr;
    vela::language::install(QStringLiteral("vela-clock"), [&qmlEngine] {
        QGuiApplication::setApplicationDisplayName(QCoreApplication::translate("Clock", "Clock"));
        if (qmlEngine) {
            qmlEngine->retranslate();
        }
    });
    QGuiApplication::setApplicationDisplayName(QCoreApplication::translate("Clock", "Clock"));
    QGuiApplication::setDesktopFileName(QStringLiteral("vela-clock"));
    QGuiApplication::setWindowIcon(QIcon::fromTheme(QStringLiteral("preferences-system-time")));
    QGuiApplication::setQuitOnLastWindowClosed(false);
    QIcon::setFallbackThemeName(QStringLiteral("hicolor"));

    Clock clock;

    QQmlApplicationEngine engine;
    qmlEngine = &engine;
    Appearance* style = vela::controls::install(engine, Appearance::Mode::Apps);
    engine.addImageProvider(QStringLiteral("appicon"), new IconProvider(32));
    engine.rootContext()->setContextProperty(QStringLiteral("Clock"), &clock);
    engine.loadFromModule("Vela.Clock", "Main");
    if (engine.rootObjects().isEmpty()) {
        return 1;
    }
    auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().first());

    QObject::connect(&server, &QLocalServer::newConnection, &app, [&server, window] {
        while (QLocalSocket* socket = server.nextPendingConnection()) {
            socket->deleteLater();
        }
        window->show();
        window->raise();
        window->requestActivate();
    });
    // Windowless and nothing left to ring: done.
    QObject::connect(&clock, &Clock::armedChanged, &app, [&clock, window] {
        if (!clock.armed() && !window->isVisible()) {
            QCoreApplication::quit();
        }
    });

    QTimer::singleShot(0, &app, [style] { style->watch(); });
    return app.exec();
}
