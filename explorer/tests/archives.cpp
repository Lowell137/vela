// SPDX-FileCopyrightText: 2026 Brando Giuffrida
// SPDX-License-Identifier: GPL-3.0-or-later

// Tests of Explorer's archives, without a UI: "Extract here" (into a folder
// named after the archive, next to it, never overwriting one that exists) and
// "Compress" (the chosen name, several items as paths relative to the
// folder). Extract and Compress are libarchive on a worker thread, so each
// test waits for the answer by signal.
//
//   ctest --test-dir build -R files-archives

#include "appmodel.h"
#include "fileops.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QProcess>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

#include <memory>

namespace {

void write(const QString& path, const QByteArray& data)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    QCOMPARE(file.write(data), data.size());
}

QByteArray read(const QString& path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray("<illeggibile>");
}

// An archive made the same way the app makes it: with real tools, absolute
// paths. false means "the tool isn't installed here".
bool makeArchive(const QString& directory, const QString& program, const QStringList& arguments)
{
    if (QStandardPaths::findExecutable(program).isEmpty()) {
        return false;
    }
    QProcess tool;
    tool.setWorkingDirectory(directory);
    tool.start(program, arguments);
    const bool finished = tool.waitForFinished(30000);
    return finished && tool.exitStatus() == QProcess::NormalExit && tool.exitCode() == 0;
}

// What the listing tool answers (the tests read it instead of the app's
// parsing).
void toolOutput(const QString& program, const QStringList& arguments, QString& output)
{
    QProcess tool;
    tool.start(program, arguments);
    QVERIFY(tool.waitForFinished(30000));
    output = QString::fromLocal8Bit(tool.readAllStandardOutput());
}

} // namespace

class Archives : public QObject {
    Q_OBJECT

    AppModel m_apps;
    FileOps* m_ops = nullptr;
    std::unique_ptr<QTemporaryDir> m_dir;

    QString path(const QString& relative) const { return m_dir->filePath(relative); }

    // The archive work runs in a QProcess: the test sleeps until "created"
    // (it went well) or "failed" arrives.
    void settle(QSignalSpy& created, QSignalSpy& failed, bool& ok)
    {
        QTRY_VERIFY_WITH_TIMEOUT(created.size() > 0 || failed.size() > 0, 30000);
        ok = created.size() > 0;
        if (!ok) {
            QWARN(qPrintable(failed.value(0).toList().value(0).toString()));
        }
    }

private slots:
    void init()
    {
        m_dir = std::make_unique<QTemporaryDir>();
        QVERIFY(m_dir->isValid());
        m_ops = new FileOps(&m_apps);
    }

    void cleanup()
    {
        delete m_ops;
        m_dir.reset();
    }

