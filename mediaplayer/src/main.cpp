// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// vela-mediaplayer: Vela's Media Player, the one of Windows 11's, playing
// with mpv.
//
//   vela-mediaplayer [video, music or folder]
//
// Built like Notepad: Qt Quick without Qt Quick Controls at startup.

#include "controls.h"
#include "iconprovider.h"
#include "language.h"
#include "mpvplayer.h"
#include "playlist.h"

#include <QDir>
#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QTimer>
#include <QUrl>
#include <QtQml/QQmlExtensionPlugin>

#include <clocale>

Q_IMPORT_QML_PLUGIN(Vela_ControlsPlugin)

int main(int argc, char* argv[])
{
    // Open comes from the portal (see notepad/src/main.cpp).
    qputenv("QT_QPA_PLATFORMTHEME", "xdgdesktopportal");
    // Crisp hinted text, as in the File Explorer (explorer/src/main.cpp).
    if (qEnvironmentVariableIsEmpty("QT_QUICK_DEFAULT_TEXT_RENDER_TYPE")) {
        QQuickWindow::setTextRenderType(QQuickWindow::NativeTextRendering);
    }
    QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL); // what mpv draws with
    QGuiApplication app(argc, argv);
    std::setlocale(LC_NUMERIC, "C"); // mpv reads its numbers with a point
    QGuiApplication::setApplicationName(QStringLiteral("vela-mediaplayer"));
    QQmlApplicationEngine* qmlEngine = nullptr;
    vela::language::install(QStringLiteral("vela-mediaplayer"), [&qmlEngine] {
        QGuiApplication::setApplicationDisplayName(QCoreApplication::translate("MediaPlayer", "Media Player"));
        if (qmlEngine) {
            qmlEngine->retranslate();
        }
    });
    QGuiApplication::setApplicationDisplayName(QCoreApplication::translate("MediaPlayer", "Media Player"));
    QGuiApplication::setOrganizationName(QStringLiteral("Vela"));
    QGuiApplication::setDesktopFileName(QStringLiteral("vela-mediaplayer"));
    QGuiApplication::setWindowIcon(QIcon::fromTheme(QStringLiteral("multimedia-video-player")));

    QIcon::setFallbackThemeName(QStringLiteral("hicolor"));

    qmlRegisterType<MpvPlayer>("Vela.MediaPlayer.Backend", 1, 0, "MpvPlayer");

    Playlist playlist;
    if (app.arguments().size() > 1) {
        playlist.open(QUrl::fromUserInput(app.arguments().at(1), QDir::currentPath(), QUrl::AssumeLocalFile));
    }

    QQmlApplicationEngine engine;
    qmlEngine = &engine;
    Appearance* style = vela::controls::install(engine, Appearance::Mode::Apps);
    engine.addImageProvider(QStringLiteral("appicon"), new IconProvider(32)); // the title bar's icon
    engine.rootContext()->setContextProperty(QStringLiteral("Playlist"), &playlist);
    engine.loadFromModule("Vela.MediaPlayer", "Main");
    if (engine.rootObjects().isEmpty()) {
        return 1;
    }

    // Following Settings live isn't needed for the first frame.
    QTimer::singleShot(0, &app, [style] { style->watch(); });
    return app.exec();
}
