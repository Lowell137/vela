// SPDX-FileCopyrightText: 2026 Brando Giuffrida
// SPDX-License-Identifier: GPL-3.0-or-later

#include "storage.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QStorageInfo>

#include <algorithm>

namespace {

// The top-level folder of a home: what a person could clean up.
QString cacheFolder()
{
    return QDir::homePath() + QStringLiteral("/.cache");
}

QString trashFolder()
{
    return QDir::homePath() + QStringLiteral("/.local/share/Trash/files");
}

// The sizes come from `du -sk` as blocks of 1024 bytes.
constexpr qint64 blockSize = 1024;

} // namespace

Storage::Storage(QObject* parent) : QObject(parent)
{
    const QList<QStorageInfo> mounted = QStorageInfo::mountedVolumes();
    QStringList devices;
    for (const QStorageInfo& volume : mounted) {
        if (!volume.isValid() || volume.bytesTotal() == 0) {
            continue;
        }
        // Only the volumes a person looks at: the root and the disks mounted
        // in the home or in the usual mount points; the pseudo filesystems
        // (procfs, sysfs, tmpfs, the boot partition's EFI...) are plumbing.
        const QString path = volume.rootPath();
        const bool interesting = path == QStringLiteral("/")
            || path == QStringLiteral("/home") || path.startsWith(QStringLiteral("/home/"))
            || path == QStringLiteral("/media") || path.startsWith(QStringLiteral("/media/"))
            || path == QStringLiteral("/mnt") || path.startsWith(QStringLiteral("/mnt/"));
        // One disk mounted twice (btrfs subvolumes for / and /home) is one
        // row, under its first mount point.
        if (!interesting || devices.contains(QString::fromUtf8(volume.device()))) {
            continue;
        }
        devices.append(QString::fromUtf8(volume.device()));
        // The label when the device has one ("Data"), else the mount point.
        const QString name = volume.name().isEmpty() ? path : volume.name();
        m_volumes.append(QVariantMap {
            { QStringLiteral("name"), name },
            { QStringLiteral("device"), volume.device() },
            { QStringLiteral("rootPath"), path },
            { QStringLiteral("total"), volume.bytesTotal() },
            // bytesFree, what the user can still fill: Windows shows that.
            { QStringLiteral("free"), volume.bytesFree() },
            { QStringLiteral("used"), volume.bytesTotal() - volume.bytesFree() },
        });
    }
}

Storage::~Storage()
{
    if (m_du) {
        m_du->kill();
        m_du->waitForFinished(1000);
    }
}

void Storage::refresh()
{
    refillSizes();
}

