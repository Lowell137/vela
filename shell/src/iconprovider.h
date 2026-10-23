// SPDX-FileCopyrightText: 2026 Brando Giuffrida
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QIcon>
#include <QPalette>
#include <QPainter>
#include <QPixmap>
#include <QPixmapCache>
#include <QQuickImageProvider>
#include <QStandardPaths>
#include <QUrl>

#include <algorithm>

// icon-theme= of ~/.config/vela/vela.conf (Settings > Personalization >
// Icons), empty if missing: then the automatic theme (KDE's, or Breeze).
inline QString configuredIconTheme()
{
    QFile file(QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + QStringLiteral("/vela/vela.conf"));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }
    QString value;
    while (!file.atEnd()) {
        const QString line = QString::fromUtf8(file.readLine()).trimmed();
        if (line.startsWith(QLatin1String("icon-theme="))) {
            value = line.mid(11).trimmed();
        }
    }
    return value;
}

// A theme installed in one of Qt's icon directories (a name, not a path).
inline bool iconThemeExists(const QString& name)
{
    if (name.isEmpty() || name.contains(u'/') || name == QLatin1String("..")) {
        return false;
    }
    const QStringList paths = QIcon::themeSearchPaths();
    return std::any_of(paths.begin(), paths.end(),
        [&](const QString& dir) { return QFileInfo::exists(dir + u'/' + name + QStringLiteral("/index.theme")); });
}

// The icon theme matching the light or dark mode: the "-dark" variant
// (breeze-dark) on dark, the normal one (breeze) on light, starting from the
// theme chosen in Settings (icon-theme=, if installed) or else in KDE; also
// "X-Dark" for "X". Call at startup and when the mode changes. With KDE's
// platform the icons come from KIconLoader, which colors the monochrome ones
// with the app's palette: that follows the mode too.
inline void applyIconTheme(bool light)
{
    QPalette palette = QGuiApplication::palette();
    const QColor text = light ? QColor(0x1b, 0x1b, 0x1b) : QColor(0xf3, 0xf3, 0xf6);
    const QColor background = light ? QColor(0xf3, 0xf3, 0xf3) : QColor(0x20, 0x20, 0x20);
    for (const QPalette::ColorGroup group : { QPalette::Active, QPalette::Inactive, QPalette::Disabled }) {
        palette.setColor(group, QPalette::WindowText, text);
        palette.setColor(group, QPalette::Text, text);
        palette.setColor(group, QPalette::ButtonText, text);
        palette.setColor(group, QPalette::Window, background);
        palette.setColor(group, QPalette::Base, background);
        palette.setColor(group, QPalette::Button, background);
    }
    const bool changed = palette != QGuiApplication::palette();
    if (changed) {
        QGuiApplication::setPalette(palette);
    }

    // Without a choice: Vela's own theme, installed with it (VELA_ICON_THEME
    // overrides), else KDE's, else breeze.
    static const QString automatic = [] {
        if (qEnvironmentVariableIsEmpty("VELA_ICON_THEME") && iconThemeExists(QStringLiteral("vela"))) {
            return QStringLiteral("vela");
        }
        QString theme = QIcon::themeName();
        if (theme.isEmpty() || theme == QLatin1String("hicolor")) {
            theme = qEnvironmentVariable("VELA_ICON_THEME", QStringLiteral("breeze-dark"));
        }
        return theme;
    }();
    const QString configured = configuredIconTheme();
    // A theme no longer installed: the automatic one.
    const QString base = iconThemeExists(configured) ? configured : automatic;
    QString plain = base;
    if (plain.endsWith(QLatin1String("-dark"), Qt::CaseInsensitive)) {
        plain.chop(5);
    }
    QString chosen = base;
    if (light && plain != base && iconThemeExists(plain)) {
        chosen = plain;
    } else if (!light) {
        for (const QString& dark : { plain + QStringLiteral("-dark"), plain + QStringLiteral("-Dark") }) {
            if (dark == base || iconThemeExists(dark)) {
                chosen = dark;
                break;
            }
        }
    }
    const bool themeChanged = QIcon::themeName() != chosen;
    if (themeChanged || changed) {
        QIcon::setThemeName(chosen);
    }
    if (changed || themeChanged) {
        // Already loaded icons have the old colors: they're reloaded.
        // KIconLoader (KDE's platform) empties its cache when it gets the
        // signal KDE's icon module sends.
        QPixmapCache::clear();
        QDBusConnection::sessionBus().send(QDBusMessage::createSignal(QStringLiteral("/KIconLoader"),
            QStringLiteral("org.kde.KIconLoader"), QStringLiteral("iconChanged"))
            << 0);
    }
}

