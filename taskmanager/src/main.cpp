// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// vela-taskmanager: Vela's Task Manager, the one of Windows 11's.
//
//   vela-taskmanager
//
// Built like Notepad: Qt Quick without Qt Quick Controls at startup.

#include "controls.h"
#include "iconprovider.h"
#include "graph.h"
#include "language.h"
#include "system.h"

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
    // Open comes from the portal (see notepad/src/main.cpp).
    qputenv("QT_QPA_PLATFORMTHEME", "xdgdesktopportal");
    // Crisp hinted text, as in the File Explorer (explorer/src/main.cpp).
    if (qEnvironmentVariableIsEmpty("QT_QUICK_DEFAULT_TEXT_RENDER_TYPE")) {
        QQuickWindow::setTextRenderType(QQuickWindow::NativeTextRendering);
    }
    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("vela-taskmanager"));
    QQmlApplicationEngine* qmlEngine = nullptr;
    vela::language::install(QStringLiteral("vela-taskmanager"), [&qmlEngine] {
        QGuiApplication::setApplicationDisplayName(QCoreApplication::translate("TaskManager", "Task Manager"));
        if (qmlEngine) {
            qmlEngine->retranslate();
        }
    });
    QGuiApplication::setApplicationDisplayName(QCoreApplication::translate("TaskManager", "Task Manager"));
    QGuiApplication::setOrganizationName(QStringLiteral("Vela"));
    QGuiApplication::setDesktopFileName(QStringLiteral("vela-taskmanager"));
    QGuiApplication::setWindowIcon(QIcon::fromTheme(QStringLiteral("utilities-system-monitor")));

    QIcon::setFallbackThemeName(QStringLiteral("hicolor"));

    qmlRegisterType<Graph>("Vela.TaskManager.Backend", 1, 0, "Graph");
    qmlRegisterUncreatableType<RowModel>("Vela.TaskManager.Backend", 1, 0, "RowModel", QStringLiteral("from Sys"));

    System system;

    QQmlApplicationEngine engine;
    qmlEngine = &engine;
    Appearance* style = vela::controls::install(engine, Appearance::Mode::Apps);
    engine.addImageProvider(QStringLiteral("appicon"), new IconProvider(32)); // its icon, in the title bar
    engine.rootContext()->setContextProperty(QStringLiteral("Sys"), &system);
    engine.loadFromModule("Vela.TaskManager", "Main");
    if (engine.rootObjects().isEmpty()) {
        return 1;
    }

    // Following Settings live isn't needed for the first frame.
    QTimer::singleShot(0, &app, [style] { style->watch(); });
    return app.exec();
}
