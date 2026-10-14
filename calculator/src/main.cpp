// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// vela-calculator: Vela's Calculator, like Windows 11's (Standard, with the
// history). Built like Notepad: Qt Quick without Qt Quick Controls.

#include "controls.h"
#include "engine.h"
#include "iconprovider.h"
#include "language.h"

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
    // Crisp hinted text, as in the File Explorer (explorer/src/main.cpp).
    if (qEnvironmentVariableIsEmpty("QT_QUICK_DEFAULT_TEXT_RENDER_TYPE")) {
        QQuickWindow::setTextRenderType(QQuickWindow::NativeTextRendering);
    }
    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("vela-calculator"));
    QQmlApplicationEngine* qmlEngine = nullptr;
    vela::language::install(QStringLiteral("vela-calculator"), [&qmlEngine] {
        QGuiApplication::setApplicationDisplayName(QCoreApplication::translate("Calculator", "Calculator"));
        if (qmlEngine) {
            qmlEngine->retranslate();
        }
    });
    QGuiApplication::setApplicationDisplayName(QCoreApplication::translate("Calculator", "Calculator"));
    QGuiApplication::setOrganizationName(QStringLiteral("Vela"));
    QGuiApplication::setDesktopFileName(QStringLiteral("vela-calculator"));
    QGuiApplication::setWindowIcon(QIcon::fromTheme(QStringLiteral("accessories-calculator")));

    QIcon::setFallbackThemeName(QStringLiteral("hicolor"));

    Engine calc;

    QQmlApplicationEngine engine;
    qmlEngine = &engine;
    Appearance* style = vela::controls::install(engine, Appearance::Mode::Apps);
    engine.addImageProvider(QStringLiteral("appicon"), new IconProvider(32)); // the Calculator's icon, in the title bar
    engine.rootContext()->setContextProperty(QStringLiteral("Calc"), &calc);
    engine.loadFromModule("Vela.Calculator", "Main");
    if (engine.rootObjects().isEmpty()) {
        return 1;
    }

    // Following Settings live isn't needed for the first frame.
    QTimer::singleShot(0, &app, [style] { style->watch(); });
    return app.exec();
}
