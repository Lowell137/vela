// SPDX-FileCopyrightText: 2026 Brando Giuffrida
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QFileSystemWatcher>
#include <QObject>
#include <QTimer>

// The user's choices the shell follows live: they're in
// ~/.config/Vela/vela-shell.conf, written by the Settings app (or the shell
// itself) and reread here as soon as the file changes. From QML:
// Config.wallpaper, Config.taskbarAlignment... (light or dark, accent and
// icons reach QML through Vela.Controls' Style.)
class Config : public QObject {
    Q_OBJECT
    // The wallpaper: a chosen image, otherwise Vela's.
    Q_PROPERTY(QString wallpaper READ wallpaper NOTIFY wallpaperChanged)
    // "center" or "left", like "Taskbar alignment".
    Q_PROPERTY(QString taskbarAlignment READ taskbarAlignment NOTIFY taskbarChanged)
    // "classic" (one bar), "islands" (three floating surfaces) or "dock" (a
    // macOS-like dock with a top panel; only with the taskbar at the bottom).
    Q_PROPERTY(QString taskbarStyle READ taskbarStyle NOTIFY taskbarChanged)
    Q_PROPERTY(bool dock READ dock NOTIFY taskbarChanged)
    Q_PROPERTY(QString taskbarPosition READ taskbarPosition NOTIFY taskbarChanged)
    Q_PROPERTY(bool taskbarTop READ taskbarTop NOTIFY taskbarChanged)
    Q_PROPERTY(bool taskbarLeft READ taskbarLeft NOTIFY taskbarChanged)
    Q_PROPERTY(bool taskbarRight READ taskbarRight NOTIFY taskbarChanged)
    Q_PROPERTY(bool taskbarVertical READ taskbarVertical NOTIFY taskbarChanged)
    // "End task" in the taskbar buttons' menu.
    Q_PROPERTY(bool endTask READ endTask NOTIFY taskbarChanged)
    // The Task View button on the taskbar.
    Q_PROPERTY(bool taskView READ taskView NOTIFY taskbarChanged)
    Q_PROPERTY(bool taskbarWindowBounce READ taskbarWindowBounce NOTIFY taskbarChanged)
    // The apps' tray icons behind the "^" button, in a flyout, like Windows 11.
    Q_PROPERTY(bool trayOverflow READ trayOverflow NOTIFY taskbarChanged)
    // The taskbar on all outputs (like Windows) or only on the main one.
    Q_PROPERTY(bool taskbarAllScreens READ taskbarAllScreens NOTIFY taskbarChanged)
    // Auto-hide: the taskbar slides away and comes back at the screen's edge.
    Q_PROPERTY(bool taskbarAutoHide READ taskbarAutoHide NOTIFY taskbarChanged)
    // "always" (one button per app) or "never" (one per window).
    Q_PROPERTY(QString taskbarCombine READ taskbarCombine NOTIFY taskbarChanged)
    // Clickable right-hand taskbar corner that toggles the desktop.
    Q_PROPERTY(bool showDesktopCorner READ showDesktopCorner NOTIFY taskbarChanged)
    // 0-100: how much more solid than the theme the Start menu's background is.
    Q_PROPERTY(int startSolid READ startSolid NOTIFY taskbarChanged)
    // The weather flyout's system cards (graphics card, processor, memory, disk).
    Q_PROPERTY(bool weatherSystemInfo READ weatherSystemInfo NOTIFY taskbarChanged)
    // Clipboard history (Win+V).
    Q_PROPERTY(bool clipboardHistory READ clipboardHistory NOTIFY clipboardChanged)
    // "Choose your mode": "dark" or "light", for the shell (taskbar, menus,
    // panels) and for apps (their windows and title bars).
    Q_PROPERTY(QString shellTheme READ shellTheme NOTIFY themeChanged)
    Q_PROPERTY(QString appTheme READ appTheme NOTIFY themeChanged)

public:
    explicit Config(QObject* parent = nullptr);

    QString wallpaper() const { return m_wallpaper; }
    QString taskbarAlignment() const { return m_taskbarAlignment; }
    bool dock() const { return m_taskbarStyle == QLatin1String("dock") && m_taskbarPosition == QLatin1String("bottom"); }
    QString taskbarStyle() const { return m_taskbarStyle; }
    QString taskbarPosition() const { return m_taskbarPosition; }
    bool taskbarTop() const { return m_taskbarPosition == QLatin1String("top"); }
    bool taskbarLeft() const { return m_taskbarPosition == QLatin1String("left"); }
    bool taskbarRight() const { return m_taskbarPosition == QLatin1String("right"); }
    bool taskbarVertical() const { return taskbarLeft() || taskbarRight(); }
    bool endTask() const { return m_endTask; }
    bool taskView() const { return m_taskView; }
    bool taskbarWindowBounce() const { return m_taskbarWindowBounce; }
    bool trayOverflow() const { return m_trayOverflow; }
    bool taskbarAllScreens() const { return m_taskbarAllScreens; }
    bool taskbarAutoHide() const { return m_taskbarAutoHide; }
    QString taskbarCombine() const { return m_taskbarCombine; }
    // The weather widget (taskbar/weather*): off by default, no location is
    // ever guessed. Changes are signalled with taskbarChanged.
    bool weather() const { return m_weather; }
    QString weatherCity() const { return m_weatherCity; }
    QString weatherLat() const { return m_weatherLat; }
    QString weatherLon() const { return m_weatherLon; }
    QString weatherUnit() const { return m_weatherUnit; }
    bool weatherSystemInfo() const { return m_weatherSystemInfo; }
    bool showDesktopCorner() const { return m_showDesktopCorner; }
    int startSolid() const { return m_startSolid; }
    bool clipboardHistory() const { return m_clipboardHistory; }
    bool doNotDisturb() const { return m_doNotDisturb; }
    QString shellTheme() const { return m_shellTheme; }
    QString appTheme() const { return m_appTheme; }

signals:
    void wallpaperChanged();
    void taskbarChanged();
    void doNotDisturbChanged(bool on);
    void themeChanged();
    void clipboardChanged();

private:
    void reload();
    void watch();

    QFileSystemWatcher m_watcher;
    QTimer m_debounce;
    QString m_path;
    QString m_wallpaper;
    QString m_taskbarAlignment;
    QString m_taskbarStyle = QStringLiteral("classic");
    QString m_taskbarPosition = QStringLiteral("bottom");
    bool m_endTask = true;
    bool m_taskView = true;
    bool m_taskbarWindowBounce = false;
    bool m_trayOverflow = true;
    bool m_taskbarAllScreens = true;
    bool m_taskbarAutoHide = false;
    QString m_taskbarCombine = QStringLiteral("always");
    bool m_weather = true;
    bool m_weatherSystemInfo = true;
    QString m_weatherCity, m_weatherLat, m_weatherLon;
    QString m_weatherUnit = QStringLiteral("c");
    bool m_showDesktopCorner = true;
    int m_startSolid = 0;
    bool m_clipboardHistory = false;
    bool m_doNotDisturb = false;
    QString m_shellTheme = QStringLiteral("dark");
    QString m_appTheme = QStringLiteral("dark");
};
