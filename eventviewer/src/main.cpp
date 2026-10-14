// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// vela-eventviewer: Vela's Event Viewer, the one of Windows 11's, reading
// systemd's journal.
//
//   vela-eventviewer
//
// Built like the Task Manager: Qt Quick without Qt Quick Controls at startup.

#include "controls.h"
#include "iconprovider.h"
#include "language.h"
#include "reader.h"

#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QTimer>
#include <QtQml/QQmlExtensionPlugin>

Q_IMPORT_QML_PLUGIN(Vela_ControlsPlugin)

int main(int argc, char* argv[])
{
    // The save dialog comes from the portal (see notepad/src/main.cpp).
    qputenv("QT_QPA_PLATFORMTHEME", "xdgdesktopportal");
    if (qEnvironmentVariableIsEmpty("QT_QUICK_DEFAULT_TEXT_RENDER_TYPE")) {
        QQuickWindow::setTextRenderType(QQuickWindow::NativeTextRendering);
    }
    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("vela-eventviewer"));
    QQmlApplicationEngine* qmlEngine = nullptr;
    vela::language::install(QStringLiteral("vela-eventviewer"), [&qmlEngine] {
        QGuiApplication::setApplicationDisplayName(QCoreApplication::translate("EventViewer", "Event Viewer"));
        if (qmlEngine) {
            qmlEngine->retranslate();
        }
    });
    QGuiApplication::setApplicationDisplayName(QCoreApplication::translate("EventViewer", "Event Viewer"));
    QGuiApplication::setOrganizationName(QStringLiteral("Vela"));
    QGuiApplication::setDesktopFileName(QStringLiteral("vela-eventviewer"));
    QGuiApplication::setWindowIcon(QIcon::fromTheme(QStringLiteral("utilities-log-viewer")));

    QIcon::setFallbackThemeName(QStringLiteral("hicolor"));

    // Declared before the engine: journalctl is stopped after the window is gone.
    Reader reader;

    QQmlApplicationEngine engine;
    qmlEngine = &engine;
    Appearance* style = vela::controls::install(engine, Appearance::Mode::Apps);
    engine.addImageProvider(QStringLiteral("appicon"), new IconProvider(32));
    engine.rootContext()->setContextProperty(QStringLiteral("Journal"), &reader);
    engine.loadFromModule("Vela.EventViewer", "Main");
    if (engine.rootObjects().isEmpty()) {
        return 1;
    }

    QTimer::singleShot(0, &app, [style] { style->watch(); });
    return app.exec();
}
