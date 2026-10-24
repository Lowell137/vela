// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QIcon>
#include <QMutex>
#include <QMutexLocker>
#include <QPixmap>
#include <QQuickImageProvider>
#include <QSettings>
#include <QUrl>

#include <algorithm>
#include <cstdlib>

// An icon of a given theme, whatever theme is in use: Settings > Icons shows
// each theme with a few of its icons.
//
//   Image { source: "image://themeicon/" + (Theme.light ? "l" : "d") + "/" + theme + "/" + name }
//
// A theme "X" in the dark mode is "X-dark" (or "X-Dark") when that exists. The
// theme's index.theme is read here, because Qt only draws from the theme in
// use: the directories nearest to the size, the theme's own first and then
// those it inherits.
class IconThemePreviewProvider : public QQuickImageProvider {
public:
    IconThemePreviewProvider()
        : QQuickImageProvider(QQuickImageProvider::Pixmap)
    {
    }

    QPixmap requestPixmap(const QString& id, QSize* size, const QSize& requestedSize) override
    {
        const QStringList parts = id.split(u'/');
        const int extent = requestedSize.isValid() ? std::max({ requestedSize.width(), requestedSize.height(), 8 }) : 48;
        QPixmap pixmap;
        if (parts.size() >= 3) {
            QString theme = QUrl::fromPercentEncoding(parts.at(1).toUtf8());
            const QString name = QUrl::fromPercentEncoding(parts.mid(2).join(u'/').toUtf8());
            if (parts.at(0) == QLatin1String("d")) {
                for (const QString& dark : { theme + QStringLiteral("-dark"), theme + QStringLiteral("-Dark") }) {
                    if (!themeRoots(dark).isEmpty()) {
                        theme = dark;
                        break;
                    }
                }
            }
            const QString path = find(theme, name, extent, 0);
            if (!path.isEmpty()) {
                pixmap = QIcon(path).pixmap(QSize(extent, extent), 1.0);
            }
        }
        if (size) {
            *size = pixmap.size();
        }
        return pixmap;
    }

private:
    struct Dir {
        QString path;
        int size = 0;
        int min = 0;
        int max = 0;
        bool scalable = false;
        bool fixedOnly = false;
    };
    struct Theme {
        QStringList roots; // the theme's folder in each icon directory that has it
        QStringList inherits;
        QList<Dir> dirs;
    };

    // The folders of `theme` in the icon directories, the first one with its index.theme.
    static QStringList themeRoots(const QString& theme)
    {
        QStringList roots;
        if (theme.isEmpty() || theme.contains(u'/') || theme == QLatin1String("..")) {
            return roots;
        }
        for (const QString& base : QIcon::themeSearchPaths()) {
            if (QFileInfo(base + u'/' + theme).isDir()) {
                roots.append(base + u'/' + theme);
            }
        }
        return roots;
    }

    Theme load(const QString& theme)
    {
        QMutexLocker lock(&m_mutex);
        auto it = m_themes.find(theme);
        if (it != m_themes.end()) {
            return it.value();
        }
        Theme info;
        info.roots = themeRoots(theme);
        for (const QString& root : std::as_const(info.roots)) {
            if (!QFileInfo::exists(root + QStringLiteral("/index.theme"))) {
                continue;
            }
            QSettings index(root + QStringLiteral("/index.theme"), QSettings::IniFormat);
            index.beginGroup(QStringLiteral("Icon Theme"));
            info.inherits = index.value(QStringLiteral("Inherits")).toStringList();
            const QStringList names = index.value(QStringLiteral("Directories")).toStringList();
            index.endGroup();
            for (const QString& name : names) {
                const QString group = name.trimmed();
                Dir dir;
                dir.path = group;
                dir.size = index.value(group + QStringLiteral("/Size"), 0).toInt();
                const QString type = index.value(group + QStringLiteral("/Type"), QStringLiteral("Threshold")).toString();
                dir.scalable = type == QLatin1String("Scalable");
                dir.min = index.value(group + QStringLiteral("/MinSize"), dir.size).toInt();
                dir.max = index.value(group + QStringLiteral("/MaxSize"), dir.size).toInt();
                dir.fixedOnly = type == QLatin1String("Fixed");
                if (!dir.path.isEmpty()) {
                    info.dirs.append(dir);
                }
            }
            break; // the first index.theme is the theme's
        }
        return m_themes.insert(theme, info).value();
    }

    // How far a directory is from the wanted size (0: exact, or scalable in range).
    static int distance(const Dir& dir, int extent)
    {
        if (dir.scalable) {
            return extent >= dir.min && extent <= dir.max ? 0 : std::min(std::abs(extent - dir.min), std::abs(extent - dir.max));
        }
        return std::abs(dir.size - extent);
    }

    QString find(const QString& theme, const QString& name, int extent, int depth)
    {
        if (depth > 6) {
            return {};
        }
        const Theme info = load(theme);
        QList<const Dir*> dirs;
        for (const Dir& dir : info.dirs) {
            dirs.append(&dir);
        }
        std::stable_sort(dirs.begin(), dirs.end(), [&](const Dir* a, const Dir* b) {
            const int da = distance(*a, extent);
            const int db = distance(*b, extent);
            return da != db ? da < db : a->scalable && !b->scalable;
        });
        static const QStringList suffixes { QStringLiteral(".svg"), QStringLiteral(".png"), QStringLiteral(".svgz") };
        for (const Dir* dir : std::as_const(dirs)) {
            for (const QString& root : info.roots) {
                for (const QString& suffix : suffixes) {
                    const QString file = root + u'/' + dir->path + u'/' + name + suffix;
                    if (QFileInfo::exists(file)) {
                        return file;
                    }
                }
            }
        }
        for (const QString& parent : info.inherits) {
            const QString found = find(parent.trimmed(), name, extent, depth + 1);
            if (!found.isEmpty()) {
                return found;
            }
        }
        return {};
    }

    QMutex m_mutex;
    QHash<QString, Theme> m_themes;
};