    // The double click and Enter extract next to the archive and Files goes
    // in: the returned folder must be it, with the content unpacked.
    void extractReturnsTheFolderAndUnpacks()
    {
        write(path("sorgente/foto.txt"), "anno 2026");
        if (!makeArchive(path("."), QStringLiteral("tar"),
                { QStringLiteral("-czf"), QStringLiteral("foto.tar.gz"), QStringLiteral("--"),
                    QStringLiteral("sorgente") })) {
            QSKIP("tar isn't installed");
        }
        QSignalSpy created(m_ops, &FileOps::created);
        QSignalSpy failed(m_ops, &FileOps::failed);
        QCOMPARE(m_ops->extractAll(path("foto.tar.gz")), path("foto"));
        bool ok = false;
        settle(created, failed, ok);
        QVERIFY(ok);
        QVERIFY(QFile::exists(path("foto/sorgente/foto.txt")));
        QCOMPARE(read(path("foto/sorgente/foto.txt")), QByteArray("anno 2026"));
    }
    // zip and 7z are read by the app itself (libarchive), and an entry like
    // "../x" can't leave the folder it extracts into.
    void extractZipAnd7zEmbedded_data()
    {
        QTest::addColumn<QString>("name");
        QTest::addColumn<QString>("type");
        QTest::newRow("zip") << "doc.zip" << "-tzip";
        QTest::newRow("7z") << "doc.7z" << "-t7z";
    }
    void extractZipAnd7zEmbedded()
    {
        QFETCH(QString, name);
        QFETCH(QString, type);
        write(path("src/sub/a.txt"), "ciao");
        if (!makeArchive(path("src"), QStringLiteral("7z"),
                { QStringLiteral("a"), type, QStringLiteral("../") + name, QStringLiteral("sub") })) {
            QSKIP("7z isn't installed");
        }
        QSignalSpy created(m_ops, &FileOps::created);
        QSignalSpy failed(m_ops, &FileOps::failed);
        m_ops->extractAll(path(name));
        bool ok = false;
        settle(created, failed, ok);
        QVERIFY(ok);
        QCOMPARE(read(path(QFileInfo(name).completeBaseName() + QStringLiteral("/sub/a.txt"))), QByteArray("ciao"));
    }
    void extractStaysInsideTheFolder()
    {
        if (!makeArchive(path("."), QStringLiteral("python3"),
                { QStringLiteral("-c"),
                    QStringLiteral("import zipfile; z=zipfile.ZipFile('evil.zip','w'); "
                                   "z.writestr('../escape.txt','x'); z.writestr('ok.txt','y'); z.close()") })) {
            QSKIP("python3 isn't installed");
        }
        QSignalSpy created(m_ops, &FileOps::created);
        QSignalSpy failed(m_ops, &FileOps::failed);
        m_ops->extractAll(path("evil.zip"));
        QTRY_VERIFY_WITH_TIMEOUT(created.size() > 0 || failed.size() > 0, 30000);
        QVERIFY(!QFile::exists(path("escape.txt")));
    }
    // "Paste here" numbering: the folder that exists isn't overwritten, the
    // second extract takes " (2)".
    void extractNeverOverwrites()
    {
        write(path("foto/content.txt"), "vecchia");
        if (!makeArchive(path("."), QStringLiteral("tar"),
                { QStringLiteral("-czf"), QStringLiteral("foto.tar.gz"), QStringLiteral("--"),
                    QStringLiteral("foto") })) {
            QSKIP("tar isn't installed");
        }
        QSignalSpy created(m_ops, &FileOps::created);
        QSignalSpy failed(m_ops, &FileOps::failed);
        QCOMPARE(m_ops->extractAll(path("foto.tar.gz")), path("foto (2)"));
        bool ok = false;
        settle(created, failed, ok);
        QVERIFY(ok);
        QCOMPARE(read(path("foto/content.txt")), QByteArray("vecchia"));
        // tar keeps the paths relative to the folder: "foto/content.txt"
        // unpacks under the target.
        QVERIFY(QFile::exists(path("foto (2)/foto/content.txt")));
    }

    // The dialog's name and ZIP; with several items selected their paths
    // stay relative to the folder (the archive opens fine elsewhere).
    void compressZipWithTheChosenName()
    {
        write(path("f1.txt"), "uno");
        write(path("cartella/f2.txt"), "due");
        QSignalSpy created(m_ops, &FileOps::created);
        QSignalSpy failed(m_ops, &FileOps::failed);
        m_ops->compress({ path("f1.txt"), path("cartella") }, path("."), QStringLiteral("pacchetto"),
            QStringLiteral("zip"));
        bool ok = false;
        settle(created, failed, ok);
        QVERIFY(ok);
        if (!QStandardPaths::findExecutable(QStringLiteral("unzip")).isEmpty()) {
            QString listing;
            toolOutput(QStringLiteral("unzip"), { QStringLiteral("-l"), path("pacchetto.zip") }, listing);
            QVERIFY2(listing.contains("f1.txt"), qPrintable(listing));
            QVERIFY2(listing.contains("cartella/f2.txt"), qPrintable(listing));
        }
    }

    // An archive named like an existing file gets " (2)", like pasting.
    void compressTakenNamesGet2()
    {
        write(path("esiste.txt"), "altro");
        // The name is taken by another archive, like pasting.
        write(path("esiste.zip"), "tutto");
        write(path("f1.txt"), "uno");
        QSignalSpy created(m_ops, &FileOps::created);
        QSignalSpy failed(m_ops, &FileOps::failed);
        m_ops->compress({ path("f1.txt") }, path("."), QStringLiteral("esiste"), QStringLiteral("zip"));
        bool ok = false;
        settle(created, failed, ok);
        QVERIFY(ok);
        QTRY_VERIFY(QFile::exists(path("esiste (2).zip")));
        QCOMPARE(read(path("esiste.zip")), QByteArray("tutto"));
        QCOMPARE(read(path("esiste.txt")), QByteArray("altro"));
    }

