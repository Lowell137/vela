// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "playlist.h"

#include <QCollator>
#include <QDir>
#include <QFileInfo>

#include <algorithm>

namespace {

// By name: mpv plays far more than this, and what's opened by hand is played
// whatever it's called.
QStringList patterns()
{
    static const QStringList list = QStringLiteral(
        "mp4 m4v mkv webm avi mov wmv flv mpg mpeg ts m2ts ogv 3gp "
        "mp3 flac ogg oga opus m4a aac wav wma aiff ape mka")
        .split(u' ');
    QStringList result;
    for (const QString& extension : list) {
        result.append(QStringLiteral("*.") + extension);
    }
    return result;
}

} // namespace

void Playlist::setIndex(int index)
{
    if (index >= 0 && index < m_files.size() && index != m_index) {
        m_index = index;
        emit indexChanged();
    }
}

QString Playlist::folder() const
{
    return QFileInfo(current().toLocalFile()).path();
}

void Playlist::open(const QUrl& url)
{
    const QFileInfo info(url.toLocalFile());
    QDir dir(info.isDir() ? info.filePath() : info.path());
    QStringList names = dir.entryList(patterns(), QDir::Files);
    QCollator order; // "e2" before "e10", like the File Explorer
    order.setNumericMode(true);
    order.setCaseSensitivity(Qt::CaseInsensitive);
    std::sort(names.begin(), names.end(), order);
    if (info.isFile() && !names.contains(info.fileName())) {
        names.append(info.fileName());
    }

    m_files.clear();
    for (const QString& name : std::as_const(names)) {
        m_files.append(QUrl::fromLocalFile(dir.filePath(name)));
    }
    m_index = info.isFile() ? names.indexOf(info.fileName()) : names.isEmpty() ? -1 : 0;
    emit filesChanged();
    emit indexChanged();
}

QString Playlist::filter() const
{
    return patterns().join(u' ');
}
