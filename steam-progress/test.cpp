// SPDX-FileCopyrightText: 2026 Brando Giuffrida
// SPDX-License-Identifier: GPL-3.0-or-later

// steam-progress: the fraction from a fake Steam library (manifests and staged
// files), no Steam and no D-Bus.

#include "progress.h"

#include <QTemporaryDir>
#include <QTest>

namespace {
void manifest(const QString& lib, int appid, int flags, qint64 toStage, qint64 staged)
{
    QDir().mkpath(lib + "/steamapps");
    QFile f(lib + QString("/steamapps/appmanifest_%1.acf").arg(appid));
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(QString("\"AppState\"\n{\n\t\"StateFlags\"\t\"%1\"\n\t\"BytesToDownload\"\t\"1\"\n\t\"BytesDownloaded\"\t\"1\"\n"
                    "\t\"BytesToStage\"\t\"%2\"\n\t\"BytesStaged\"\t\"%3\"\n}\n")
                .arg(flags)
                .arg(toStage)
                .arg(staged)
                .toUtf8());
}
void staged(const QString& lib, int appid, int bytes)
{
    const QString dir = lib + QString("/steamapps/downloading/%1/Content").arg(appid);
    QDir().mkpath(dir);
    QFile f(dir + "/file.bin");
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(QByteArray(bytes, 'x'));
}
}

class SteamProgressTest : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void nothingDownloading()
    {
        QTemporaryDir d;
        QCOMPARE(steamprogress::progress(d.path()), -1.0);
        manifest(d.path(), 1, 4, 1000, 1000); // installed, no update running
        QCOMPARE(steamprogress::progress(d.path()), -1.0);
    }
    void runningUsesTheManifest()
    {
        QTemporaryDir d;
        manifest(d.path(), 1, 1026, 1000, 250);
        QCOMPARE(steamprogress::progress(d.path()), 0.25);
    }
    void liveFolderBeatsAStaleManifest()
    {
        // Steam's window says 50% while the manifest still says 10%.
        QTemporaryDir d;
        manifest(d.path(), 1, 1026, 1000, 100);
        staged(d.path(), 1, 500);
        QCOMPARE(steamprogress::progress(d.path()), 0.5);
    }
    void severalDownloadsAreOneFraction()
    {
        QTemporaryDir d;
        manifest(d.path(), 1, 1026, 1000, 500);
        manifest(d.path(), 2, 1026, 3000, 0);
        manifest(d.path(), 3, 4, 9999, 0); // finished: not counted
        QCOMPARE(steamprogress::progress(d.path()), 500.0 / 4000.0);
    }
    void otherLibraryIsRead()
    {
        QTemporaryDir d;
        QTemporaryDir other;
        QDir().mkpath(d.path() + "/steamapps");
        QFile vdf(d.path() + "/steamapps/libraryfolders.vdf");
        QVERIFY(vdf.open(QIODevice::WriteOnly));
        vdf.write(QString("\"libraryfolders\"\n{\n\t\"0\"\n\t{\n\t\t\"path\"\t\t\"%1\"\n\t}\n}\n").arg(other.path()).toUtf8());
        vdf.close();
        manifest(other.path(), 7, 1026, 400, 100);
        QCOMPARE(steamprogress::progress(d.path()), 0.25);
    }
    void neverAboveOne()
    {
        QTemporaryDir d;
        manifest(d.path(), 1, 1026, 100, 0);
        staged(d.path(), 1, 400); // the folder can hold more than the manifest's target
        QCOMPARE(steamprogress::progress(d.path()), 1.0);
    }
};

QTEST_MAIN(SteamProgressTest)
#include "test.moc"
