// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// Which files of a folder the Media Player goes through, and in what order.

#include "playlist.h"

#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

class PlaylistTest : public QObject {
    Q_OBJECT

    QTemporaryDir m_dir;

    QUrl url(const QString& name) const { return QUrl::fromLocalFile(m_dir.filePath(name)); }

private slots:
    void initTestCase()
    {
        for (const char* name : { "e10.mkv", "e2.mkv", "Song.MP3", "e2.srt", "clip" }) {
            QFile file(m_dir.filePath(QString::fromLatin1(name)));
            QVERIFY(file.open(QIODevice::WriteOnly));
        }
    }

    void opensAFileWithItsFolder()
    {
        Playlist list;
        list.open(url("e10.mkv"));
        QCOMPARE(list.files(), (QList<QUrl> { url("e2.mkv"), url("e10.mkv"), url("Song.MP3") }));
        QCOMPARE(list.index(), 1);
        QCOMPARE(list.name(), "e10.mkv");
        list.setIndex(3); // past the end: stays
        QCOMPARE(list.index(), 1);
    }

    void opensAFolder()
    {
        Playlist list;
        list.open(QUrl::fromLocalFile(m_dir.path()));
        QCOMPARE(list.index(), 0);
        QCOMPARE(list.name(), "e2.mkv");
    }

    void playsWhatIsOpenedByHand()
    {
        Playlist list;
        list.open(url("clip")); // no extension: still in the list
        QCOMPARE(list.files().size(), 4);
        QCOMPARE(list.name(), "clip");
    }
};

QTEST_GUILESS_MAIN(PlaylistTest)
#include "playlist.moc"
