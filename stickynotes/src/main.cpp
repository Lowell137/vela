// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// vela-stickynotes: Vela's Sticky Notes, the one of Windows 11's.
//
//   vela-stickynotes
//
// Built like Notepad: Qt Quick without Qt Quick Controls at startup.

#include "controls.h"
#include "iconprovider.h"
#include "instance.h"
#include "language.h"
#include "notestore.h"

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
    if (qEnvironmentVariableIsEmpty("QT_QUICK_DEFAULT_TEXT_RENDER_TYPE")) {
        QQuickWindow::setTextRenderType(QQuickWindow::NativeTextRendering);
    }
    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("vela-stickynotes"));
    QQmlApplicationEngine* qmlEngine = nullptr;
    vela::language::install(QStringLiteral("vela-stickynotes"), [&qmlEngine] {
        QGuiApplication::setApplicationDisplayName(QCoreApplication::translate("StickyNotes", "Sticky Notes"));
        if (qmlEngine) {
            qmlEngine->retranslate();
        }
    });
    QGuiApplication::setApplicationDisplayName(QCoreApplication::translate("StickyNotes", "Sticky Notes"));
    QGuiApplication::setOrganizationName(QStringLiteral("Vela"));
    QGuiApplication::setDesktopFileName(QStringLiteral("vela-stickynotes"));
    QGuiApplication::setWindowIcon(QIcon::fromTheme(QStringLiteral("knotes")));
    QIcon::setFallbackThemeName(QStringLiteral("hicolor"));

    Instance instance;
    if (!instance.acquire()) {
        return 0;
    }

    NoteStore store(NoteStore::defaultPath());
    QObject::connect(&app, &QCoreApplication::aboutToQuit, &store, &NoteStore::flush);

    QQmlApplicationEngine engine;
    qmlEngine = &engine;
    Appearance* style = vela::controls::install(engine, Appearance::Mode::Apps);
    engine.addImageProvider(QStringLiteral("appicon"), new IconProvider(32));
    engine.rootContext()->setContextProperty(QStringLiteral("Notes"), &store);
    engine.rootContext()->setContextProperty(QStringLiteral("Instance"), &instance);
    engine.loadFromModule("Vela.StickyNotes", "Main");
    if (engine.rootObjects().isEmpty()) {
        return 1;
    }

    QTimer::singleShot(0, &app, [style] { style->watch(); });
    return app.exec();
}
