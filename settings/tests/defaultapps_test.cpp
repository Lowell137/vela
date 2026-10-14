// SPDX-FileCopyrightText: 2026 Brando Giuffrida
// SPDX-License-Identifier: GPL-3.0-or-later

// Terminal and Task manager candidates come from the installed .desktop files.

#include "appmodel.h"
#include "defaultapps.h"
#include "preferences.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

// DefaultApps reads these two; the test has no Preferences to construct.
QString Preferences::compositorValue(const QString&) const { return {}; }
void Preferences::setCompositorValue(const QString&, const QString&) { }

namespace {

void desktop(const QString& dir, const QString& id, const QByteArray& body)
{
    QDir().mkpath(dir);
    QFile f(dir + u'/' + id + QStringLiteral(".desktop"));
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("[Desktop Entry]\nType=Application\n" + body);
}

QStringList names(const QVariantMap& category)
{
    QStringList out;
    for (const QVariant& c : category.value(QStringLiteral("candidates")).toList()) {
        out << c.toMap().value(QStringLiteral("name")).toString();
    }
    return out;
}

} // namespace

class DefaultAppsTest : public QObject {
    Q_OBJECT
private slots:
    void toolsComeFromDesktopFiles()
    {
        QTemporaryDir data;
        QVERIFY(data.isValid());
        qputenv("XDG_DATA_HOME", data.path().toUtf8());
        qputenv("XDG_DATA_DIRS", data.path().toUtf8() + "/none");
        const QString dir = data.path() + QStringLiteral("/applications");
        desktop(dir, QStringLiteral("ghostty"), "Name=Ghostty\nExec=/usr/bin/ghostty --gtk-single-instance=true\nCategories=System;TerminalEmulator;\n");
        desktop(dir, QStringLiteral("hidden"), "Name=Hidden Term\nExec=hiddenterm\nNoDisplay=true\nCategories=TerminalEmulator;\n");
        desktop(dir, QStringLiteral("resources"), "Name=Resources\nExec=resources\nCategories=System;\n");
        desktop(dir, QStringLiteral("mc"), "Name=Mission\nExec=missioncenter\nCategories=System;Monitor;\n");
        desktop(dir, QStringLiteral("btop"), "Name=btop++\nExec=btop\nTerminal=true\nCategories=System;Monitor;ConsoleOnly;\n");
        desktop(dir, QStringLiteral("nvtop"), "Name=nvtop\nExec=nvtop\nTerminal=true\nCategories=System;Monitor;\n");
        desktop(dir, QStringLiteral("editor"), "Name=Editor\nExec=editor\nCategories=Utility;\n");

        AppModel apps;
        apps.reload();
        DefaultApps defaults(&apps);
        QVariantMap terminal, monitor;
        for (const QVariant& c : defaults.categories()) {
            const QVariantMap m = c.toMap();
            if (m.value(QStringLiteral("key")) == QLatin1String("terminal")) terminal = m;
            if (m.value(QStringLiteral("key")) == QLatin1String("task-manager")) monitor = m;
        }
        QCOMPARE(names(terminal), (QStringList { "Automatic", "Ghostty" }));
        const QStringList tm = names(monitor);
        QVERIFY(tm.contains(QStringLiteral("Resources")));
        QVERIFY(tm.contains(QStringLiteral("Mission")));
        QVERIFY(tm.contains(QStringLiteral("btop++")));
        QVERIFY(!tm.contains(QStringLiteral("nvtop")));
        QVERIFY(!tm.contains(QStringLiteral("Editor")));
    }
};

QTEST_GUILESS_MAIN(DefaultAppsTest)
#include "defaultapps_test.moc"
