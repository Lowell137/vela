// SPDX-FileCopyrightText: 2026 Brando Giuffrida
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QObject>
#include <QProcess>
#include <QString>
#include <QVariantList>

// System > Storage, like Windows' Storage: the volumes with their fill
// levels, and the temporary files a person can clean up by hand. Only what
// the user asks for is deleted (the cache, the trash), and only inside the
// home folder; there is no automatic cleaning.
class Storage : public QObject {
    Q_OBJECT
    // The disks that hold the system and the user's stuff, like
    // QStorageInfo::mountedVolumes() without the pseudo filesystems.
    Q_PROPERTY(QVariantList volumes READ volumes NOTIFY changed)
    // A `du` over the home's top level is running (it can take a while).
    Q_PROPERTY(bool scanning READ scanning NOTIFY changed)
    // The home's biggest top-level entries, biggest first, at most ten.
    Q_PROPERTY(QVariantList folders READ folders NOTIFY changed)
    // The size of the cache and of the trash, from the same `du`.
    Q_PROPERTY(quint64 cacheSize READ cacheSize NOTIFY changed)
    Q_PROPERTY(quint64 trashSize READ trashSize NOTIFY changed)
    // "The trash is already empty." and the like, not errors.
    Q_PROPERTY(QString message READ message NOTIFY changed)

public:
    explicit Storage(QObject* parent = nullptr);
    ~Storage() override;

    // Scans the home folder again (the page asks when it opens).
    Q_INVOKABLE void refresh();
    // Deletes the contents of the trash's files and info folders (the
    // folders stay, that's how the desktop expects them).
    Q_INVOKABLE void emptyTrash();
    // Deletes the top level of the cache (the folder stays; the apps build
    // their caches again).
    Q_INVOKABLE void clearCache();

    QVariantList volumes() const { return m_volumes; }
    bool scanning() const { return m_du != nullptr; }
    QVariantList folders() const { return m_folders; }
    quint64 cacheSize() const { return m_cacheSize; }
    quint64 trashSize() const { return m_trashSize; }
    QString message() const { return m_message; }

signals:
    void changed();

private:
    // One `du -sk` for the home's top level (and the trash): the async work
    // of the page.
    void refillSizes();
    // Deletes the contents of a folder, the folder itself stays; remembers
    // whether anything was there and whether everything went away.
    void cleanFolder(const QString& folder, bool& anything, bool& failed);

    QProcess* m_du = nullptr;
    QVariantList m_volumes;
    QVariantList m_folders;
    quint64 m_cacheSize = 0;
    quint64 m_trashSize = 0;
    QString m_message;
};
