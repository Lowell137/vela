// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// The formats ImageMagick adds to what Qt reads: the list it reports, and
// real files in them (made with ImageMagick itself, so they are skipped when
// it isn't installed).

#include "imageformats.h"

#include <QProcess>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>

class FormatsTest : public QObject {
    Q_OBJECT

    QTemporaryDir m_dir;
    QString m_magick;

    // A 40x30 red picture in `suffix`, or an empty name if it can't be made.
    QString make(const QString& name)
    {
        const QString path = m_dir.filePath(name);
        QProcess process;
        process.start(m_magick, { QStringLiteral("-size"), QStringLiteral("40x30"), QStringLiteral("xc:red"), path });
        return process.waitForFinished(20000) && process.exitCode() == 0 ? path : QString();
    }

private slots:
    void initTestCase() { m_magick = QStandardPaths::findExecutable(QStringLiteral("magick")); }

    void parsesTheFormatList()
    {
        const QByteArray list =
            "   Format  Module    Mode  Description\n"
            "-------------------------------------------------------------\n"
            "      3FR  DNG       r--   Hasselblad CFV/H3D39II Raw Format\n"
            "      3GP  VIDEO     r--   Media Container\n"
            "     AVIF  HEIC      rw+   AV1 Image File Format\n"
            "     DDS*  DDS       rw+   Microsoft DirectDraw Surface\n"
            "      EPT  EPT       -w+   Encapsulated PostScript\n"
            "     HEIC  HEIC      rw+   High Efficiency Image Format\n";
        const QStringList readable = formats::parseReadable(list);
        QVERIFY(readable.contains(QStringLiteral("avif")));
        QVERIFY(readable.contains(QStringLiteral("heic")));
        QVERIFY(readable.contains(QStringLiteral("3fr")));
        QVERIFY(readable.contains(QStringLiteral("dds"))); // the "*" isn't part of the name
        QVERIFY(!readable.contains(QStringLiteral("3gp"))); // a video container
        QVERIFY(!readable.contains(QStringLiteral("ept"))); // write only, not a listed picture
    }

    void readsTheFormatsQtDoesNot()
    {
        if (m_magick.isEmpty()) {
            QSKIP("ImageMagick isn't installed");
        }
        int tried = 0;
        for (const char* suffix : { "avif", "heic", "jxl", "psd", "jp2" }) {
            if (!formats::extra().contains(QLatin1String(suffix))) {
                continue;
            }
            const QString path = make(QStringLiteral("red.") + QLatin1String(suffix));
            if (path.isEmpty()) {
                continue; // this build of ImageMagick can't write it
            }
            tried++;
            QString error;
            const QImage image = formats::read(path, {}, &error);
            QVERIFY2(!image.isNull(), qPrintable(QLatin1String(suffix) + ": " + error));
            QCOMPARE(image.size(), QSize(40, 30));
            const QColor color = image.pixelColor(20, 15);
            QVERIFY2(color.red() > 200 && color.green() < 60 && color.blue() < 60, suffix);
        }
        if (tried == 0) {
            QSKIP("this ImageMagick can't write any of the formats");
        }
    }

    void aNameWithBracketsIsNotAFrameOrAPattern()
    {
        if (m_magick.isEmpty() || !formats::extra().contains(QStringLiteral("avif"))) {
            QSKIP("no AVIF through ImageMagick");
        }
        const QString path = make(QStringLiteral("holiday [1] *.avif"));
        if (path.isEmpty()) {
            QSKIP("can't write AVIF");
        }
        QCOMPARE(formats::read(path).size(), QSize(40, 30));
    }

    void asksTheDecoderForASmallOne()
    {
        if (m_magick.isEmpty() || !formats::extra().contains(QStringLiteral("avif"))) {
            QSKIP("no AVIF through ImageMagick");
        }
        const QString path = make(QStringLiteral("small.avif"));
        if (path.isEmpty()) {
            QSKIP("can't write AVIF");
        }
        const QImage image = formats::read(path, QSize(20, 20));
        QVERIFY(!image.isNull());
        QVERIFY(image.width() <= 20 && image.height() <= 20);
    }

    void aBrokenFileIsAnError()
    {
        if (m_magick.isEmpty()) {
            QSKIP("ImageMagick isn't installed");
        }
        QFile file(m_dir.filePath(QStringLiteral("broken.avif")));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("this is not an image");
        file.close();
        QString error;
        QVERIFY(formats::read(file.fileName(), {}, &error).isNull());
        QVERIFY(!error.isEmpty());
    }

    void showsFilesQtReadsAsTheyAre()
    {
        const QUrl png = QUrl::fromLocalFile(QStringLiteral("/tmp/a.png"));
        QCOMPARE(formats::shown(png), png);
        if (formats::extra().contains(QStringLiteral("avif"))) {
            const QUrl shown = formats::shown(QUrl::fromLocalFile(QStringLiteral("/tmp/a.avif")));
            QCOMPARE(shown.scheme(), QStringLiteral("image"));
            QCOMPARE(shown.host(), QStringLiteral("photo"));
        }
    }

    void theProviderDecodesAndKeepsTheSize()
    {
        if (m_magick.isEmpty() || !formats::extra().contains(QStringLiteral("avif"))) {
            QSKIP("no AVIF through ImageMagick");
        }
        const QString path = make(QStringLiteral("provided.avif"));
        if (path.isEmpty()) {
            QSKIP("can't write AVIF");
        }
        const QUrl shown = formats::shown(QUrl::fromLocalFile(path));
        const QString id = shown.path().mid(1); // after the leading "/"
        FormatImages provider(nullptr);
        QSize size;
        const QImage image = provider.requestImage(id, &size, QSize());
        QCOMPARE(image.size(), QSize(40, 30));
        QCOMPARE(size, QSize(40, 30));
    }
};

QTEST_GUILESS_MAIN(FormatsTest)
#include "imageformats.moc"
