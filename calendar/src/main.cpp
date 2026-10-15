// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// vela-calendar: Vela's Calendar, the one of Windows 11's, local only.
//
//   vela-calendar [file.ics]
//
// One process per user: a second start asks the first to show its window (and
// to import the file), then ends. The first stays alive without a window while
// reminders are pending.

#include "controls.h"
#include "iconprovider.h"
#include "language.h"
#include "store.h"

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
    // Open and Save come from the portal (see notepad/src/main.cpp).
    qputenv("QT_QPA_PLATFORMTHEME", "xdgdesktopportal");
    if (qEnvironmentVariableIsEmpty("QT_QUICK_DEFAULT_TEXT_RENDER_TYPE")) {
        QQuickWindow::setTextRenderType(QQuickWindow::NativeTextRendering);
    }
    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("vela-calendar"));

    // The file given as argument, absolute: the first instance has another directory.
    QString file;
    for (const QString& arg : app.arguments().mid(1)) {
        if (!arg.startsWith('-')) file = QFileInfo(arg).absoluteFilePath();
    }

    const QString serverName = QStringLiteral("vela-calendar-%1").arg(qEnvironmentVariable("USER"));
    {
        QLocalSocket socket;
        socket.connectToServer(serverName);
        if (socket.waitForConnected(300)) {
            socket.write((file.isEmpty() ? QStringLiteral("show") : "open " + file).toUtf8() + '\n');
            socket.waitForBytesWritten(1000);
            return 0;
        }
    }
    QLocalServer::removeServer(serverName); // left by a crash
    QLocalServer server;
    server.listen(serverName);
    app.setQuitOnLastWindowClosed(false); // Store decides, by the reminders

    QQmlApplicationEngine* qmlEngine = nullptr;
    vela::language::install(QStringLiteral("vela-calendar"), [&qmlEngine] {
        QGuiApplication::setApplicationDisplayName(QCoreApplication::translate("Calendar", "Calendar"));
        if (qmlEngine) {
            qmlEngine->retranslate();
        }
    });
    QGuiApplication::setApplicationDisplayName(QCoreApplication::translate("Calendar", "Calendar"));
    QGuiApplication::setOrganizationName(QStringLiteral("Vela"));
    QGuiApplication::setDesktopFileName(QStringLiteral("vela-calendar"));
    QIcon::setFallbackThemeName(QStringLiteral("hicolor"));
    QGuiApplication::setWindowIcon(QIcon::fromTheme(QStringLiteral("calendar")));

    Store store;
    QObject::connect(&server, &QLocalServer::newConnection, &store, [&server, &store] {
        while (QLocalSocket* socket = server.nextPendingConnection()) {
            QObject::connect(socket, &QLocalSocket::readyRead, socket, [socket, &store] {
                while (socket->canReadLine()) {
                    const QString line = QString::fromUtf8(socket->readLine()).trimmed();
                    if (line.startsWith(QLatin1String("open "))) emit store.importRequested(line.mid(5));
                    emit store.showRequested();
                }
            });
            QObject::connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);
        }
    });

    QQmlApplicationEngine engine;
    qmlEngine = &engine;
    Appearance* style = vela::controls::install(engine, Appearance::Mode::Apps);
    engine.addImageProvider(QStringLiteral("appicon"), new IconProvider(32));
    engine.rootContext()->setContextProperty(QStringLiteral("Cal"), &store);
    engine.rootContext()->setContextProperty(QStringLiteral("StartFile"), file);
    engine.loadFromModule("Vela.Calendar", "Main");
    if (engine.rootObjects().isEmpty()) {
        return 1;
    }

    QTimer::singleShot(0, &app, [style] { style->watch(); });
    return app.exec();
}
