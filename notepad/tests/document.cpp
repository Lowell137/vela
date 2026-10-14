// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// Notepad must write back what it read: a file opened and saved untouched
// keeps its bytes, whatever its encoding and line endings.

#include "document.h"

#include <QtTest>

class DocumentTest : public QObject {
    Q_OBJECT

private slots:
    void roundTrip_data()
    {
        QTest::addColumn<QByteArray>("bytes");
        QTest::addColumn<QString>("encoding");
        QTest::addColumn<QString>("lineEnding");
        QTest::addColumn<QString>("text");

        const QString text = QStringLiteral("İstanbul\nçay €\n");
        QTest::newRow("utf8 lf") << text.toUtf8() << "UTF-8" << "LF" << text;
        QTest::newRow("utf8 crlf") << QByteArray("a\r\nb\r\n") << "UTF-8" << "CRLF" << "a\nb\n";
        QTest::newRow("utf8 cr") << QByteArray("a\rb") << "UTF-8" << "CR" << "a\nb";
        QTest::newRow("utf8 bom") << QByteArray("\xEF\xBB\xBF") + text.toUtf8() << "UTF-8 with BOM" << "LF" << text;
        QTest::newRow("utf16 le") << QByteArray("\xFF\xFE" "a\0\r\0\n\0\x30\x01", 10) << "UTF-16 LE" << "CRLF" << QStringLiteral("a\nİ");
        QTest::newRow("utf16 be") << QByteArray("\xFE\xFF" "\0a\0\n\x01\x30", 8) << "UTF-16 BE" << "LF" << QStringLiteral("a\nİ");
        QTest::newRow("latin1") << QByteArray("caf\xE9\n\xFF\x80") << "ISO-8859-1" << "LF" << QString::fromLatin1("caf\xE9\n\xFF\x80");
        QTest::newRow("empty") << QByteArray() << "UTF-8" << "LF" << QString();
    }

    void roundTrip()
    {
        QFETCH(QByteArray, bytes);
        QFETCH(QString, encoding);
        QFETCH(QString, lineEnding);
        QFETCH(QString, text);

        QString foundEncoding;
        QString foundLineEnding;
        const QString decoded = Document::decode(bytes, &foundEncoding, &foundLineEnding);
        QCOMPARE(foundEncoding, encoding);
        QCOMPARE(foundLineEnding, lineEnding);
        QCOMPARE(decoded, text);

        bool ok = false;
        QCOMPARE(Document::encode(decoded, foundEncoding, foundLineEnding, &ok), bytes);
        QVERIFY(ok);
    }

    // Saving must refuse rather than write "?" in place of a character.
    void latin1RefusesWhatItCantHold()
    {
        bool ok = true;
        Document::encode(QStringLiteral("İ"), QStringLiteral("ISO-8859-1"), QStringLiteral("LF"), &ok);
        QVERIFY(!ok);
    }
};

QTEST_GUILESS_MAIN(DocumentTest)
#include "document.moc"
