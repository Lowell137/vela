// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// vela-weather: Vela's Weather, the one of Windows 11's (Open-Meteo, no key).
//
//   vela-weather
//
// The place is never guessed from the IP: the user searches a city.

#include "controls.h"
#include "iconprovider.h"
#include "language.h"
#include "weathermodel.h"

#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QSettings>
#include <QStandardPaths>
#include <QTimer>
#include <QtQml/QQmlExtensionPlugin>

Q_IMPORT_QML_PLUGIN(Vela_ControlsPlugin)

int main(int argc, char* argv[])
{
    qputenv("QT_QPA_PLATFORMTHEME", "xdgdesktopportal");
    if (qEnvironmentVariableIsEmpty("QT_QUICK_DEFAULT_TEXT_RENDER_TYPE")) {
        QQuickWindow::setTextRenderType(QQuickWindow::NativeTextRendering);
    }
    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("vela-weather"));
    QQmlApplicationEngine* qmlEngine = nullptr;
    vela::language::install(QStringLiteral("vela-weather"), [&qmlEngine] {
        QGuiApplication::setApplicationDisplayName(QCoreApplication::translate("Weather", "Weather"));
        if (qmlEngine) {
            qmlEngine->retranslate();
        }
    });
    QGuiApplication::setApplicationDisplayName(QCoreApplication::translate("Weather", "Weather"));
    QGuiApplication::setOrganizationName(QStringLiteral("Vela"));
    QGuiApplication::setDesktopFileName(QStringLiteral("vela-weather"));
    QGuiApplication::setWindowIcon(QIcon::fromTheme(QStringLiteral("weather-clear")));
    QIcon::setFallbackThemeName(QStringLiteral("hicolor"));

    QSettings settings;
    // The taskbar widget's file (organization Vela, application vela-shell):
    // only read, once, for the first place.
    const QString shellConfig = QSettings(QSettings::NativeFormat, QSettings::UserScope, QStringLiteral("Vela"), QStringLiteral("vela-shell")).fileName();
    const QString data = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/Vela/weather");
    const QString cache = QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation) + QStringLiteral("/Vela/weather");
    WeatherModel weather(data + QStringLiteral("/places.json"), cache, shellConfig, &settings);

    QQmlApplicationEngine engine;
    qmlEngine = &engine;
    Appearance* style = vela::controls::install(engine, Appearance::Mode::Apps);
    engine.addImageProvider(QStringLiteral("appicon"), new IconProvider(64));
    engine.rootContext()->setContextProperty(QStringLiteral("Weather"), &weather);
    engine.loadFromModule("Vela.Weather", "Main");
    if (engine.rootObjects().isEmpty()) {
        return 1;
    }

    // The cache is on screen already; the network answers after the first frame.
    QTimer::singleShot(0, &app, [style, &weather] {
        style->watch();
        weather.refresh();
    });
    return app.exec();
}
