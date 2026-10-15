// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// WAV headers and peaks (also of a file that is still growing), names, and
// the decimated waveform.

#include "wav.h"

#include <QFile>
#include <QTemporaryDir>
#include <QtEndian>
#include <QtTest>

namespace {

template <typename T>
void put(QByteArray& out, T value)
{
    T le = qToLittleEndian(value);
    out.append(reinterpret_cast<const char*>(&le), sizeof(T));
}

// A WAV: tag 1 (PCM) or 3 (float), `declared` is the data size field.
QByteArray makeWav(int tag, int channels, int bits, const QByteArray& data, quint32 declared, bool extensible = false)
{
    QByteArray out("RIFF");
    put<quint32>(out, 36 + data.size());
    out += "WAVEfmt ";
    put<quint32>(out, extensible ? 40 : 16);
    put<quint16>(out, extensible ? 0xFFFE : tag);
    put<quint16>(out, channels);
    put<quint32>(out, 48000);
    put<quint32>(out, 48000 * channels * bits / 8);
    put<quint16>(out, channels * bits / 8);
    put<quint16>(out, bits);
    if (extensible) {
        put<quint16>(out, 22);
        put<quint16>(out, bits);
        put<quint32>(out, 3);
        put<quint16>(out, tag);
        out.append(QByteArray(14, 0));
    }
    out += "data";
    put<quint32>(out, declared);
    return out + data;
}

QByteArray pcm16(const QList<int>& samples)
{
    QByteArray out;
    for (int s : samples) {
        put<qint16>(out, qint16(s));
    }
    return out;
}

QString write(const QTemporaryDir& dir, const QString& name, const QByteArray& bytes)
{
    QFile file(dir.filePath(name));
    if (!file.open(QIODevice::WriteOnly)) {
        qFatal("temporary file");
    }
    file.write(bytes);
    return file.fileName();
}

} // namespace

class WavTest : public QObject {
    Q_OBJECT

private slots:
    void header16Mono()
    {
        const auto f = wav::parseHeader(makeWav(1, 1, 16, pcm16({ 0, 1 }), 4));
        QVERIFY(f.valid);
        QVERIFY(!f.isFloat);
        QCOMPARE(f.channels, 1);
        QCOMPARE(f.rate, 48000);
        QCOMPARE(f.dataOffset, 44);
        QCOMPARE(f.frameBytes(), 2);
    }

    void headerExtensibleFloat()
    {
        const auto f = wav::parseHeader(makeWav(3, 2, 32, QByteArray(8, 0), 8, true));
        QVERIFY(f.valid);
        QVERIFY(f.isFloat);
        QCOMPARE(f.frameBytes(), 8);
        QCOMPARE(f.dataOffset, 12 + 8 + 40 + 8);
    }

    void headerRejects()
    {
        QVERIFY(!wav::parseHeader("garbage").valid);
        QVERIFY(!wav::parseHeader(makeWav(2, 1, 4, {}, 0)).valid); // ADPCM-like tag 2
        QVERIFY(!wav::parseHeader(makeWav(1, 1, 16, {}, 0).left(30)).valid); // cut inside the header
    }

    void peak16()
    {
        const auto f = wav::parseHeader(makeWav(1, 2, 16, {}, 0));
        const QByteArray data = pcm16({ 100, -16384, 8000, 3 }); // frames L,R / L,R
        QCOMPARE(wav::peakOf(data.constData(), data.size(), f), 0.5f);
        const QByteArray full = pcm16({ -32768 });
        QCOMPARE(wav::peakOf(full.constData(), 2, wav::parseHeader(makeWav(1, 1, 16, {}, 0))), 1.0f);
    }

    void peak24AndFloat()
    {
        QByteArray d24;
        d24 += QByteArray::fromHex("000040"); // +0.5
        d24 += QByteArray::fromHex("0000c0"); // -0.5
        const auto f24 = wav::parseHeader(makeWav(1, 1, 24, {}, 0));
        QCOMPARE(wav::peakOf(d24.constData(), d24.size(), f24), 0.5f);

        QByteArray f32;
        put<float>(f32, 0.25f);
        put<float>(f32, -2.0f); // clipped values still read as 1
        const auto ff = wav::parseHeader(makeWav(3, 1, 32, {}, 0));
        QCOMPARE(wav::peakOf(f32.constData(), f32.size(), ff), 1.0f);
        QCOMPARE(wav::peakOf(f32.constData(), 4, ff), 0.25f);
    }