void Storage::refillSizes()
{
    if (m_du) {
        return; // already measuring
    }
    const QString home = QDir::homePath();
    const QString trash = trashFolder();
    // The home's top level, and the trash's files folder with it (it sits
    // deeper, but the page wants its size from the same run).
    QStringList paths;
    // The trash first: du omits arguments inside a directory it has already
    // walked (the nested Trash folder would be counted but never shown).
    if (QFileInfo(trash).isDir()) {
        paths.append(trash);
    }
    const QFileInfoList entries = QDir(home)
        .entryInfoList(QDir::Dirs | QDir::Files | QDir::Hidden | QDir::NoDotAndDotDot, QDir::NoSort);
    for (const QFileInfo& entry : entries) {
        // A disk mounted in the home (a cloud drive) has its own row above:
        // walking it could mean the whole remote.
        if (entry.absoluteFilePath() != trash && QStorageInfo(entry.absoluteFilePath()).rootPath() != entry.absoluteFilePath()) {
            paths.append(entry.absoluteFilePath());
        }
    }
    if (paths.isEmpty()) {
        m_folders.clear();
        m_cacheSize = 0;
        m_trashSize = 0;
        emit changed();
        return;
    }
    // du is coreutils, never missing on the systems Vela targets; guarded
    // anyway (findExecutable is empty then).
    const QString du = QStandardPaths::findExecutable(QStringLiteral("du"));
    if (du.isEmpty()) {
        return; // du missing: the page shows what it knows
    }
    m_du = new QProcess(this);
    connect(m_du, &QProcess::finished, this, [this, home, trash]() {
        // "size<TAB>path", one line per entrance that was asked for; du exits
        // non-zero when it couldn't read something (a permission...), the
        // lines it did produce are still fine.
        QHash<QString, qint64> sizes;
        const QString output = QString::fromUtf8(m_du->readAllStandardOutput());
        for (const QString& raw : output.split(QLatin1Char('\n'), Qt::SkipEmptyParts)) {
            const int tab = raw.indexOf(QLatin1Char('\t'));
            if (tab <= 0) {
                continue;
            }
            sizes.insert(raw.mid(tab + 1), raw.left(tab).toLongLong() * blockSize);
        }
        // The home's top level again, so the order matches the listing.
        QVariantList folders;
        const QFileInfoList entries = QDir(home)
            .entryInfoList(QDir::Dirs | QDir::Files | QDir::Hidden | QDir::NoDotAndDotDot, QDir::NoSort);
        for (const QFileInfo& entry : entries) {
            if (!sizes.contains(entry.absoluteFilePath())) {
                continue;
            }
            folders.append(QVariantMap {
                { QStringLiteral("name"), entry.fileName() },
                { QStringLiteral("size"), sizes.value(entry.absoluteFilePath()) },
                { QStringLiteral("path"), entry.absoluteFilePath() },
            });
        }
        // Biggest first, the whole list for the page to cut.
        std::sort(folders.begin(), folders.end(), [](const QVariant& a, const QVariant& b) {
            return a.toMap().value(QStringLiteral("size")).toLongLong()
                > b.toMap().value(QStringLiteral("size")).toLongLong();
        });
        if (folders.length() > 10) {
            folders = folders.mid(0, 10);
        }
        m_folders = folders;
        m_cacheSize = static_cast<quint64>(sizes.value(cacheFolder(), 0));
        m_trashSize = static_cast<quint64>(sizes.value(trash, 0));
        m_du->deleteLater();
        m_du = nullptr;
        emit changed();
    });
    m_du->start(du, QStringList() << QStringLiteral("-sk") << paths);
    emit changed(); // scanning: "Measuring..." instead of "Nothing could be measured"
}

void Storage::cleanFolder(const QString& folder, bool& anything, bool& failed)
{
    const QFileInfoList entries = QDir(folder)
        .entryInfoList(QDir::Dirs | QDir::Files | QDir::Hidden | QDir::NoDotAndDotDot, QDir::NoSort);
    for (const QFileInfo& entry : entries) {
        anything = true;
        // The folder itself stays, only its contents go.
        const bool ok = entry.isDir() && !entry.isSymLink() ? QDir(entry.absoluteFilePath()).removeRecursively()
            : QFile::remove(entry.absoluteFilePath());
        if (!ok) {
            failed = true;
        }
    }
}

void Storage::emptyTrash()
{
    const QString trash = trashFolder();
    const QString info = QDir::homePath() + QStringLiteral("/.local/share/Trash/info");
    bool anything = false;
    bool failed = false;
    // Both folders: files and info go together, or the desktop's trash gets
    // confused.
    cleanFolder(trash, anything, failed);
    cleanFolder(info, anything, failed);
    if (!anything) {
        m_message = tr("The trash is already empty.");
    } else if (failed) {
        m_message = tr("Some of the trash could not be deleted.");
    } else {
        m_message = tr("The trash is emptied.");
    }
    refillSizes();
    emit changed();
}

void Storage::clearCache()
{
    const QString cache = cacheFolder();
    bool anything = false;
    bool failed = false;
    cleanFolder(cache, anything, failed);
    if (!anything) {
        m_message = tr("The cache is already empty.");
    } else if (failed) {
        m_message = tr("Some of the cache could not be deleted.");
    } else {
        m_message = tr("The cache is cleared. The apps build it again.");
    }
    refillSizes();
    emit changed();
}
