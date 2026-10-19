// SPDX-FileCopyrightText: 2026 Brando Giuffrida
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

// The global shortcuts that apps asked for through the GlobalShortcuts portal
// (globalshortcuts.h), kept in ~/.config/vela/app-shortcuts.conf, one per line:
//
//   number|Super+Q|app id|shortcut id|description
//
// The compositor reads the number and the keys (compositor/src/shortcuts.c) and
// tells the shell when they are pressed and released; the shell owns the
// portal; Settings lists them to change or remove. A line with empty keys is a
// shortcut the app has but the user hasn't given a key yet.

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QList>
#include <QSaveFile>
#include <QString>
#include <QStringList>

#include "../../common/privatefile.h"

namespace vela {

struct AppShortcut {
    int number = 0;
    QString combo; // "Super+Q", empty: no key
    QString appId;
    QString id;
    QString description;
};

// Fields never hold the separator or a line break.
inline QString appShortcutField(QString text)
{
    text.replace(u'|', u'_').replace(u'\n', u' ').replace(u'\r', u' ');
    return text.trimmed();
}

inline QString appShortcutsPath()
{
    const QString config = qEnvironmentVariable("XDG_CONFIG_HOME");
    const QString base = config.startsWith(u'/') ? config : QDir::homePath() + QStringLiteral("/.config");
    return base + QStringLiteral("/vela/app-shortcuts.conf");
}

inline QList<AppShortcut> parseAppShortcuts(const QString& text)
{
    QList<AppShortcut> out;
    for (const QString& line : text.split(u'\n')) {
        if (line.isEmpty() || line.startsWith(u'#')) {
            continue;
        }
        const QStringList parts = line.split(u'|');
        bool ok = false;
        const int number = parts.value(0).toInt(&ok);
        if (parts.size() < 5 || !ok || number <= 0 || parts.at(2).isEmpty() || parts.at(3).isEmpty()) {
            continue;
        }
        // The description is last and may have been written with a bar by hand.
        out.append({ number, parts.at(1).trimmed(), parts.at(2), parts.at(3), parts.mid(4).join(u'|').trimmed() });
    }
    return out;
}

inline QString serializeAppShortcuts(const QList<AppShortcut>& shortcuts)
{
    QString text = QStringLiteral("# Global shortcuts that apps asked for (Settings > Keyboard shortcuts)\n");
    for (const AppShortcut& s : shortcuts) {
        text += QString::number(s.number) + u'|' + appShortcutField(s.combo) + u'|' + appShortcutField(s.appId) + u'|'
            + appShortcutField(s.id) + u'|' + appShortcutField(s.description) + u'\n';
    }
    return text;
}

inline QList<AppShortcut> loadAppShortcuts(const QString& path = appShortcutsPath())
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }
    return parseAppShortcuts(QString::fromUtf8(file.readAll()));
}

inline bool saveAppShortcuts(const QList<AppShortcut>& shortcuts, const QString& path = appShortcutsPath())
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile file(path);
    if (!openPrivateFile(file, QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }
    const QByteArray data = serializeAppShortcuts(shortcuts).toUtf8();
    if (file.write(data) != data.size()) {
        file.cancelWriting();
        return false;
    }
    return file.commit();
}

} // namespace vela