    // 7z is written by the app itself: a folder with a file inside, listed
    // and read back by the real 7z.
    void compress7zAndRoundTrip()
    {
        write(path("f1.txt"), "uno");
        write(path("cartella/sub/f2.txt"), "due");
        QSignalSpy created(m_ops, &FileOps::created);
        QSignalSpy failed(m_ops, &FileOps::failed);
        m_ops->compress({ path("f1.txt"), path("cartella") }, path("."), QStringLiteral("pacchetto"),
            QStringLiteral("7z"));
        bool ok = false;
        settle(created, failed, ok);
        QVERIFY(ok);
        QVERIFY(QFile::exists(path("pacchetto.7z")));
        if (!QStandardPaths::findExecutable(QStringLiteral("7z")).isEmpty()) {
            QString listing;
            toolOutput(QStringLiteral("7z"), { QStringLiteral("l"), path("pacchetto.7z") }, listing);
            QVERIFY2(listing.contains("f1.txt"), qPrintable(listing));
            QVERIFY2(listing.contains("cartella/sub/f2.txt") || listing.contains("cartella\\sub\\f2.txt"), qPrintable(listing));
        }
        // And the app's own extract reads it back.
        QDir().mkpath(path("fuori"));
        QFile::copy(path("pacchetto.7z"), path("fuori/pacchetto.7z"));
        QSignalSpy created2(m_ops, &FileOps::created);
        QSignalSpy failed2(m_ops, &FileOps::failed);
        m_ops->extractAll(path("fuori/pacchetto.7z"));
        settle(created2, failed2, ok);
        QVERIFY(ok);
        QCOMPARE(read(path("fuori/pacchetto/f1.txt")), QByteArray("uno"));
        QCOMPARE(read(path("fuori/pacchetto/cartella/sub/f2.txt")), QByteArray("due"));
    }

    // zip made by the app unpacks with the real unzip/7z (a real zip).
    void compressZipRoundTrip()
    {
        write(path("cartella/f2.txt"), "due");
        QSignalSpy created(m_ops, &FileOps::created);
        QSignalSpy failed(m_ops, &FileOps::failed);
        m_ops->compress({ path("cartella") }, path("."), QStringLiteral("z"), QStringLiteral("zip"));
        bool ok = false;
        settle(created, failed, ok);
        QVERIFY(ok);
        QDir().mkpath(path("out"));
        if (makeArchive(path("."), QStringLiteral("7z"),
                { QStringLiteral("x"), QStringLiteral("-oout"), QStringLiteral("z.zip") })) {
            QCOMPARE(read(path("out/cartella/f2.txt")), QByteArray("due"));
        }
    }

    // tar.gz goes to tar.
    void compressTarGz()
    {
        write(path("f1.txt"), "uno");
        write(path("f2.txt"), "due");
        QSignalSpy created(m_ops, &FileOps::created);
        QSignalSpy failed(m_ops, &FileOps::failed);
        m_ops->compress({ path("f1.txt"), path("f2.txt") }, path("."), QStringLiteral("pacchetto"),
            QStringLiteral("tar.gz"));
        bool ok = false;
        settle(created, failed, ok);
        QVERIFY(ok);
        if (!QStandardPaths::findExecutable(QStringLiteral("tar")).isEmpty()) {
            QString listing;
            toolOutput(QStringLiteral("tar"), { QStringLiteral("-tzf"), path("pacchetto.tar.gz") }, listing);
            QVERIFY2(listing.contains("f1.txt"), qPrintable(listing));
            QVERIFY2(listing.contains("f2.txt"), qPrintable(listing));
        }
    }
};

int main(int argc, char* argv[])
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QGuiApplication app(argc, argv);
    Archives test;
    return QTest::qExec(&test, argc, argv);
}

#include "archives.moc"
