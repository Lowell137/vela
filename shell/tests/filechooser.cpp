// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// The FileChooser portal backend over a private session bus, with a fake
// Explorer (a shell script): the request it is given, the answer it prints,
// Cancel, a program that doesn't start, and Close.

#include "filechooser.h"
#include "globalshortcuts.h"

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusMetaType>
#include <QDBusPendingCall>
#include <QDBusPendingReply>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

struct Rule {
    uint kind;
    QString pattern;
};
struct Filter {
    QString name;
    QList<Rule> rules;
};
Q_DECLARE_METATYPE(Rule)
Q_DECLARE_METATYPE(QList<Rule>)
Q_DECLARE_METATYPE(Filter)
Q_DECLARE_METATYPE(QList<Filter>)
QDBusArgument& operator<<(QDBusArgument& a, const Rule& r)
{
    a.beginStructure();
    a << r.kind << r.pattern;
    a.endStructure();
    return a;
}
const QDBusArgument& operator>>(const QDBusArgument& a, Rule& r)
{
    a.beginStructure();
    a >> r.kind >> r.pattern;
    a.endStructure();
    return a;
}
QDBusArgument& operator<<(QDBusArgument& a, const Filter& f)
{
    a.beginStructure();
    a << f.name << f.rules;
    a.endStructure();
    return a;
}
const QDBusArgument& operator>>(const QDBusArgument& a, Filter& f)
{
    a.beginStructure();
    a >> f.name >> f.rules;
    a.endStructure();
    return a;
}

class FileChooserTest : public QObject {
    Q_OBJECT
    QTemporaryDir m_dir;

    QString script(const QString& body)
    {
        const QString path = m_dir.filePath(QStringLiteral("fake-%1.sh").arg(QTest::currentTestFunction()));
        QFile file(path);
        file.open(QIODevice::WriteOnly);
        file.write("#!/bin/sh\n" + body.toUtf8() + "\n");
        file.setPermissions(QFile::permissions(path) | QFile::ExeOwner);
        return path;
    }

    // The app: another connection, or Qt would call the object directly.
    QVariantList call(const QString& method, const QString& handle, const QVariantMap& options, int timeout = 5000)
    {
        QDBusConnection bus = QDBusConnection::connectToBus(QDBusConnection::SessionBus, QStringLiteral("app-") + method);
        QDBusMessage m = QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.impl.portal.desktop.vela"),
            QStringLiteral("/org/freedesktop/portal/desktop"), QStringLiteral("org.freedesktop.impl.portal.FileChooser"), method);
        m.setArguments({ QVariant::fromValue(QDBusObjectPath(handle)), QStringLiteral("org.app"), QString(), QStringLiteral("Open"), options });
        QDBusPendingCall pending = bus.asyncCall(m, timeout);
        QSignalSpy spy(new QDBusPendingCallWatcher(pending, this), &QDBusPendingCallWatcher::finished);
        spy.wait(timeout);
        return QDBusPendingReply<>(pending).reply().arguments();
    }

private Q_SLOTS:
    void initTestCase()
    {
        qDBusRegisterMetaType<Rule>();
        qDBusRegisterMetaType<QList<Rule>>();
        qDBusRegisterMetaType<Filter>();
        qDBusRegisterMetaType<QList<Filter>>();
    }

    void answers()
    {
        GlobalShortcuts gs;
        auto* chooser = new FileChooser(&gs);
        chooser->setProgram(script(QStringLiteral("cp \"$2\" %1/request.json\necho '{\"uris\":[\"file:///tmp/a.png\",\"file:///tmp/b b.png\"],\"filter\":1}'").arg(m_dir.path())));
        if (!gs.registerService()) {
            QSKIP("no private session bus");
        }
        const QList<Filter> filters { { "Images", { { 0, "*.png" }, { 0, "*.jpg" } } }, { "All", { { 0, "*" } } } };
        const QVariantMap options { { "multiple", true }, { "current_folder", QByteArray("/home\0", 6) }, { "filters", QVariant::fromValue(filters) } };
        const QVariantList reply = call(QStringLiteral("OpenFile"), QStringLiteral("/org/freedesktop/portal/desktop/request/x/1"), options);
        QCOMPARE(reply.size(), 2);
        QCOMPARE(reply.at(0).toUInt(), 0u);
        const QVariantMap results = qdbus_cast<QVariantMap>(reply.at(1));
        QCOMPARE(results.value("uris").toStringList(), QStringList({ "file:///tmp/a.png", "file:///tmp/b b.png" }));

        QFile file(m_dir.filePath("request.json"));
        QVERIFY(file.open(QIODevice::ReadOnly));
        const QJsonObject request = QJsonDocument::fromJson(file.readAll()).object();
        QCOMPARE(request.value("mode").toString(), QString("open"));
        QCOMPARE(request.value("folder").toString(), QString("/home"));
        QVERIFY(request.value("multiple").toBool());
        QCOMPARE(request.value("filters").toArray().size(), 2);
        QCOMPARE(request.value("filters").toArray().at(0).toObject().value("globs").toArray().at(1).toString(), QString("*.jpg"));

        // Cancel: it exits without an answer.
        chooser->setProgram(script(QStringLiteral("exit 1")));
        const QVariantList cancelled = call(QStringLiteral("SaveFile"), QStringLiteral("/org/freedesktop/portal/desktop/request/x/2"), {});
        QCOMPARE(cancelled.at(0).toUInt(), 1u);

        // A program that isn't there: "other" error.
        chooser->setProgram(QStringLiteral("/nonexistent/vela-files"));
        const QVariantList failed = call(QStringLiteral("OpenFile"), QStringLiteral("/org/freedesktop/portal/desktop/request/x/3"), {});
        QCOMPARE(failed.at(0).toUInt(), 2u);
    }

    void requestOptions()
    {
        const QVariantMap options { { "current_name", "doc.txt" }, { "current_file", QByteArray("/data/old.txt\0", 14) }, { "directory", true } };
        const QJsonObject request = FileChooser::requestFor(QStringLiteral("save"), QStringLiteral("Save"), options);
        QCOMPARE(request.value("name").toString(), QString("doc.txt"));
        QCOMPARE(request.value("folder").toString(), QString("/data")); // from current_file
        QVERIFY(request.value("directory").toBool());
        QCOMPARE(request.value("filter").toInt(), 0);
    }
};

QTEST_MAIN(FileChooserTest)
#include "filechooser.moc"
