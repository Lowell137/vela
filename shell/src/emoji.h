// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

// The emoji of the Win+. panel (`Emoji` in QML), from shell/data/emoji.tsv:
// by group, found by name in English, Turkish and Italian, and the ones used
// last. Read when the panel first asks: the shell starts without them.

#include <QFile>
#include <QLocale>
#include <QObject>
#include <QSettings>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

class Emoji : public QObject {
    Q_OBJECT
    // What was typed from the panel, most recent first.
    Q_PROPERTY(QStringList recent READ recent NOTIFY recentChanged)

public:
    using QObject::QObject;

    QStringList recent() const { return QSettings().value(QStringLiteral("clipboard/recentEmoji")).toStringList(); }

    // [{emoji, name}] of a group (0: smileys ... 8: flags), or found by `text`
    // in every group when it isn't empty.
    Q_INVOKABLE QVariantList list(int group, const QString& text = {})
    {
        load();
        const QStringList words = text.toLower().split(u' ', Qt::SkipEmptyParts);
        QVariantList result;
        for (const Entry& entry : std::as_const(m_entries)) {
            if (words.isEmpty() ? entry.group != group : !std::all_of(words.begin(), words.end(),
                    [&entry](const QString& word) { return entry.words.contains(word); })) {
                continue;
            }
            result.append(QVariantMap { { QStringLiteral("emoji"), entry.emoji }, { QStringLiteral("name"), entry.name } });
            if (!words.isEmpty() && result.size() >= 200) {
                break;
            }
        }
        return result;
    }

    Q_INVOKABLE void used(const QString& text)
    {
        QStringList list = recent();
        list.removeAll(text);
        list.prepend(text);
        QSettings().setValue(QStringLiteral("clipboard/recentEmoji"), list.mid(0, 40));
        emit recentChanged();
    }

signals:
    void recentChanged();

private:
    struct Entry {
        QString emoji;
        int group = 0;
        QString name;
        QString words;
    };

    void load()
    {
        QFile file(QStringLiteral(":/data/emoji.tsv"));
        if (!m_entries.isEmpty() || !file.open(QIODevice::ReadOnly)) {
            return;
        }
        // The name in the session's language: the columns are en, tr, it.
        const QString language = QLocale().name().left(2);
        const int column = language == QLatin1String("tr") ? 3 : language == QLatin1String("it") ? 4 : 2;
        const QStringList lines = QString::fromUtf8(file.readAll()).split(u'\n', Qt::SkipEmptyParts);
        for (const QString& line : lines) {
            const QStringList fields = line.split(u'\t');
            if (fields.size() >= 6) {
                m_entries.append({ fields[0], fields[1].toInt(), fields[column].isEmpty() ? fields[2] : fields[column], fields[5] });
            }
        }
    }

    QList<Entry> m_entries;
};
