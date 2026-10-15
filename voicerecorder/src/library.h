// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

// The recordings folder: the list, and what is done to a file in it
// (rename, trash, trim and export through ffmpeg, its waveform).

#include <QDir>
#include <QFileSystemWatcher>
#include <QObject>
#include <QTimer>
#include <QVariantList>

class Library : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList recordings READ recordings NOTIFY recordingsChanged) // [{name, base, path, modified, seconds}]
    Q_PROPERTY(QVariantList waveform READ waveform NOTIFY waveformChanged) // peaks 0..1 of waveformPath
    Q_PROPERTY(QString waveformPath READ waveformPath NOTIFY waveformChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged) // ffmpeg is working

public:
    explicit Library(const QDir& folder, QObject* parent = nullptr);

    QVariantList recordings() const { return m_recordings; }
    QVariantList waveform() const { return m_waveform; }
    QString waveformPath() const { return m_waveformPath; }
    bool busy() const { return m_busy; }
    QDir folder() const { return m_folder; }

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void loadWaveform(const QString& path);
    // "" or the reason it can't be done.
    Q_INVOKABLE QString rename(const QString& path, const QString& base);
    Q_INVOKABLE bool remove(const QString& path); // to the trash
    // ffmpeg, in the background: `done(path, error)` with the new file.
    Q_INVOKABLE void trim(const QString& path, double start, double end, bool inPlace);
    Q_INVOKABLE void exportAs(const QString& path, const QString& extension);
    Q_INVOKABLE void showInFolder(const QString& path);
    Q_INVOKABLE void copyPath(const QString& path);

signals:
    void recordingsChanged();
    void waveformChanged();
    void busyChanged();
    void done(const QString& path, const QString& error);

private:
    void convert(const QStringList& arguments, const QString& tmp, const QString& target, bool replace);

    QDir m_folder;
    QFileSystemWatcher m_watcher;
    QTimer m_later; // one refresh for a burst of changes
    QVariantList m_recordings;
    QVariantList m_waveform;
    QString m_waveformPath;
    int m_generation = 0;
    bool m_busy = false;
};
