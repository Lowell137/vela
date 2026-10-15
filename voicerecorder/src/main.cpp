// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// vela-voicerecorder: Vela's Sound Recorder, the one of Windows 11's.
//
//   vela-voicerecorder
//
// Records through PipeWire (pw-record) into ~/Music/Recordings and plays
// with libmpv. Built like the Task Manager: Qt Quick without Controls.

#include "audioplayer.h"
#include "controls.h"
#include "iconprovider.h"
#include "language.h"
#include "library.h"
#include "recorder.h"

#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QStandardPaths>
#include <QTimer>
#include <QtQml/QQmlExtensionPlugin>

#include <clocale>

Q_IMPORT_QML_PLUGIN(Vela_ControlsPlugin)

int main(int argc, char* argv[])
{
    qputenv("QT_QPA_PLATFORMTHEME", "xdgdesktopportal");
    if (qEnvironmentVariableIsEmpty("QT_QUICK_DEFAULT_TEXT_RENDER_TYPE")) {
        QQuickWindow::setTextRenderType(QQuickWindow::NativeTextRendering);
    }
    QGuiApplication app(argc, argv);
    std::setlocale(LC_NUMERIC, "C"); // mpv reads its numbers with a point
    QGuiApplication::setApplicationName(QStringLiteral("vela-voicerecorder"));
    QQmlApplicationEngine* qmlEngine = nullptr;
    vela::language::install(QStringLiteral("vela-voicerecorder"), [&qmlEngine] {
        QGuiApplication::setApplicationDisplayName(QCoreApplication::translate("VoiceRecorder", "Sound Recorder"));
        if (qmlEngine) {
            qmlEngine->retranslate();
        }
    });
    QGuiApplication::setApplicationDisplayName(QCoreApplication::translate("VoiceRecorder", "Sound Recorder"));
    QGuiApplication::setOrganizationName(QStringLiteral("Vela"));
    QGuiApplication::setDesktopFileName(QStringLiteral("vela-voicerecorder"));
    QGuiApplication::setWindowIcon(QIcon::fromTheme(QStringLiteral("audio-input-microphone")));
    QIcon::setFallbackThemeName(QStringLiteral("hicolor"));

    const QDir folder(QStandardPaths::writableLocation(QStandardPaths::MusicLocation) + QStringLiteral("/Recordings"));
    Library library(folder);
    Recorder recorder(folder);
    AudioPlayer player;

    QQmlApplicationEngine engine;
    qmlEngine = &engine;
    Appearance* style = vela::controls::install(engine, Appearance::Mode::Apps);
    engine.addImageProvider(QStringLiteral("appicon"), new IconProvider(32));
    QQmlContext* context = engine.rootContext();
    context->setContextProperty(QStringLiteral("Lib"), &library);
    context->setContextProperty(QStringLiteral("Rec"), &recorder);
    context->setContextProperty(QStringLiteral("Player"), &player);
    engine.loadFromModule("Vela.VoiceRecorder", "Main");
    if (engine.rootObjects().isEmpty()) {
        return 1;
    }
    recorder.refreshDevices();

    QTimer::singleShot(0, &app, [style] { style->watch(); });
    return app.exec();
}
