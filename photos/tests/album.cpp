// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// Which files of a folder Photos goes through, and in what order.

#include "album.h"

#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

class AlbumTest : public QObject {
    Q_OBJECT

    QTemporaryDir m_dir;

    QUrl url(const QString& name) const { return QUrl::fromLocalFile(m_dir.filePath(name)); }

private slots:
    void initTestCase()
    {
        for (const char* name : { "a10.png", "a2.png", "B1.JPG", "notes.txt", "scan" }) {
            QFile file(m_dir.filePath(QString::fromLatin1(name)));
            QVERIFY(file.open(QIODevice::WriteOnly));
        }
    }

    void opensAFileWithItsFolder()
    {
        Album album;
        album.open(url("a10.png"));
        QCOMPARE(album.files(), (QList<QUrl> { url("a2.png"), url("a10.png"), url("B1.JPG") }));
        QCOMPARE(album.index(), 1);
        QCOMPARE(album.name(), "a10.png");
        album.setIndex(7); // past the end: stays
        QCOMPARE(album.index(), 1);
    }

    void opensAFolder()
    {
        Album album;
        album.open(QUrl::fromLocalFile(m_dir.path()));
        QCOMPARE(album.files().size(), 3);
        QCOMPARE(album.name(), "a2.png");
    }

    void showsAFileThatIsntNamedLikeAnImage()
    {
        Album album;
        album.open(url("scan"));
        QCOMPARE(album.files().size(), 4);
        QCOMPARE(album.name(), "scan");
    }
};

QTEST_MAIN(AlbumTest)
#include "album.moc"
