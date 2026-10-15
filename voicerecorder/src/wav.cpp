// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "wav.h"

#include <QFile>
#include <QFileInfo>
#include <QtEndian>

#include <algorithm>
#include <cmath>
#include <cstring>

namespace wav {

qint64 Format::dataBytes(qint64 fileSize) const
{
    const qint64 available = std::max<qint64>(0, fileSize - dataOffset);
    if (declaredSize == 0 || declaredSize == 0xFFFFFFFFu) {
        return available;
    }
    return std::min<qint64>(declaredSize, available);
}

Format parseHeader(const QByteArray& head)
{
    Format format;
    if (head.size() < 12 || head.left(4) != "RIFF" || head.mid(8, 4) != "WAVE") {
        return format;
    }
    const auto* bytes = reinterpret_cast<const uchar*>(head.constData());
    bool haveFormat = false;
    qint64 at = 12;
    while (at + 8 <= head.size()) {
        const QByteArray id = head.mid(at, 4);
        const quint32 size = qFromLittleEndian<quint32>(bytes + at + 4);
        const qint64 body = at + 8;
        if (id == "fmt " && size >= 16 && body + 16 <= head.size()) {
            int tag = qFromLittleEndian<quint16>(bytes + body);
            format.channels = qFromLittleEndian<quint16>(bytes + body + 2);
            format.rate = int(qFromLittleEndian<quint32>(bytes + body + 4));
            format.bits = qFromLittleEndian<quint16>(bytes + body + 14);
            if (tag == 0xFFFE && size >= 26 && body + 26 <= head.size()) {
                tag = qFromLittleEndian<quint16>(bytes + body + 24); // the sub-format's first bytes
            }
            format.isFloat = tag == 3;
            haveFormat = (tag == 1 || tag == 3) && format.channels > 0 && format.rate > 0;
        } else if (id == "data") {
            if (!haveFormat) {
                return format;
            }
            const bool known = format.isFloat ? format.bits == 32 : (format.bits == 16 || format.bits == 24 || format.bits == 32);
            format.valid = known;
            format.dataOffset = body;
            format.declaredSize = size;
            return format;
        }
        at = body + size + (size & 1);
    }
    return format;
}

Format readHeader(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return parseHeader(file.read(4096));
}

float peakOf(const char* data, qint64 bytes, const Format& format)
{
    if (!format.valid) {
        return 0;
    }
    const int width = format.bits / 8;
    const qint64 whole = bytes / format.frameBytes() * format.frameBytes();
    const auto* p = reinterpret_cast<const uchar*>(data);
    float peak = 0;
    for (qint64 i = 0; i + width <= whole; i += width) {
        float value;
        if (format.isFloat) {
            const quint32 raw = qFromLittleEndian<quint32>(p + i);
            std::memcpy(&value, &raw, 4);
        } else if (format.bits == 16) {
            value = qFromLittleEndian<qint16>(p + i) / 32768.0f;
        } else if (format.bits == 24) {
            const qint32 raw = (p[i] << 8 | p[i + 1] << 16 | p[i + 2] << 24) >> 8; // sign-extended
            value = raw / 8388608.0f;
        } else {
            value = qFromLittleEndian<qint32>(p + i) / 2147483648.0f;
        }
        peak = std::max(peak, std::min(1.0f, std::fabs(value)));
    }
    return peak;
}

float tailPeak(const QString& path, int ms)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return 0;
    }
    const Format format = parseHeader(file.read(4096));
    if (!format.valid) {
        return 0;
    }
    const qint64 end = format.dataOffset + format.dataBytes(file.size());
    const qint64 frame = format.frameBytes();
    const qint64 want = std::max<qint64>(1, qint64(format.rate) * ms / 1000) * frame;
    // Whole frames only: the writer may be in the middle of one.
    const qint64 usable = (end - format.dataOffset) / frame * frame;
    const qint64 from = format.dataOffset + std::max<qint64>(0, usable - want);
    if (usable <= 0 || !file.seek(from)) {
        return 0;
    }
    const QByteArray tail = file.read(std::min(want, usable));
    return peakOf(tail.constData(), tail.size(), format);
}

QList<float> waveform(const QString& path, int buckets)
{
    QFile file(path);
    if (buckets <= 0 || !file.open(QIODevice::ReadOnly)) {
        return {};
    }
    const Format format = parseHeader(file.read(4096));
    if (!format.valid) {
        return {};
    }
    const qint64 frame = format.frameBytes();
    const qint64 total = format.dataBytes(file.size()) / frame;
    if (total == 0 || !file.seek(format.dataOffset)) {
        return {};
    }
    const int count = int(std::min<qint64>(buckets, total));
    QList<float> peaks(count, 0.0f);
    const qint64 chunkFrames = 1 << 16;
    QByteArray chunk;
    for (qint64 done = 0; done < total;) {
        const qint64 n = std::min(chunkFrames, total - done);
        chunk = file.read(n * frame);
        const qint64 got = chunk.size() / frame;
        if (got == 0) {
            break;
        }
        // A bucket gets the frames whose index maps into it; the peak of a
        // slice of the chunk, so no sample is looked at twice.
        for (qint64 start = 0; start < got;) {
            const int bucket = int((done + start) * count / total);
            const qint64 bucketEnd = ((bucket + 1) * total + count - 1) / count; // first frame of the next bucket
            const qint64 len = std::min(got - start, std::max<qint64>(1, bucketEnd - (done + start)));
            peaks[bucket] = std::max(peaks[bucket], peakOf(chunk.constData() + start * frame, len * frame, format));
            start += len;
        }
        done += got;
    }
    return peaks;
}

double seconds(const QString& path)
{
    const Format format = readHeader(path);
    if (!format.valid) {
        return 0;
    }
    return double(format.dataBytes(QFileInfo(path).size()) / format.frameBytes()) / format.rate;
}

} // namespace wav

namespace names {

bool valid(const QString& base)
{
    if (base.isEmpty() || base.size() > 200 || base != base.trimmed() || base.startsWith(QLatin1Char('.'))
        || base.endsWith(QLatin1Char('.'))) {
        return false;
    }
    return std::none_of(base.begin(), base.end(), [](QChar c) {
        return c == QLatin1Char('/') || c == QLatin1Char('\\') || c.unicode() < 0x20 || c.unicode() == 0x7f;
    });
}

QString stamp(const QDateTime& when)
{
    return QStringLiteral("Recording ") + when.toString(QStringLiteral("yyyy-MM-dd HH.mm.ss"));
}

QString unique(const QDir& dir, const QString& base, const QString& ext)
{
    QString name = base + ext;
    for (int n = 2; dir.exists(name); n++) {
        name = QStringLiteral("%1 (%2)%3").arg(base).arg(n).arg(ext);
    }
    return name;
}

} // namespace names
