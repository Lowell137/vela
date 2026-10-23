// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QObject>
#include <QStringList>
#include <QUrl>
#include <QVariantList>

#include <cstdio>

// The Explorer as a file chooser: `vela-files --pick request.json`. The shell's
// portal backend (org.freedesktop.impl.portal.FileChooser) starts it for the
// apps that ask for a file. The request:
//
//   { "mode": "open" | "save" | "savefiles", "title": "", "accept": "", "multiple": false,
//     "directory": false, "folder": "/start/here", "name": "suggested.txt", "names": ["a", "b"],
//     "filters": [{ "name": "Images", "globs": ["*.png", "*.jpg"] }], "filter": 0 }
//
// The answer is one line on stdout, { "uris": [...], "filter": 0 }, and exit
// code 0; no line (closed, Cancel) means nothing was chosen.
class Picker : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool active READ active CONSTANT)
    Q_PROPERTY(QString mode READ mode CONSTANT)
    Q_PROPERTY(QString title READ title CONSTANT)
    Q_PROPERTY(QString acceptLabel READ acceptLabel CONSTANT)
    Q_PROPERTY(bool multiple READ multiple CONSTANT)
    Q_PROPERTY(bool directory READ directory CONSTANT)
    Q_PROPERTY(QString startFolder READ startFolder CONSTANT)
    Q_PROPERTY(QString currentName READ currentName CONSTANT)
    Q_PROPERTY(QStringList names READ names CONSTANT)
    Q_PROPERTY(QVariantList filters READ filters CONSTANT)
    Q_PROPERTY(int currentFilter READ currentFilter CONSTANT)

public:
    bool load(const QString& path)
    {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            return false;
        }
        const QJsonObject o = QJsonDocument::fromJson(file.readAll()).object();
        m_mode = o.value(QStringLiteral("mode")).toString(QStringLiteral("open"));
        m_title = o.value(QStringLiteral("title")).toString();
        m_accept = o.value(QStringLiteral("accept")).toString();
        m_multiple = o.value(QStringLiteral("multiple")).toBool();
        m_directory = o.value(QStringLiteral("directory")).toBool();
        m_folder = o.value(QStringLiteral("folder")).toString();
        if (!QDir(m_folder).exists()) {
            m_folder = QDir::homePath();
        }
        m_name = o.value(QStringLiteral("name")).toString();
        for (const QJsonValue& v : o.value(QStringLiteral("names")).toArray()) {
            m_names.append(v.toString());
        }
        for (const QJsonValue& v : o.value(QStringLiteral("filters")).toArray()) {
            const QJsonObject f = v.toObject();
            QStringList globs;
            for (const QJsonValue& g : f.value(QStringLiteral("globs")).toArray()) {
                globs.append(g.toString());
            }
            m_filters.append(QVariantMap { { QStringLiteral("name"), f.value(QStringLiteral("name")).toString() },
                { QStringLiteral("globs"), globs } });
        }
        m_filter = o.value(QStringLiteral("filter")).toInt();
        m_active = true;
        return true;
    }

    bool active() const { return m_active; }
    QString mode() const { return m_mode; }
    QString title() const { return m_title; }
    QString acceptLabel() const { return m_accept; }
    bool multiple() const { return m_multiple; }
    bool directory() const { return m_directory; }
    QString startFolder() const { return m_folder; }
    QString currentName() const { return m_name; }
    QStringList names() const { return m_names; }
    QVariantList filters() const { return m_filters; }
    int currentFilter() const { return m_filter; }

    // The paths chosen (for "savefiles", the folder: each name goes in it).
    Q_INVOKABLE void accept(const QStringList& paths, int filter)
    {
        QJsonArray uris;
        if (m_mode == QLatin1String("savefiles") && !paths.isEmpty()) {
            for (const QString& name : m_names) {
                uris.append(QUrl::fromLocalFile(QDir(paths.first()).filePath(name)).toString());
            }
        } else {
            for (const QString& path : paths) {
                uris.append(QUrl::fromLocalFile(path).toString());
            }
        }
        const QByteArray line = QJsonDocument(QJsonObject { { QStringLiteral("uris"), uris }, { QStringLiteral("filter"), filter } })
                                    .toJson(QJsonDocument::Compact);
        std::fwrite(line.constData(), 1, size_t(line.size()), stdout);
        std::fputc('\n', stdout);
        std::fflush(stdout);
        QCoreApplication::exit(0);
    }
    Q_INVOKABLE void cancel() { QCoreApplication::exit(1); }
    // What `path` is now: "dir", "file" or "" (nothing).
    Q_INVOKABLE QString kind(const QString& path) const
    {
        const QFileInfo info(path);
        return info.isDir() ? QStringLiteral("dir") : info.exists() ? QStringLiteral("file") : QString();
    }

private:
    bool m_active = false;
    QString m_mode;
    QString m_title;
    QString m_accept;
    bool m_multiple = false;
    bool m_directory = false;
    QString m_folder;
    QString m_name;
    QStringList m_names;
    QVariantList m_filters;
    int m_filter = 0;
};
