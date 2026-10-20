// SPDX-FileCopyrightText: 2026 Brando Giuffrida
// SPDX-License-Identifier: GPL-3.0-or-later

#include "appearance.h"

#include "defaults.h"
#include "iconprovider.h"
#include "mica.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>

static QString themeFilePath();

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
    reload();
}

void Appearance::watch()
{
    // Whoever writes the files replaces them: the directories are watched too.
    const QString shellConfig = QSettings(QStringLiteral("Vela"), QStringLiteral("vela-shell")).fileName();
    // vela.conf: the icon theme (icon-theme=).
    const QString velaConfig = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + QStringLiteral("/vela/vela.conf");
    for (const QString& path : { shellConfig, wallpaperTintCachePath(), velaConfig, themeFilePath() }) {
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

// theme=<name> of vela.conf, and where its file is: the user's themes first,
// then the data directories' (a package or a nested run's XDG_DATA_DIRS).
static QString themeFilePath()
{
    QString name;
    QFile conf(QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + QStringLiteral("/vela/vela.conf"));
    if (conf.open(QIODevice::ReadOnly | QIODevice::Text)) {
        while (!conf.atEnd()) {
            const QByteArray line = conf.readLine().trimmed();
            if (line.startsWith("theme=")) {
                name = QString::fromUtf8(line.mid(6)).trimmed();
            }
        }
    }
    // A name is a file name: nothing that leads elsewhere.
    if (name.isEmpty() || name.contains(u'/') || name.startsWith(u'.')) {
        return {};
    }
    const QString relative = QStringLiteral("vela/themes/") + name + QStringLiteral(".conf");
    const QString user = QStandardPaths::locate(QStandardPaths::GenericConfigLocation, relative);
    return user.isEmpty() ? QStandardPaths::locate(QStandardPaths::GenericDataLocation, relative) : user;
}

// key=value lines, # comments. A value that is a color becomes a QColor
// (#RRGGBB or #AARRGGBB), a number a double; anything else is ignored.
static QVariantMap loadThemeFile(const QString& path)
{
    QVariantMap map;
    QFile file(path);
    if (path.isEmpty() || !file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return map;
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
        }
    }
    return map;
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
    const QString themeFile = themeFilePath();
    const QVariantMap theme = loadThemeFile(themeFile);
    if (m_watching && !themeFile.isEmpty() && !m_watcher.files().contains(themeFile)) {
        m_watcher.addPath(themeFile);
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
