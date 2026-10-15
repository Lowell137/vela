// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

// The files added to the project (`MediaPool` in QML): each probed with
// ffprobe in the background, with a thumbnail (a frame at a tenth of a
// video) and, for pictures, a still video for the preview's EDL, all under
// ~/.cache/Vela/videoeditor/. Also the few file chores QML can't do.

#include <QObject>
#include <QUrl>
#include <QVariantList>

class MediaPool : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList items READ items NOTIFY itemsChanged)
    Q_PROPERTY(QString cache READ cache CONSTANT)
    Q_PROPERTY(int pending READ pending NOTIFY itemsChanged) // files still being probed

public:
    explicit MediaPool(QObject* parent = nullptr);

    QVariantList items() const { return m_items; }
    QString cache() const { return m_cache; }
    int pending() const { return m_pending; }

    // Adds files (those already there are skipped); `place`: put each on the
    // timeline once probed, at `at` (see placed()).
    Q_INVOKABLE void add(const QList<QUrl>& urls, bool place = false, double at = -1);
    Q_INVOKABLE void setFiles(const QStringList& paths); // a project's, in order
    Q_INVOKABLE void remove(const QString& path);
    Q_INVOKABLE QStringList paths() const;
    Q_INVOKABLE QVariantMap item(const QString& path) const;

    Q_INVOKABLE bool exists(const QString& path) const;
    Q_INVOKABLE bool newer(const QString& path, const QString& than) const;
    Q_INVOKABLE void removeFile(const QString& path) const;
    Q_INVOKABLE void showInFolder(const QString& path) const;

signals:
    void itemsChanged();
    void placed(const QVariantMap& item, double at);
    void stillReady(); // the preview can reload: a picture's still video is there
    void failed(const QString& name);

private:
    void probe(const QString& path, bool place, double at);
    void finish(const QString& path, const QVariantMap& item, bool place, double at);
    void thumbnail(const QString& path, const QVariantMap& item);
    void makeStill(const QString& source);

    QVariantList m_items;
    QString m_cache;
    int m_pending = 0;
};
