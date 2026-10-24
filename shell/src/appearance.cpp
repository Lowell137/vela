// SPDX-FileCopyrightText: 2026 Brando Giuffrida
// SPDX-License-Identifier: GPL-3.0-or-later

#include "appearance.h"

#include "defaults.h"
#include "iconprovider.h"
#include "mica.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QSettings>
#include <QStandardPaths>
#include <QUrl>

static QStringList themeFilePaths(const QString& onlySurface = QString());

Appearance::Appearance(QObject* parent, Mode mode)
    : QObject(parent)
    , m_mode(mode)
{
    m_debounce.setSingleShot(true);
    m_debounce.setInterval(100);
    connect(&m_debounce, &QTimer::timeout, this, [this] {
        watch();
        reload();
    });
    if (mode == Mode::Shell) {
        // Light by day, dark by night (Settings > Colors): Settings does the
        // switch, so the GTK/KDE/icons follow like a click would. shortcut:
        // it starts a process a minute while the schedule is on, move it into
        // the shell if it ever shows in a profile.
        m_themeSchedule.setInterval(60 * 1000);
        connect(&m_themeSchedule, &QTimer::timeout, this, [] {
            QFile conf(QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + QStringLiteral("/vela/vela.conf"));
            if (conf.open(QIODevice::ReadOnly | QIODevice::Text) && (QByteArray("\n") + conf.readAll()).contains("\ntheme-schedule=yes")) {
                QProcess::startDetached(QStringLiteral("vela-settings"), { QStringLiteral("--apply-theme-schedule") });
            }
        });
        m_themeSchedule.start();
    }
    reload();
}

void Appearance::watch()
{
    // Whoever writes the files replaces them: the directories are watched too.
    const QString shellConfig = QSettings(QStringLiteral("Vela"), QStringLiteral("vela-shell")).fileName();
    // vela.conf: the icon theme (icon-theme=).
    const QString velaConfig = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + QStringLiteral("/vela/vela.conf");
    for (const QString& path : QStringList { shellConfig, wallpaperTintCachePath(), velaConfig } + themeFilePaths()) {
        if (path.isEmpty()) {
            continue;
        }
        const QFileInfo info(path);
        if (info.exists() && !m_watcher.files().contains(path)) {
            m_watcher.addPath(path);
        }
        if (info.dir().exists() && !m_watcher.directories().contains(info.absolutePath())) {
            m_watcher.addPath(info.absolutePath());
        }
    }
    if (!m_watching) {
        m_watching = true;
        connect(&m_watcher, &QFileSystemWatcher::fileChanged, &m_debounce, qOverload<>(&QTimer::start));
        connect(&m_watcher, &QFileSystemWatcher::directoryChanged, &m_debounce, qOverload<>(&QTimer::start));
    }
}

// theme=<name> of vela.conf, and the surfaces' own styles (taskbar-style=,
// startmenu-style=, notifications-style=, like Windhawk's three stylers) in
// themes/taskbar/, themes/startmenu/, themes/notifications/: where their
// files are, the user's first, then the data directories' (a package or a
// nested run's XDG_DATA_DIRS). A style's values win over the theme's.
// onlySurface ("taskbar/"...): the theme and just that surface's style.
static QStringList themeFilePaths(const QString& onlySurface)
{
    static const std::pair<QByteArray, QString> keys[] = {
        { "theme=", QString() },
        { "taskbar-style=", QStringLiteral("taskbar/") },
        { "startmenu-style=", QStringLiteral("startmenu/") },
        { "notifications-style=", QStringLiteral("notifications/") },
    };
    QString names[std::size(keys)];
    QFile conf(QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + QStringLiteral("/vela/vela.conf"));
    if (conf.open(QIODevice::ReadOnly | QIODevice::Text)) {
        while (!conf.atEnd()) {
            const QByteArray line = conf.readLine().trimmed();
            for (size_t i = 0; i < std::size(keys); ++i) {
                if (line.startsWith(keys[i].first)) {
                    names[i] = QString::fromUtf8(line.mid(keys[i].first.size())).trimmed();
                }
            }
        }
    }
    QStringList paths;
    for (size_t i = 0; i < std::size(keys); ++i) {
        if (!onlySurface.isEmpty() && !keys[i].second.isEmpty() && keys[i].second != onlySurface) {
            continue;
        }
        // A name is a file name: nothing that leads elsewhere.
        if (names[i].isEmpty() || names[i].contains(u'/') || names[i].startsWith(u'.')) {
            continue;
        }
        const QString relative = QStringLiteral("vela/themes/") + keys[i].second + names[i] + QStringLiteral(".conf");
        const QString user = QStandardPaths::locate(QStandardPaths::GenericConfigLocation, relative);
        const QString path = user.isEmpty() ? QStandardPaths::locate(QStandardPaths::GenericDataLocation, relative) : user;
        if (!path.isEmpty()) {
            paths.append(path);
        }
    }
    return paths;
}

