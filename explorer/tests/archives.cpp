// SPDX-FileCopyrightText: 2026 Brando Giuffrida
// SPDX-License-Identifier: GPL-3.0-or-later

// Tests of Explorer's archives, without a UI: "Extract here" (into a folder
// named after the archive, next to it, never overwriting one that exists) and
// "Compress" (the chosen name, several items as paths relative to the
// folder). The work is external (tar, 7z, unzip, zip), so each test waits for
// the tool's answer by signal.
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
