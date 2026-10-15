// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

// What the Sound Recorder needs of a WAV file: its header (also of a file
// pw-record is still writing) and the peaks of its samples.

#include <QByteArray>
#include <QDateTime>
#include <QDir>
#include <QList>
#include <QString>

namespace wav {

struct Format {
    bool valid = false;
    bool isFloat = false;
    int channels = 0;
    int rate = 0;
    int bits = 0;
    qint64 dataOffset = 0;
    quint32 declaredSize = 0; // the data chunk's size field (0 or 0xFFFFFFFF while recording)

    int frameBytes() const { return channels * bits / 8; }
    // The bytes of samples in a file of `fileSize`: a growing or truncated file
    // is read up to its end whatever its header says.
    qint64 dataBytes(qint64 fileSize) const;
};

// The first bytes of a file (4 KiB is plenty).
Format parseHeader(const QByteArray& head);
Format readHeader(const QString& path);
// Largest |sample| (0..1) of whole frames in `bytes` of sample data.
float peakOf(const char* data, qint64 bytes, const Format& format);
// Peak of the last `ms` milliseconds of a file, 0 when there is none yet.
float tailPeak(const QString& path, int ms);
// Peaks of the whole file in at most `buckets` steps, each 0..1.
QList<float> waveform(const QString& path, int buckets);
// Length in seconds, 0 for a file that is not a readable WAV.
double seconds(const QString& path);

} // namespace wav

namespace names {

// A file name (without its folder) that is safe to create: not empty, no
// slash or control character, not "." or "..", not hidden.
bool valid(const QString& base);
// "Recording 2026-10-15 14.32.07"
QString stamp(const QDateTime& when);
// base + ext, or "base (2)" + ext when it is taken in `dir`.
QString unique(const QDir& dir, const QString& base, const QString& ext);

} // namespace names
