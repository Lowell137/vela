// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "library.h"
#include "wav.h"

#include <QClipboard>
#include <QDesktopServices>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QProcess>
#include <QThread>
#include <QUrl>

namespace {
const QStringList audioFilters { QStringLiteral("*.wav"), QStringLiteral("*.ogg"), QStringLiteral("*.m4a"),
    QStringLiteral("*.mp3"), QStringLiteral("*.flac"), QStringLiteral("*.opus") };
}

Library::Library(const QDir& folder, QObject* parent)
    : QObject(parent)
    , m_folder(folder)
{
    m_folder.mkpath(QStringLiteral("."));
    m_watcher.addPath(m_folder.path());
    m_later.setSingleShot(true);
    m_later.setInterval(200);
    connect(&m_later, &QTimer::timeout, this, &Library::refresh);
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, &m_later, qOverload<>(&QTimer::start));
    refresh();
}

void Library::refresh()
{
    QVariantList found;
    const QFileInfoList files = m_folder.entryInfoList(audioFilters, QDir::Files, QDir::Time); // newest first
    for (const QFileInfo& file : files) {
        // shortcut: only a WAV's length is known without opening it, the
        // others show it once selected (mpv knows); upgrade with ffprobe.
        found.append(QVariantMap {
            { QStringLiteral("name"), file.fileName() },
            { QStringLiteral("base"), file.completeBaseName() },
            { QStringLiteral("path"), file.filePath() },
            { QStringLiteral("modified"), file.lastModified().toMSecsSinceEpoch() },
            { QStringLiteral("seconds"), wav::seconds(file.filePath()) },
        });
    }
    if (found != m_recordings) {
        m_recordings = found;
        emit recordingsChanged();
    }
}

void Library::loadWaveform(const QString& path)
{
    const int generation = ++m_generation;
    m_waveformPath = path;
    m_waveform.clear();
    emit waveformChanged();
    // The file may be hours long: read it away from the interface.
    QThread* thread = QThread::create([this, path, generation] {
        const QList<float> peaks = wav::waveform(path, 800);
        QVariantList list;
        list.reserve(peaks.size());
        for (float peak : peaks) {
            list.append(peak);
        }
        QMetaObject::invokeMethod(this, [this, list, generation] {
            if (generation == m_generation) {
                m_waveform = list;
                emit waveformChanged();
            }
        }, Qt::QueuedConnection);
    });
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    thread->start();
}

QString Library::rename(const QString& path, const QString& base)
{
    const QFileInfo info(path);
    const QString name = base.trimmed();
    if (!names::valid(name)) {
        return tr("This name can't be used. Use letters and numbers, without slashes.");
    }
    if (name == info.completeBaseName()) {
        return {};
    }
    const QString target = info.dir().filePath(name + QLatin1Char('.') + info.suffix());
    if (QFileInfo::exists(target)) {
        return tr("A recording with this name already exists.");
    }
    if (!QFile::rename(path, target)) {
        return tr("The recording could not be renamed.");
    }
    refresh();
    return {};
}

bool Library::remove(const QString& path)
{
    const bool ok = QFile::moveToTrash(path);
    refresh();
    return ok;
}

void Library::convert(const QStringList& arguments, const QString& tmp, const QString& target, bool replace)
{
    if (m_busy) {
        return;
    }
    m_busy = true;
    emit busyChanged();
    auto* ffmpeg = new QProcess(this);
    auto finish = [this, ffmpeg, tmp, target, replace](const QString& error) {
        QString problem = error;
        if (problem.isEmpty()) {
            if (replace) {
                QFile::remove(target); // shortcut: a crash between these two loses the original, rename(2) over it needs QSaveFile
            }
            if (!QFile::rename(tmp, target)) {
                problem = tr("The file could not be written.");
            }
        }
        if (!problem.isEmpty()) {
            QFile::remove(tmp);
        }
        m_busy = false;
        emit busyChanged();
        refresh();
        emit done(problem.isEmpty() ? target : QString(), problem);
        ffmpeg->deleteLater();
    };
    connect(ffmpeg, &QProcess::finished, this, [ffmpeg, finish, this](int code, QProcess::ExitStatus status) {
        finish(code == 0 && status == QProcess::NormalExit ? QString() : tr("ffmpeg could not convert the recording."));
    });
    connect(ffmpeg, &QProcess::errorOccurred, this, [finish, this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            finish(tr("ffmpeg could not be started."));
        }
    });
    ffmpeg->start(QStringLiteral("ffmpeg"), arguments);
}

void Library::trim(const QString& path, double start, double end, bool inPlace)
{
    const QFileInfo info(path);
    const QString ext = QLatin1Char('.') + info.suffix();
    const QString tmp = info.dir().filePath(QStringLiteral(".trim-%1").arg(QDateTime::currentMSecsSinceEpoch()) + ext);
    const QString target = inPlace ? path
        : info.dir().filePath(names::unique(info.dir(), info.completeBaseName() + tr(" (trimmed)"), ext));
    // -c copy: exact for PCM, and for the others the cut falls on a packet.
    convert({ QStringLiteral("-y"), QStringLiteral("-v"), QStringLiteral("error"), QStringLiteral("-i"), path,
              QStringLiteral("-ss"), QString::number(start, 'f', 3), QStringLiteral("-to"), QString::number(end, 'f', 3),
              QStringLiteral("-c"), QStringLiteral("copy"), tmp }, tmp, target, inPlace);
}

void Library::exportAs(const QString& path, const QString& extension)
{
    const QFileInfo info(path);
    const QString ext = QLatin1Char('.') + extension;
    const QString tmp = info.dir().filePath(QStringLiteral(".export-%1").arg(QDateTime::currentMSecsSinceEpoch()) + ext);
    const QString target = info.dir().filePath(names::unique(info.dir(), info.completeBaseName(), ext));
    convert({ QStringLiteral("-y"), QStringLiteral("-v"), QStringLiteral("error"), QStringLiteral("-i"), path,
              QStringLiteral("-vn"), tmp }, tmp, target, false);
}

void Library::showInFolder(const QString& path)
{
    // shortcut: opens the folder, not selecting the file; org.freedesktop.FileManager1.ShowItems when the Explorer has it.
    QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(path).path()));
}

void Library::copyPath(const QString& path)
{
    QGuiApplication::clipboard()->setText(path);
}