// key=value lines, # comments. A value that is a color becomes a QColor
// (#RRGGBB or #AARRGGBB), a number a double, "asset:<file>" the URL of a picture
// of themes/assets/; anything else is ignored.
static void loadThemeFile(const QString& path, QVariantMap& map)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return;
    }
    while (!file.atEnd()) {
        const QString line = QString::fromUtf8(file.readLine()).trimmed();
        const qsizetype equals = line.indexOf(u'=');
        if (line.startsWith(u'#') || equals <= 0) {
            continue;
        }
        const QString key = line.left(equals).trimmed();
        const QString value = line.mid(equals + 1).trimmed();
        bool number = false;
        const double d = value.toDouble(&number);
        if (number) {
            map.insert(key, d);
        } else if (QColor color(value); color.isValid()) {
            map.insert(key, color);
        } else if (value.startsWith(QLatin1String("asset:"))) {
            // A picture of a theme (themes/assets/<name>): its URL, if it is installed.
            const QString name = value.mid(6).trimmed();
            if (name.isEmpty() || name.startsWith(u'/') || name.contains(QLatin1String(".."))) {
                continue;
            }
            const QString relative = QStringLiteral("vela/themes/assets/") + name;
            QString file = QStandardPaths::locate(QStandardPaths::GenericConfigLocation, relative);
            if (file.isEmpty()) {
                file = QStandardPaths::locate(QStandardPaths::GenericDataLocation, relative);
            }
            if (!file.isEmpty()) {
                map.insert(key, QUrl::fromLocalFile(file).toString());
            }
        }
    }
}

// cursor-theme= and cursor-size= of vela.conf to the apps the shell starts,
// as the compositor does for its own (it exported them when the shell started).
// shortcut: keys removed later keep the last values here until the shell restarts.
static void applyCursorEnvironment()
{
    QFile file(QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + QStringLiteral("/vela/vela.conf"));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return;
    }
    while (!file.atEnd()) {
        const QByteArray line = file.readLine().trimmed();
        if ((line.startsWith("cursor-theme=") || line.startsWith("cursor-size=")) && !line.endsWith('=')) {
            const qsizetype equals = line.indexOf('=');
            qputenv(line.startsWith("cursor-theme=") ? "XCURSOR_THEME" : "XCURSOR_SIZE", line.mid(equals + 1).trimmed());
        }
    }
}

QVariantMap Appearance::themeFor(const QString& surface) const
{
    QVariantMap map;
    for (const QString& path : themeFilePaths(surface)) {
        loadThemeFile(path, map);
    }
    return map;
}

void Appearance::reload()
{
    if (m_mode == Mode::Shell) {
        applyCursorEnvironment();
    }
    QSettings shell(QStringLiteral("Vela"), QStringLiteral("vela-shell"));
    shell.sync();
    const QString key = m_mode == Mode::Shell ? QStringLiteral("appearance/shellTheme") : QStringLiteral("appearance/appTheme");
    const bool light = shell.value(key).toString() == QLatin1String("light");
    QColor accent(shell.value(QStringLiteral("appearance/accent")).toString());
    if (!accent.isValid()) {
        accent = vela::defaults::accent();
    }
    const QColor tint = cachedWallpaperTint();
    const QColor mica = micaFromTint(tint, true, light);
    const QColor micaInactive = micaFromTint(tint, false, light);
    const QString iconTheme = configuredIconTheme();
    QVariantMap theme;
    for (const QString& themeFile : themeFilePaths()) {
        loadThemeFile(themeFile, theme);
        if (m_watching && !m_watcher.files().contains(themeFile)) {
            m_watcher.addPath(themeFile);
        }
    }
    // taskbar-backdrop= of vela.conf (Settings > Themes): the user's, over the theme's.
    QFile velaConf(QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + QStringLiteral("/vela/vela.conf"));
    if (velaConf.open(QIODevice::ReadOnly | QIODevice::Text)) {
        // A yes/no of vela.conf as the theme's key of the same name.
        const auto flagLine = [&theme](const QByteArray& line, const char* key) {
            const QByteArray prefix = QByteArray(key) + '=';
            if (!line.startsWith(prefix)) {
                return;
            }
            const QByteArray value = line.mid(prefix.size()).trimmed();
            theme.insert(QString::fromLatin1(key), value == "yes" || value == "1" ? 1.0 : 0.0);
        };
        while (!velaConf.atEnd()) {
            const QByteArray line = velaConf.readLine().trimmed();
            // taskbar-opacity= (percent of the theme's fill, Settings > Themes).
            if (line.startsWith("taskbar-opacity=")) {
                theme.insert(QStringLiteral("user-taskbar-opacity"), line.mid(16).toInt() / 100.0);
            }
            flagLine(line, "taskbar-backdrop");
            flagLine(line, "quicksettings-slider-press-zoom");
        }
    }
    if (light == m_light && accent == m_accent && mica == m_mica && micaInactive == m_micaInactive
        && iconTheme == m_iconTheme && theme == m_theme) {
        return;
    }
    const bool first = !m_accent.isValid();
    if (iconTheme != m_iconTheme && !first) {
        ++m_iconGeneration;
    }
    m_iconTheme = iconTheme;
    m_theme = theme;
    m_light = light;
    m_accent = accent;
    m_mica = mica;
    m_micaInactive = micaInactive;
    applyIconTheme(light); // before QML asks for the icons
    QString mode = light ? QStringLiteral("l/") : QStringLiteral("d/");
    if (m_iconGeneration > 0) {
        mode += u'~' + QString::number(m_iconGeneration) + u'/';
    }
    if (first) {
        m_iconMode = mode;
        return;
    }
    emit changed();
    // The image://icon prefix a moment later, when the icon caches
    // (KIconLoader's too, which gets the D-Bus signal) are empty: only then
    // QML asks for the icons again.
    QTimer::singleShot(300, this, [this, mode] {
        if (mode != m_iconMode) {
            m_iconMode = mode;
            emit iconModeChanged();
        }
    });
}
