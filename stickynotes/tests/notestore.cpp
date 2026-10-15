// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "notestore.h"

#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

class NoteStoreTest : public QObject
{
    Q_OBJECT

    QTemporaryDir dir;
    QString path() const { return dir.filePath(QStringLiteral("sub/notes.json")); }

private slots:
    void init() { QFile::remove(path()); QFile::remove(path() + ".bad"); }

    void roundTrip()
    {
        int id;
        {
            NoteStore store(path());
            id = store.create("green");
            store.setText(id, "Milk\nEggs");
            store.setGeometry(id, 10, 20, 400, 250);
            store.flush();
        }
        NoteStore again(path());
        QCOMPARE(again.count(), 1);
        const QVariantMap n = again.get(id);
        QCOMPARE(n["text"].toString(), QString("Milk\nEggs"));
        QCOMPARE(n["color"].toString(), QString("green"));
        QCOMPARE(n["width"].toInt(), 400);
        QCOMPARE(n["y"].toInt(), 20);
        QVERIFY(n["open"].toBool());
        QCOMPARE(again.index(0, 0).data(NoteStore::PreviewRole).toString(), QString("Milk"));
    }

    void corruptFileIsKept()
    {
        QDir().mkpath(QFileInfo(path()).absolutePath());
        QFile f(path());
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("{ not json");
        f.close();
        NoteStore store(path());
        QCOMPARE(store.count(), 0);
        QVERIFY(QFile::exists(path() + ".bad"));
        QVERIFY(!QFile::exists(path()));
        store.create("yellow");
        store.flush();
        QVERIFY(QFile::exists(path()));
    }

    void debounceAndFlush()
    {
        NoteStore store(path(), 200);
        const int id = store.create("yellow");
        store.setText(id, "a");
        QVERIFY(store.dirty());
        QTest::qWait(120);
        store.setText(id, "ab"); // restarts the wait
        QTest::qWait(120);
        QVERIFY(store.dirty());
        QVERIFY(!QFile::exists(path()));
        QTest::qWait(250);
        QVERIFY(!store.dirty());
        QVERIFY(QFile::exists(path()));
        store.setText(id, "abc");
        store.flush(); // at once
        QVERIFY(!store.dirty());
        NoteStore again(path());
        QCOMPARE(again.get(id)["text"].toString(), QString("abc"));
    }

    void ids()
    {
        NoteStore store(path());
        const int a = store.create("");
        const int b = store.create("nonsense");
        QVERIFY(a != b);
        QCOMPARE(store.get(b)["color"].toString(), QString("yellow"));
        store.remove(b);
        QVERIFY(store.create("pink") > a); // never reuses a live id
        // Loaded duplicates and missing ids get new ones.
        QDir().mkpath(QFileInfo(path()).absolutePath());
        QFile f(path());
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(R"({"notes":[{"id":4,"text":"x"},{"id":4,"text":"y"},{"text":"z"},7]})");
        f.close();
        NoteStore loaded(path());
        QCOMPARE(loaded.count(), 3);
        QSet<int> seen;
        for (const auto& n : loaded.notes()) {
            QVERIFY(n.id > 0);
            seen.insert(n.id);
        }
        QCOMPARE(seen.size(), 3);
    }

    void openAndQuitting()
    {
        NoteStore store(path());
        const int id = store.create("blue");
        QCOMPARE(store.openIds().size(), 1);
        store.setQuitting(true);
        store.setOpen(id, false); // windows going away at exit
        QCOMPARE(store.openIds().size(), 1);
        store.setQuitting(false);
        store.setOpen(id, false);
        QCOMPARE(store.openIds().size(), 0);
    }

    void filter()
    {
        QVERIFY(NoteStore::matches("Buy MILK", " milk "));
        QVERIFY(NoteStore::matches("anything", "  "));
        QVERIFY(!NoteStore::matches("eggs", "milk"));
        NoteStore store(path());
        store.setText(store.create("gray"), "milk run");
        store.setText(store.create("gray"), "eggs");
        store.setFilter("milk");
        QCOMPARE(store.rowCount(), 1);
        QCOMPARE(store.index(0, 0).data(NoteStore::TextRole).toString(), QString("milk run"));
        store.setFilter("");
        QCOMPARE(store.rowCount(), 2);
        QCOMPARE(store.index(0, 0).data(NoteStore::TextRole).toString(), QString("eggs")); // newest first
    }
};

QTEST_GUILESS_MAIN(NoteStoreTest)
#include "notestore.moc"
