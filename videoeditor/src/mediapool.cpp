// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "mediapool.h"

#include "exportcommand.h"
#include "timeline.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>

namespace {

// Runs a tool in the background; `done(ok, stdout)` when it ends.
template <typename Done>
void run(QObject* owner, const QString& program, const QStringList& arguments, Done done)
{
    auto* process = new QProcess(owner);
    QObject::connect(process, &QProcess::finished, owner, [process, done](int code, QProcess::ExitStatus status) {
        done(status == QProcess::NormalExit && code == 0, process->readAllStandardOutput());
        process->deleteLater();
    });
    QObject::connect(process, &QProcess::errorOccurred, owner, [process, done](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            done(false, QByteArray());
            process->deleteLater();
        }
    });
    process->start(program, arguments);
}

// A still video: the picture (or black) for Timeline::stillLength seconds,
// with silence, small enough for the preview.
QStringList stillArguments(const QString& source, const QString& output)
{
    QStringList args { "-v", "error", "-y" };
    if (source.isEmpty()) {
        args << "-f" << "lavfi" << "-i" << "color=c=black:s=640x360:r=5";
    } else {
        args << "-loop" << "1" << "-framerate" << "5" << "-i" << source;
    }
    args << "-f" << "lavfi" << "-i" << "anullsrc=r=48000:cl=stereo" << "-t" << QString::number(Timeline::stillLength)
         << "-vf" << "scale='trunc(min(1280,iw)/2)*2':-2,format=yuv420p" << "-c:v" << "libx264" << "-preset" << "ultrafast"
         << "-tune" << "stillimage" << "-g" << "25" << "-c:a" << "aac" << "-f" << "mp4" << output;
    return args;
}

} // namespace

MediaPool::MediaPool(QObject* parent)
    : QObject(parent)
    , m_cache(QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation) + QStringLiteral("/Vela/videoeditor"))
{
    QDir().mkpath(m_cache);
    makeStill(QString()); // the gaps' black
}

void MediaPool::add(const QList<QUrl>& urls, bool place, double at)
{
    for (const QUrl& url : urls) {
        const QString path = url.isLocalFile() ? url.toLocalFile() : url.toString();
        if (!item(path).isEmpty()) {
            if (place) {
                emit placed(item(path), at);
            }
            continue;
        }
        m_items.append(QVariantMap { { "path", path }, { "name", QFileInfo(path).fileName() }, { "loading", true } });
        probe(path, place, at);
    }
    emit itemsChanged();
}

void MediaPool::setFiles(const QStringList& paths)
{
    if (paths == this->paths()) {
        return;
    }
    m_items.clear();
    QList<QUrl> urls;
    for (const QString& path : paths) {
        urls.append(QUrl::fromLocalFile(path));
    }
    add(urls);
}

void MediaPool::remove(const QString& path)
{
    for (qsizetype i = 0; i < m_items.size(); i++) {
        if (m_items[i].toMap().value("path").toString() == path) {
            m_items.removeAt(i);
            emit itemsChanged();
            return;
        }
    }
}

QStringList MediaPool::paths() const
{
    QStringList list;
    for (const QVariant& item : m_items) {
        list.append(item.toMap().value("path").toString());
    }
    return list;
}

QVariantMap MediaPool::item(const QString& path) const
{
    for (const QVariant& item : m_items) {
        if (item.toMap().value("path").toString() == path) {
            return item.toMap();
        }
    }
    return {};
}

void MediaPool::probe(const QString& path, bool place, double at)
{
    m_pending++;
    if (!QFileInfo::exists(path)) {
        finish(path, { { "path", path }, { "name", QFileInfo(path).fileName() }, { "missing", true } }, false, at);
        return;
    }
    run(this, QStringLiteral("ffprobe"), { "-v", "quiet", "-print_format", "json", "-show_format", "-show_streams", path },
        [this, path, place, at](bool ok, const QByteArray& json) {
            QVariantMap item = ok ? parseProbe(json, path) : QVariantMap();
            if (item.isEmpty()) {
                emit failed(QFileInfo(path).fileName());
            }
            finish(path, item, place, at);
        });
}

void MediaPool::finish(const QString& path, const QVariantMap& item, bool place, double at)
{
    m_pending--;
    for (qsizetype i = 0; i < m_items.size(); i++) {
        if (m_items[i].toMap().value("path").toString() != path) {
            continue;
        }
        if (item.isEmpty()) {
            m_items.removeAt(i); // nothing playable
        } else {
            m_items[i] = item;
            if (item.value("kind") != QLatin1String("audio") && !item.value("missing").toBool()) {
                thumbnail(path, item);
            }
            if (item.value("kind") == QLatin1String("image")) {
                makeStill(path);
            }
            if (place) {
                emit placed(item, at);
            }
        }
        break;
    }
    emit itemsChanged();
}

void MediaPool::thumbnail(const QString& path, const QVariantMap& item)
{
    const QFileInfo info(path);
    const QString key = path + QString::number(info.lastModified().toMSecsSinceEpoch());
    const QString file = m_cache + u'/' + QString::fromLatin1(QCryptographicHash::hash(key.toUtf8(), QCryptographicHash::Sha1).toHex()) + QStringLiteral(".jpg");
    auto set = [this, path, file] {
        for (QVariant& entry : m_items) {
            QVariantMap map = entry.toMap();
            if (map.value("path").toString() == path) {
                map.insert("thumb", QUrl::fromLocalFile(file));
                entry = map;
                emit itemsChanged();
            }
        }
    };
    if (QFileInfo::exists(file)) {
        set();
        return;
    }
    QStringList args { "-v", "error", "-y" };
    if (item.value("kind") == QLatin1String("video")) {
        args << "-ss" << num(item.value("duration").toDouble() / 10);
    }
    args << "-i" << path << "-frames:v" << "1" << "-vf" << "scale=192:-2" << "-q:v" << "4" << file;
    run(this, QStringLiteral("ffmpeg"), args, [set](bool ok, const QByteArray&) {
        if (ok) {
            set();
        }
    });
}

void MediaPool::makeStill(const QString& source)
{
    const QString file = Timeline::stillPath(m_cache, source);
    if (QFileInfo(file).size() > 0) {
        return;
    }
    const QString partial = file + QStringLiteral(".part");
    run(this, QStringLiteral("ffmpeg"), stillArguments(source, partial), [this, file, partial](bool ok, const QByteArray&) {
        if (ok && QFile::rename(partial, file)) {
            emit stillReady();
        } else {
            QFile::remove(partial);
        }
    });
}

bool MediaPool::exists(const QString& path) const
{
    return QFileInfo::exists(path);
}

bool MediaPool::newer(const QString& path, const QString& than) const
{
    return QFileInfo(path).lastModified() > QFileInfo(than).lastModified();
}

void MediaPool::removeFile(const QString& path) const
{
    QFile::remove(path);
}

void MediaPool::showInFolder(const QString& path) const
{
    QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(path).absolutePath()));
}
