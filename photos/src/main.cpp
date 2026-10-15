// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// vela-photos: Vela's Photos, the viewer of Windows 11's.
//
//   vela-photos [image or folder]
//
// Built like Notepad: Qt Quick without Qt Quick Controls at startup.

#include "album.h"
#include "formats.h"
#include "controls.h"
#include "editview.h"
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
    QGuiApplication::setApplicationName(QStringLiteral("vela-photos"));
    QQmlApplicationEngine* qmlEngine = nullptr;
    vela::language::install(QStringLiteral("vela-photos"), [&qmlEngine] {
        QGuiApplication::setApplicationDisplayName(QCoreApplication::translate("Photos", "Photos"));
        if (qmlEngine) {
            qmlEngine->retranslate();
        }
    });
    QGuiApplication::setApplicationDisplayName(QCoreApplication::translate("Photos", "Photos"));
    QGuiApplication::setOrganizationName(QStringLiteral("Vela"));
    QGuiApplication::setDesktopFileName(QStringLiteral("vela-photos"));
    QGuiApplication::setWindowIcon(QIcon::fromTheme(QStringLiteral("image-viewer")));

    QIcon::setFallbackThemeName(QStringLiteral("hicolor"));

    qmlRegisterType<EditView>("Vela.Photos.Backend", 1, 0, "EditView");

    Album album;
    if (app.arguments().size() > 1) {
        album.open(QUrl::fromUserInput(app.arguments().at(1), QDir::currentPath(), QUrl::AssumeLocalFile));
    }

    QQmlApplicationEngine engine;
    qmlEngine = &engine;
    Appearance* style = vela::controls::install(engine, Appearance::Mode::Apps);
    engine.addImageProvider(QStringLiteral("appicon"), new IconProvider(32)); // Photos' icon, in the title bar
    engine.addImageProvider(QStringLiteral("photo"), new FormatImages(&album)); // AVIF, HEIC, RAW...
    engine.rootContext()->setContextProperty(QStringLiteral("Album"), &album);
    engine.loadFromModule("Vela.Photos", "Main");
    if (engine.rootObjects().isEmpty()) {
        return 1;
    }

    // Following Settings live isn't needed for the first frame.
    QTimer::singleShot(0, &app, [style] { style->watch(); });
    return app.exec();
}