    void tailOfAGrowingFile()
    {
        QTemporaryDir dir;
        // 100 ms of silence, then 10 ms at 0.5; size field 0 as while recording.
        QList<int> samples(4800, 0);
        samples += QList<int>(480, 16384);
        // A half frame at the end: the writer is in the middle of a sample.
        const QByteArray bytes = makeWav(1, 1, 16, pcm16(samples) + QByteArray(1, 0x7f), 0);
        const QString path = write(dir, "grow.wav", bytes);
        QCOMPARE(wav::tailPeak(path, 50), 0.5f);
        QCOMPARE(wav::tailPeak(path, 1), 0.5f);
        QCOMPARE(wav::seconds(path), 0.11);
        // Only the header yet.
        QCOMPARE(wav::tailPeak(write(dir, "empty.wav", makeWav(1, 1, 16, {}, 0)), 50), 0.0f);
        QCOMPARE(wav::tailPeak(dir.filePath("missing.wav"), 50), 0.0f);
        // Cut inside the header: not valid, no peak.
        QCOMPARE(wav::tailPeak(write(dir, "cut.wav", bytes.left(20)), 50), 0.0f);
    }

    void declaredSizeHolds()
    {
        // A trailing chunk after the data is not audio.
        QTemporaryDir dir;
        QByteArray bytes = makeWav(1, 1, 16, pcm16({ 1000, 1000, 32767 }), 4); // declares 2 samples
        const QString path = write(dir, "tail.wav", bytes);
        QCOMPARE(wav::seconds(path), 2.0 / 48000);
        QVERIFY(wav::tailPeak(path, 50) < 0.1f);
    }

    void waveformBuckets()
    {
        QTemporaryDir dir;
        QList<int> samples;
        for (int i = 0; i < 1000; i++) {
            samples.append(i == 250 ? 32767 : (i == 900 ? -16384 : 10)); // one loud spot in bucket 2 and 9 of 10
        }
        const QString path = write(dir, "w.wav", makeWav(1, 1, 16, pcm16(samples), 2000));
        const QList<float> peaks = wav::waveform(path, 10);
        QCOMPARE(peaks.size(), 10);
        QCOMPARE(peaks[2], 32767 / 32768.0f);
        QCOMPARE(peaks[9], 0.5f);
        QVERIFY(peaks[0] < 0.01f);
        // Fewer frames than buckets: one bucket per frame.
        QCOMPARE(wav::waveform(write(dir, "s.wav", makeWav(1, 1, 16, pcm16({ 5, 6, 7 }), 6)), 100).size(), 3);
        QVERIFY(wav::waveform(dir.filePath("none.wav"), 10).isEmpty());
    }

    void waveformLargerThanAChunk()
    {
        QTemporaryDir dir;
        QList<int> samples(200000, 0); // > the 65536-frame chunk
        samples[150000] = 20000;
        const QString path = write(dir, "big.wav", makeWav(1, 1, 16, pcm16(samples), 400000));
        const QList<float> peaks = wav::waveform(path, 100);
        QCOMPARE(peaks.size(), 100);
        QCOMPARE(peaks[75], 20000 / 32768.0f);
        QCOMPARE(*std::max_element(peaks.begin(), peaks.end()), peaks[75]);
    }

    void nameValidation()
    {
        QVERIFY(names::valid(QStringLiteral("Recording 2026-10-15 14.32.07")));
        QVERIFY(names::valid(QStringLiteral("Toplantı kaydı")));
        QVERIFY(!names::valid(QString()));
        QVERIFY(!names::valid(QStringLiteral("a/b")));
        QVERIFY(!names::valid(QStringLiteral("a\\b")));
        QVERIFY(!names::valid(QStringLiteral("..")));
        QVERIFY(!names::valid(QStringLiteral(".hidden")));
        QVERIFY(!names::valid(QStringLiteral(" padded ")));
        QVERIFY(!names::valid(QStringLiteral("tab\there")));
        QVERIFY(!names::valid(QString(201, QLatin1Char('x'))));
    }

    void uniqueNames()
    {
        QTemporaryDir dir;
        const QDir d(dir.path());
        const QDateTime when(QDate(2026, 10, 15), QTime(14, 32, 7));
        const QString base = names::stamp(when);
        QCOMPARE(base, QStringLiteral("Recording 2026-10-15 14.32.07"));
        QCOMPARE(names::unique(d, base, ".wav"), base + ".wav");
        write(dir, base + ".wav", "x");
        QCOMPARE(names::unique(d, base, ".wav"), base + " (2).wav");
        write(dir, base + " (2).wav", "x");
        QCOMPARE(names::unique(d, base, ".wav"), base + " (3).wav");
    }
};

QTEST_APPLESS_MAIN(WavTest)
#include "wav.moc"
