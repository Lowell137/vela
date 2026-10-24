// SPDX-FileCopyrightText: 2026 Brando Giuffrida
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>

#include <sys/xattr.h>

// File tags the freedesktop way: the extended attribute user.xdg.tags, names
// separated by commas (what Dolphin and Baloo read and write too). Without
// xattr support on the file system the file simply has no tags.
namespace vela::tags {

inline constexpr const char* attribute = "user.xdg.tags";

inline QStringList read(const QString& path)
{
    const QByteArray file = path.toUtf8();
    char buffer[1024];
    const ssize_t size = ::getxattr(file.constData(), attribute, buffer, sizeof(buffer));
    if (size <= 0) {
        return {};
    }
    QStringList tags = QString::fromUtf8(buffer, int(size)).split(u',', Qt::SkipEmptyParts);
    for (QString& tag : tags) {
        tag = tag.trimmed();
    }
    tags.removeAll(QString());
    return tags;
}

// False when the file system refuses (no xattr, read-only, not ours).
inline bool write(const QString& path, const QStringList& tags)
{
    const QByteArray file = path.toUtf8();
    if (tags.isEmpty()) {
        const int removed = ::removexattr(file.constData(), attribute);
        return removed == 0 || errno == ENODATA;
    }
    const QByteArray value = tags.join(u',').toUtf8();
    return ::setxattr(file.constData(), attribute, value.constData(), size_t(value.size()), 0) == 0;
}

} // namespace vela::tags
