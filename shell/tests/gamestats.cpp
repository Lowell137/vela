// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// Tests of the Game Bar's parsers (gamestats.h) and the MangoHud config
// editor (mangohudconf.h).
//
//   ctest --test-dir build -R gamestats

#include "gamestats.h"
#include "mangohudconf.h"

#include <QTemporaryDir>
#include <QTest>

using namespace gamestats;

class GameStatsTest : public QObject {
    Q_OBJECT

private slots:
    void procStat()
    {
        const QByteArray a = "cpu  100 0 50 800 50 0 0 0 0 0\ncpu0 1 2 3 4\n";
        const QByteArray b = "cpu  200 0 100 850 50 0 0 0 0 0\n";
        const CpuTimes before = parseProcStat(a);
        QCOMPARE(before.total, quint64(1000));
        QCOMPARE(before.idle, quint64(850));
        // 200 jiffies passed, 50 of them idle: 75 % busy.
        QCOMPARE(cpuPercent(before, parseProcStat(b)), 75.0);
        QCOMPARE(cpuPercent(before, before), -1.0); // no time passed
        QCOMPARE(parseProcStat("intr 1 2 3").total, quint64(0));
    }

    void meminfo()
    {
        const MemInfo info = parseMeminfo("MemTotal:       32768000 kB\nMemFree:  1000 kB\nMemAvailable:   16384000 kB\n");
        QCOMPARE(info.totalKb, qint64(32768000));
        QCOMPARE(info.availableKb, qint64(16384000));
        QCOMPARE(parseMeminfo("garbage").totalKb, qint64(-1));
    }

    void sysfs()
    {
        QCOMPARE(parseSysfs("45250\n", 1000), 45.25); // hwmon millidegrees
        QCOMPARE(parseSysfs("87000000\n", 1e6), 87.0); // power1_average µW
        QCOMPARE(parseSysfs("42"), 42.0); // gpu_busy_percent
        QCOMPARE(parseSysfs(""), -1.0);
        QCOMPARE(parseSysfs("N/A"), -1.0);
    }

    void nvidia()
    {
        const GpuSample s = parseNvidiaCsv("0, 35, 61, 120.50, 2048, 8192\n");
        QCOMPARE(s.index, 0);
        QCOMPARE(s.busy, 35.0);
        QCOMPARE(s.temp, 61.0);
        QCOMPARE(s.power, 120.5);
        QCOMPARE(s.memUsedMb, 2048.0);
        QCOMPARE(s.memTotalMb, 8192.0);
        // A laptop GPU without a power sensor.
        const GpuSample na = parseNvidiaCsv("1, 3, 40, [N/A], 10, 4096");
        QCOMPARE(na.index, 1);
        QCOMPARE(na.power, -1.0);
        QCOMPARE(na.memTotalMb, 4096.0);
        QCOMPARE(parseNvidiaCsv("[Not Supported], [N/A], [N/A], [N/A], [N/A], [N/A]").busy, -1.0);
        QCOMPARE(parseNvidiaCsv("garbage").index, -1);
    }

    void ring()
    {
        RingBuffer ring(3);
        QCOMPARE(ring.values(), QVariantList());
        ring.push(1);
        ring.push(2);
        QCOMPARE(ring.values(), (QVariantList { 1.0, 2.0 }));
        ring.push(3);
        ring.push(4); // 1 falls out
        QCOMPARE(ring.values(), (QVariantList { 2.0, 3.0, 4.0 }));
        QCOMPARE(ring.size(), 3);
        ring.clear();
        QCOMPARE(ring.size(), 0);
        ring.push(5);
        QCOMPARE(ring.values(), (QVariantList { 5.0 }));
    }

    void settings()
    {
        QTemporaryDir dir;
        QSettings settings(dir.filePath(QStringLiteral("vela-shell.conf")), QSettings::IniFormat);
        settings.setValue(QStringLiteral("gamebar/overlay"), false);
        settings.setValue(QStringLiteral("gamebar/opacity"), 0.5);
        settings.setValue(QStringLiteral("gamebar/pinned"), QStringLiteral("fps,cpu"));
        settings.setValue(QStringLiteral("gamebar/crosshair-color"), QStringLiteral("#ff0000"));
        settings.setValue(QStringLiteral("other/key"), 1);
        settings.sync();
        QSettings reread(settings.fileName(), QSettings::IniFormat);
        const QVariantMap map = readSettings(reread);
        QCOMPARE(map.size(), 4);
        QCOMPARE(map.value(QStringLiteral("overlay")).typeId(), QMetaType::Bool);
        QCOMPARE(map.value(QStringLiteral("overlay")).toBool(), false);
        QCOMPARE(map.value(QStringLiteral("opacity")).toDouble(), 0.5);
        QCOMPARE(map.value(QStringLiteral("pinned")).toString(), QStringLiteral("fps,cpu"));
        QCOMPARE(map.value(QStringLiteral("crosshair-color")).toString(), QStringLiteral("#ff0000"));
    }

    void mangohudRoundTrip()
    {
        const QString text = QStringLiteral(
            "# my config\n"
            "legacy_layout=false\n"
            "fps\n"
            "#gpu_temp\n"
            "position=top-left\n"
            "custom_text_center=Hello = World\n");
        MangoHudConf conf = MangoHudConf::parse(text);
        QCOMPARE(conf.text(), text); // untouched: identical
        QVERIFY(conf.flag(QStringLiteral("fps")));
        QVERIFY(!conf.flag(QStringLiteral("gpu_temp"))); // only commented out
        QVERIFY(conf.flag(QStringLiteral("gpu_stats"))); // MangoHud's default
        QCOMPARE(conf.value(QStringLiteral("position")), QStringLiteral("top-left"));
        QVERIFY(conf.value(QStringLiteral("font_size")).isNull());

        conf.setFlag(QStringLiteral("fps"), false);
        conf.setFlag(QStringLiteral("gpu_temp"), true);
        conf.setValue(QStringLiteral("position"), QStringLiteral("bottom-right"));
        conf.setValue(QStringLiteral("font_size"), QStringLiteral("20"));
        QCOMPARE(conf.text(), QStringLiteral(
            "# my config\n"
            "legacy_layout=false\n"
            "fps=0\n"
            "#gpu_temp\n"
            "position=bottom-right\n"
            "custom_text_center=Hello = World\n"
            "gpu_temp\n"
            "font_size=20\n"));
        QVERIFY(!conf.flag(QStringLiteral("fps")));
        QCOMPARE(MangoHudConf::parse(QString()).text(), QString());
    }

    void mangohudPresets()
    {
        MangoHudConf conf = MangoHudConf::parse(QStringLiteral("toggle_hud=F12\n"));
        conf.applyPreset(QStringLiteral("minimal"));
        for (const QString& key : MangoHudConf::flagKeys()) {
            QCOMPARE(conf.flag(key), key == QLatin1String("fps"));
        }
        QCOMPARE(conf.value(QStringLiteral("toggle_hud")), QStringLiteral("F12"));
        conf.applyPreset(QStringLiteral("full"));
        for (const QString& key : MangoHudConf::flagKeys()) {
            QVERIFY(conf.flag(key));
        }
        conf.applyPreset(QStringLiteral("battery"));
        QVERIFY(conf.flag(QStringLiteral("battery")));
        QVERIFY(conf.flag(QStringLiteral("frametime")));
        QVERIFY(!conf.flag(QStringLiteral("gpu_stats")));
    }
};

QTEST_GUILESS_MAIN(GameStatsTest)
#include "gamestats.moc"