// Makes the system theme's icons available to QML:
//   Image { source: "image://icon/" + encodeURIComponent("org.kde.dolphin")
//           sourceSize: Qt.size(width, height) }
// Accepts both theme icon names and absolute paths. The sourceSize is the
// logical size it's shown at: Qt multiplies it by the output scale and here
// it's drawn at exactly those pixels.
//
// Two modes: "icon" uses the theme's drawing for that size (command icons,
// monochrome and crisp when small); "fileicon" always uses the colored
// drawing of at least `designSize` pixels, carefully scaled down (files,
// folders and places, colored even when small, like in Windows).
//
// A prefix before the name: "l/" and "d/" say for which mode (light or dark)
// the icon is asked, so changing mode changes the address and QML reloads
// it; "w/" wants it white (on the accent). It can be followed by "~N/" (N
// changes with the icon theme, for the same reason).
class IconProvider : public QQuickImageProvider {
public:
    explicit IconProvider(int designSize = 0)
        : QQuickImageProvider(QQuickImageProvider::Pixmap)
        , m_designSize(designSize)
    {
    }

    QPixmap requestPixmap(const QString& id, QSize* size, const QSize& requestedSize) override
    {
        const int extent = requestedSize.isValid()
            ? std::max({ requestedSize.width(), requestedSize.height(), 8 })
            : 64;
        QString name = QUrl::fromPercentEncoding(id.toUtf8());
        bool white = false;
        if (id.size() > 2 && id[1] == u'/' && (id[0] == u'l' || id[0] == u'd' || id[0] == u'w')) {
            white = id[0] == u'w';
            QString rest = id.mid(2);
            // "~N/": the icon theme's generation (Appearance::iconMode).
            if (rest.startsWith(u'~') && rest.indexOf(u'/') > 0) {
                rest = rest.mid(rest.indexOf(u'/') + 1);
            }
            name = QUrl::fromPercentEncoding(rest.toUtf8());
        }

        // An app's own drawings (:/fluent, see common/fluent) come before the
        // theme's, in the text's color of the mode they're asked for.
        const QString own = QStringLiteral(":/fluent/") + name + QStringLiteral(".svg");
        if (name.size() != id.size() && QFile::exists(own)) {
            QPixmap pixmap = QIcon(own).pixmap(QSize(extent, extent), 1.0);
            QPainter painter(&pixmap);
            painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
            painter.fillRect(pixmap.rect(), id[0] == u'l' ? QColor(0x1b, 0x1b, 0x1b)
                : id[0] == u'd' ? QColor(0xf3, 0xf3, 0xf6) : QColor(Qt::white));
            if (size) {
                *size = pixmap.size();
            }
            return pixmap;
        }

        QIcon icon = name.startsWith(u'/') ? QIcon(name) : QIcon::fromTheme(name);
        if (icon.isNull()) {
            icon = QIcon::fromTheme(QStringLiteral("application-x-executable"));
        }
        // Exactly at the requested pixels: Qt has already multiplied the
        // sourceSize (the icon's logical size) by the output scale. Without
        // ratio 1, QIcon would multiply again by the output's integer scale
        // and the icon, shrunk on screen, would get blurry (especially at
        // fractional scales such as 125%).
        QPixmap pixmap;
        if (extent < m_designSize) {
            pixmap = icon.pixmap(QSize(m_designSize, m_designSize), 1.0)
                         .scaled(extent, extent, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        } else {
            pixmap = icon.pixmap(QSize(extent, extent), 1.0);
        }
        if (white && !pixmap.isNull()) {
            QPainter painter(&pixmap);
            painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
            painter.fillRect(pixmap.rect(), Qt::white);
        }
        if (size) {
            *size = pixmap.size();
        }
        return pixmap;
    }

private:
    int m_designSize;
};
