// SPDX-FileCopyrightText: 2026 Brando Giuffrida
// SPDX-License-Identifier: GPL-3.0-or-later

#include "config.h"

#include "defaults.h"

#include <QDir>
#include <QFileInfo>
#include <QSettings>

Config::Config(QObject* parent)
    : QObject(parent)
    , m_path(QSettings().fileName())
{
    // Whoever writes the file replaces it (QSaveFile): the directory is
    // watched too, and the file is watched again on every change.
    m_debounce.setSingleShot(true);
    m_debounce.setInterval(100);
    connect(&m_debounce, &QTimer::timeout, this, [this] {
        watch();
        reload();
    });
    connect(&m_watcher, &QFileSystemWatcher::fileChanged, &m_debounce, qOverload<>(&QTimer::start));
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, &m_debounce, qOverload<>(&QTimer::start));
    watch();
    reload();
}

void Config::watch()
{
    const QFileInfo info(m_path);
    if (info.exists() && !m_watcher.files().contains(m_path)) {
        m_watcher.addPath(m_path);
    }
    if (info.dir().exists() && !m_watcher.directories().contains(info.absolutePath())) {
        m_watcher.addPath(info.absolutePath());
    }
}

void Config::reload()
{
    QSettings settings;
    settings.sync(); // rereads what other processes wrote

    // VELA_WALLPAPER (for tests) takes precedence over the saved choice.
    QString wallpaper = qEnvironmentVariable("VELA_WALLPAPER");
    if (wallpaper.isEmpty()) {
        wallpaper = settings.value(QStringLiteral("appearance/wallpaper")).toString();
    }
    if (wallpaper.isEmpty() || (!wallpaper.startsWith(u':') && !QFileInfo::exists(wallpaper))) {
        wallpaper = vela::defaults::wallpaper();
    }
    if (wallpaper != m_wallpaper) {
        m_wallpaper = wallpaper;
        emit wallpaperChanged();
    }

    const QString alignment = settings.value(QStringLiteral("taskbar/alignment")).toString() == QLatin1String("left")
        ? QStringLiteral("left")
        : QStringLiteral("center");
    QString position = settings.value(QStringLiteral("taskbar/position")).toString();
    if (position != QLatin1String("top") && position != QLatin1String("left") && position != QLatin1String("right")) {
        position = QStringLiteral("bottom");
    }
    const bool endTask = settings.value(QStringLiteral("taskbar/endTask"), true).toBool();
    const bool taskView = settings.value(QStringLiteral("taskbar/taskView"), true).toBool();
    const bool trayOverflow = settings.value(QStringLiteral("taskbar/trayOverflow"), true).toBool();
    const bool allScreens = settings.value(QStringLiteral("taskbar/allScreens"), true).toBool();
    const bool autoHide = settings.value(QStringLiteral("taskbar/autoHide"), false).toBool();
    const QString combine = settings.value(QStringLiteral("taskbar/combine")).toString() == QLatin1String("never")
        ? QStringLiteral("never")
        : QStringLiteral("always");
    // weather=on|off ("true"/"false" too: a QSettings bool would read "off" as true).
    const QString weatherValue = settings.value(QStringLiteral("taskbar/weather")).toString().toLower();
    const bool weather = weatherValue == QLatin1String("on") || weatherValue == QLatin1String("true") || weatherValue == QLatin1String("1");
    const QString weatherCity = settings.value(QStringLiteral("taskbar/weather-city")).toString();
    const QString weatherLat = settings.value(QStringLiteral("taskbar/weather-lat")).toString();
    const QString weatherLon = settings.value(QStringLiteral("taskbar/weather-lon")).toString();
    const QString weatherUnit = settings.value(QStringLiteral("taskbar/weather-unit")).toString().toLower() == QLatin1String("f")
        ? QStringLiteral("f")
        : QStringLiteral("c");
    const bool weatherChanged = weather != m_weather || weatherCity != m_weatherCity || weatherLat != m_weatherLat
        || weatherLon != m_weatherLon || weatherUnit != m_weatherUnit;
    m_weather = weather;
    m_weatherCity = weatherCity;
    m_weatherLat = weatherLat;
    m_weatherLon = weatherLon;
    m_weatherUnit = weatherUnit;
    if (weatherChanged || autoHide != m_taskbarAutoHide || combine != m_taskbarCombine || alignment != m_taskbarAlignment || position != m_taskbarPosition || endTask != m_endTask || taskView != m_taskView
        || trayOverflow != m_trayOverflow || allScreens != m_taskbarAllScreens) {
        m_taskbarAutoHide = autoHide;
        m_taskbarCombine = combine;
    const bool showDesktopCorner = settings.value(QStringLiteral("taskbar/showDesktopCorner"), true).toBool();
    if (alignment != m_taskbarAlignment || endTask != m_endTask || taskView != m_taskView
        || allScreens != m_taskbarAllScreens || showDesktopCorner != m_showDesktopCorner) {
        m_taskbarAlignment = alignment;
        m_taskbarPosition = position;
        m_endTask = endTask;
        m_taskView = taskView;
        m_trayOverflow = trayOverflow;
        m_taskbarAllScreens = allScreens;
        m_showDesktopCorner = showDesktopCorner;
        emit taskbarChanged();
    }

    auto mode = [&](const char* key) {
        return settings.value(QLatin1String(key)).toString() == QLatin1String("light") ? QStringLiteral("light")
                                                                                       : QStringLiteral("dark");
    };
    const QString shellTheme = mode("appearance/shellTheme");
    const QString appTheme = mode("appearance/appTheme");
    if (shellTheme != m_shellTheme || appTheme != m_appTheme) {
        m_shellTheme = shellTheme;
        m_appTheme = appTheme;
        emit themeChanged();
    }

    const bool clipboard = settings.value(QStringLiteral("clipboard/history"), false).toBool();
    if (clipboard != m_clipboardHistory) {
        m_clipboardHistory = clipboard;
        emit clipboardChanged();
    }

    const bool dnd = settings.value(QStringLiteral("notifications/doNotDisturb"), false).toBool();
    if (dnd != m_doNotDisturb) {
        m_doNotDisturb = dnd;
        emit doNotDisturbChanged(dnd);
    }
}
