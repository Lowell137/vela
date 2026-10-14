// SPDX-FileCopyrightText: 2026 Brando Giuffrida
// SPDX-License-Identifier: GPL-3.0-or-later

// The taskbar keys of vela-shell.conf: auto-hide (off) and combine ("always"),
// and the values that aren't recognized.

#include "config.h"

#include <QSettings>
#include <QTemporaryDir>
#include <QTest>

class ConfigTest : public QObject {
    Q_OBJECT
    QTemporaryDir m_dir;

private Q_SLOTS:
    void initTestCase()
    {
        QCoreApplication::setOrganizationName(QStringLiteral("Vela"));
        QCoreApplication::setApplicationName(QStringLiteral("vela-shell-test"));
        QSettings::setPath(QSettings::NativeFormat, QSettings::UserScope, m_dir.path());
    }
    void defaults()
    {
        QSettings().clear();
        Config config;
        QVERIFY(!config.taskbarAutoHide());
        QCOMPARE(config.taskbarCombine(), QStringLiteral("always"));
    }
    void values()
    {
        {
            QSettings settings;
            settings.setValue(QStringLiteral("taskbar/autoHide"), true);
            settings.setValue(QStringLiteral("taskbar/combine"), QStringLiteral("never"));
        }
        Config config;
        QVERIFY(config.taskbarAutoHide());
        QCOMPARE(config.taskbarCombine(), QStringLiteral("never"));
    }
    void unknownCombineIsAlways()
    {
        QSettings().setValue(QStringLiteral("taskbar/combine"), QStringLiteral("when-full"));
        Config config;
        QCOMPARE(config.taskbarCombine(), QStringLiteral("always"));
    }
};

QTEST_GUILESS_MAIN(ConfigTest)
#include "config.moc"
