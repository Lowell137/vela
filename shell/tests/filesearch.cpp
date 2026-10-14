// SPDX-FileCopyrightText: 2026 Brando Giuffrida
// SPDX-License-Identifier: GPL-3.0-or-later

#include "../src/filesearch.h"
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

class FileSearchTest : public QObject {
    Q_OBJECT

private slots:
    void matchesNames();
    void skipsHiddenAndModules();
    void respectsDepthAndLimit();
    void filesBeforeFolders();
};

static void touch(const QString& path)
{
    QVERIFY(QDir().mkpath(QFileInfo(path).path()));
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
}

void FileSearchTest::matchesNames()
{
    QTemporaryDir dir;
    touch(dir.filePath(QStringLiteral("docs/Budget-2026.ods")));
    touch(dir.filePath(QStringLiteral("other.txt")));
    std::atomic_bool cancel(false);
    const QStringList r = FileSearch::scan(dir.path(), QStringLiteral("budget"), 6, 8, 1000, cancel);
    QCOMPARE(r.size(), 1);
    QVERIFY(r.first().endsWith(QLatin1String("Budget-2026.ods")));
}

void FileSearchTest::skipsHiddenAndModules()
{
    QTemporaryDir dir;
    touch(dir.filePath(QStringLiteral(".hidden/secret.txt")));
    touch(dir.filePath(QStringLiteral("node_modules/secret.js")));
    touch(dir.filePath(QStringLiteral("a/secret.md")));
    std::atomic_bool cancel(false);
    const QStringList r = FileSearch::scan(dir.path(), QStringLiteral("secret"), 6, 8, 1000, cancel);
    QCOMPARE(r.size(), 1);
    QVERIFY(r.first().endsWith(QLatin1String("a/secret.md")));
}

void FileSearchTest::respectsDepthAndLimit()
{
    QTemporaryDir dir;
    touch(dir.filePath(QStringLiteral("a/b/c/deep.txt")));
    for (int i = 0; i < 5; ++i) {
        touch(dir.filePath(QStringLiteral("f%1.log").arg(i)));
    }
    std::atomic_bool cancel(false);
    QVERIFY(FileSearch::scan(dir.path(), QStringLiteral("deep"), 2, 8, 1000, cancel).isEmpty());
    QCOMPARE(FileSearch::scan(dir.path(), QStringLiteral("deep"), 6, 8, 1000, cancel).size(), 1);
    QCOMPARE(FileSearch::scan(dir.path(), QStringLiteral(".log"), 6, 3, 1000, cancel).size(), 3);
}

void FileSearchTest::filesBeforeFolders()
{
    QTemporaryDir dir;
    touch(dir.filePath(QStringLiteral("capture/x.txt")));
    touch(dir.filePath(QStringLiteral("a/capture/x.txt")));
    touch(dir.filePath(QStringLiteral("zz/capture.png")));
    std::atomic_bool cancel(false);
    const QStringList r = FileSearch::scan(dir.path(), QStringLiteral("capture"), 6, 5, 1000, cancel);
    QCOMPARE(r.size(), 3);
    QVERIFY(r.first().endsWith(QLatin1String("capture.png")));
}

QTEST_MAIN(FileSearchTest)
#include "filesearch.moc"
