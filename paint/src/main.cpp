// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// vela-paint: Vela's Paint, the one of Windows 11's.
//
//   vela-paint [image]
//
// Built like Notepad: Qt Quick without Qt Quick Controls at startup.

#include "canvas.h"
#include "controls.h"
#include "iconprovider.h"
#include "language.h"

#include <QDir>
#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QTimer>
#include <QUrl>
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
    QGuiApplication::setApplicationName(QStringLiteral("vela-paint"));
    QQmlApplicationEngine* qmlEngine = nullptr;
    vela::language::install(QStringLiteral("vela-paint"), [&qmlEngine] {
        QGuiApplication::setApplicationDisplayName(QCoreApplication::translate("Paint", "Paint"));
        if (qmlEngine) {
            qmlEngine->retranslate();
        }
    });
    QGuiApplication::setApplicationDisplayName(QCoreApplication::translate("Paint", "Paint"));
    QGuiApplication::setOrganizationName(QStringLiteral("Vela"));
    QGuiApplication::setDesktopFileName(QStringLiteral("vela-paint"));
    QGuiApplication::setWindowIcon(QIcon::fromTheme(QStringLiteral("applications-graphics")));

    QIcon::setFallbackThemeName(QStringLiteral("hicolor"));

    qmlRegisterType<PaintCanvas>("Vela.Paint.Backend", 1, 0, "PaintCanvas");

    // The picture to open, if there is one.
    const QUrl start = app.arguments().size() > 1
        ? QUrl::fromUserInput(app.arguments().at(1), QDir::currentPath(), QUrl::AssumeLocalFile) : QUrl();

    QQmlApplicationEngine engine;
    qmlEngine = &engine;
    Appearance* style = vela::controls::install(engine, Appearance::Mode::Apps);
    engine.addImageProvider(QStringLiteral("appicon"), new IconProvider(32)); // Paint's icon, in the title bar
    engine.rootContext()->setContextProperty(QStringLiteral("StartFile"), start);
    engine.loadFromModule("Vela.Paint", "Main");
    if (engine.rootObjects().isEmpty()) {
        return 1;
    }

    // Following Settings live isn't needed for the first frame.
    QTimer::singleShot(0, &app, [style] { style->watch(); });
    return app.exec();
}
