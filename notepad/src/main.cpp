// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// vela-notepad: Vela's Notepad, like Windows 11's.
//
//   vela-notepad [file...]   (each file in its own tab)
//   vela-notepad --new-window   (empty: without the last session's files)
//
// Built like the File Explorer: Qt Quick without Qt Quick Controls at startup,
// and what the first frame doesn't need (the file picker, Find) is loaded when
// it's first used.

#include "app.h"
#include "controls.h"
#include "document.h"
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
    // Open and Save come from the portal, like every other app's in Vela.
    // Without widgets the KDE platform theme has no file dialog and Qt would
    // draw its own; this theme adds the portal's dialogs on top of the
    // desktop's theme and changes nothing else.
    qputenv("QT_QPA_PLATFORMTHEME", "xdgdesktopportal");
    // Crisp hinted text, as in the File Explorer (explorer/src/main.cpp).
    if (qEnvironmentVariableIsEmpty("QT_QUICK_DEFAULT_TEXT_RENDER_TYPE")) {
        QQuickWindow::setTextRenderType(QQuickWindow::NativeTextRendering);
    }
    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("vela-notepad"));
    QQmlApplicationEngine* qmlEngine = nullptr;
    vela::language::install(QStringLiteral("vela-notepad"), [&qmlEngine] {
        QGuiApplication::setApplicationDisplayName(QCoreApplication::translate("Notepad", "Notepad"));
        if (qmlEngine) {
            qmlEngine->retranslate();
        }
    });
    QGuiApplication::setApplicationDisplayName(QCoreApplication::translate("Notepad", "Notepad"));
    QGuiApplication::setOrganizationName(QStringLiteral("Vela"));
    QGuiApplication::setDesktopFileName(QStringLiteral("vela-notepad"));
    QGuiApplication::setWindowIcon(QIcon::fromTheme(QStringLiteral("accessories-text-editor")));

    QIcon::setFallbackThemeName(QStringLiteral("hicolor"));

    QList<QUrl> files;
    const QStringList args = app.arguments().mid(1);
    const bool fresh = args.contains(QLatin1String("--new-window"));
    for (const QString& arg : args) {
        if (arg == QLatin1String("--new-window")) {
            continue;
        }
        files.append(QUrl::fromUserInput(arg, QDir::currentPath(), QUrl::AssumeLocalFile));
    }

    qmlRegisterType<Document>("Vela.Notepad.Backend", 1, 0, "Document");

    App notepad;

    QQmlApplicationEngine engine;
    qmlEngine = &engine;
    Appearance* style = vela::controls::install(engine, Appearance::Mode::Apps);
    engine.addImageProvider(QStringLiteral("appicon"), new IconProvider(32)); // Notepad's icon, in the title bar
    QQmlContext* context = engine.rootContext();
    context->setContextProperty(QStringLiteral("App"), &notepad);
    context->setContextProperty(QStringLiteral("StartFiles"), QVariant::fromValue(files));
    context->setContextProperty(QStringLiteral("FreshWindow"), fresh);
    engine.loadFromModule("Vela.Notepad", "Main");
    if (engine.rootObjects().isEmpty()) {
        return 1;
    }

    // Following Settings live isn't needed for the first frame.
    QTimer::singleShot(0, &app, [style] { style->watch(); });
    return app.exec();
}
