// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

// The videos and music of the opened one's folder (`Playlist` in QML), in the
// order a file manager lists them: what Previous and Next go through.

#include <QList>
#include <QObject>
#include <QString>
#include <QUrl>

class Playlist : public QObject {
    Q_OBJECT
    Q_PROPERTY(QList<QUrl> files READ files NOTIFY filesChanged)
    Q_PROPERTY(int index READ index WRITE setIndex NOTIFY indexChanged)
    Q_PROPERTY(QUrl current READ current NOTIFY indexChanged)
    Q_PROPERTY(QString name READ name NOTIFY indexChanged)
    Q_PROPERTY(QString folder READ folder NOTIFY indexChanged)

public:
    using QObject::QObject;

    QList<QUrl> files() const { return m_files; }
    int index() const { return m_index; }
    void setIndex(int index);
    QUrl current() const { return m_files.value(m_index); }
    QString name() const { return current().fileName(); }
    QString folder() const;

    // A file, with its folder's others, or a folder, from its first one.
    Q_INVOKABLE void open(const QUrl& url);
    Q_INVOKABLE QString filter() const; // "*.mp4 *.mkv ...", for the Open dialog

signals:
    void filesChanged();
    void indexChanged();

private:
    QList<QUrl> m_files;
    int m_index = -1;
};
