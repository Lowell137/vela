// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// vela-videoeditor: Vela's Video Editor, the basics of Windows 11's
// (timeline.h tells how it fits together).
//
//   vela-videoeditor [project.velaedit]
//
// Built like the Media Player: Qt Quick on OpenGL, which mpv draws with.

#include "controls.h"
#include "exporter.h"
#include "iconprovider.h"
#include "language.h"
#include "mediapool.h"
#include "mpvplayer.h"
#include "timeline.h"

#include <QDir>
#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QTimer>
#include <QtQml/QQmlExtensionPlugin>

#include <clocale>

Q_IMPORT_QML_PLUGIN(Vela_ControlsPlugin)

int main(int argc, char* argv[])
{
    // Open and Save come from the portal (see notepad/src/main.cpp).
    qputenv("QT_QPA_PLATFORMTHEME", "xdgdesktopportal");
    if (qEnvironmentVariableIsEmpty("QT_QUICK_DEFAULT_TEXT_RENDER_TYPE")) {
        QQuickWindow::setTextRenderType(QQuickWindow::NativeTextRendering);
    }
    QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL); // what mpv draws with
    QGuiApplication app(argc, argv);
    std::setlocale(LC_NUMERIC, "C"); // mpv reads its numbers with a point
    QGuiApplication::setApplicationName(QStringLiteral("vela-videoeditor"));
    QQmlApplicationEngine* qmlEngine = nullptr;
    vela::language::install(QStringLiteral("vela-videoeditor"), [&qmlEngine] {
        QGuiApplication::setApplicationDisplayName(QCoreApplication::translate("VideoEditor", "Video Editor"));
        if (qmlEngine) {
            qmlEngine->retranslate();
        }
    });
    QGuiApplication::setApplicationDisplayName(QCoreApplication::translate("VideoEditor", "Video Editor"));
    QGuiApplication::setOrganizationName(QStringLiteral("Vela"));
    QGuiApplication::setDesktopFileName(QStringLiteral("vela-videoeditor"));
    QGuiApplication::setWindowIcon(QIcon::fromTheme(QStringLiteral("kdenlive")));
    QIcon::setFallbackThemeName(QStringLiteral("hicolor"));

    qmlRegisterType<MpvPlayer>("Vela.VideoEditor.Backend", 1, 0, "MpvPlayer");

    Timeline timeline;
    MediaPool media;
    Exporter exporter;

    QQmlApplicationEngine engine;
    qmlEngine = &engine;
    Appearance* style = vela::controls::install(engine, Appearance::Mode::Apps);
    engine.addImageProvider(QStringLiteral("appicon"), new IconProvider(32)); // the title bar's icon
    engine.rootContext()->setContextProperty(QStringLiteral("Project"), &timeline);
    engine.rootContext()->setContextProperty(QStringLiteral("Media"), &media);
    engine.rootContext()->setContextProperty(QStringLiteral("Exporter"), &exporter);
    const QStringList arguments = app.arguments();
    engine.rootContext()->setContextProperty(QStringLiteral("StartFile"),
        arguments.size() > 1 ? QDir::current().absoluteFilePath(arguments.at(1)) : QString());
    engine.loadFromModule("Vela.VideoEditor", "Main");
    if (engine.rootObjects().isEmpty()) {
        return 1;
    }

    QTimer::singleShot(0, &app, [style] { style->watch(); });
    return app.exec();
}
