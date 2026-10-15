// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QByteArray>
#include <QList>
#include <QSettings>
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <QVariantList>

#include <algorithm>

// The Game Bar's numbers (docs/shortcuts.md, gamebar.cpp): what /proc, sysfs
// and nvidia-smi print, read by small pure functions. Unknown is -1.
namespace gamestats {

// The first "cpu" line of /proc/stat: all the jiffies, and the idle ones
// (idle + iowait).
struct CpuTimes {
    quint64 total = 0;
    quint64 idle = 0;
};

inline CpuTimes parseProcStat(const QByteArray& text)
{
    CpuTimes times;
    const qsizetype end = text.indexOf('\n');
    const QByteArray line = text.left(end < 0 ? text.size() : end);
    if (!line.startsWith("cpu ")) {
        return times;
    }
    const QList<QByteArray> fields = line.mid(4).simplified().split(' ');
    // user nice system idle iowait irq softirq steal (guest is already in user).
    for (qsizetype i = 0; i < fields.size() && i < 8; ++i) {
        const quint64 value = fields.at(i).toULongLong();
        times.total += value;
        if (i == 3 || i == 4) {
            times.idle += value;
        }
    }
    return times;
}

// Busy share between two readings, 0..100; -1 without a step forward.
inline double cpuPercent(const CpuTimes& before, const CpuTimes& after)
{
    if (after.total <= before.total || after.idle < before.idle) {
        return -1;
    }
    const double total = double(after.total - before.total);
    const double idle = double(after.idle - before.idle);
    return std::clamp(100.0 * (total - idle) / total, 0.0, 100.0);
}

// /proc/meminfo: MemTotal and MemAvailable, in kB.
struct MemInfo {
    qint64 totalKb = -1;
    qint64 availableKb = -1;
};

inline MemInfo parseMeminfo(const QByteArray& text)
{
    MemInfo info;
    for (const QByteArray& line : text.split('\n')) {
        const qsizetype colon = line.indexOf(':');
        if (colon < 0) {
            continue;
        }
        const QByteArray key = line.left(colon);
        bool ok = false;
        const qint64 value = line.mid(colon + 1).simplified().split(' ').value(0).toLongLong(&ok);
        if (!ok) {
            continue;
        }
        if (key == "MemTotal") {
            info.totalKb = value;
        } else if (key == "MemAvailable") {
            info.availableKb = value;
        }
    }
    return info;
}

// A sysfs number (hwmon temp*_input in millidegrees, power*_average in
// microwatts, mem_info_vram_* in bytes, gpu_busy_percent) divided by `scale`.
inline double parseSysfs(const QByteArray& text, double scale = 1)
{
    bool ok = false;
    const qint64 value = text.trimmed().toLongLong(&ok);
    return ok && value >= 0 ? value / scale : -1;
}

// One line of `nvidia-smi --query-gpu=index,utilization.gpu,temperature.gpu,
// power.draw,memory.used,memory.total --format=csv,noheader,nounits`. Fields
// a card doesn't report are "[N/A]" (or "[Not Supported]").
struct GpuSample {
    int index = -1;
    double busy = -1; // %
    double temp = -1; // °C
    double power = -1; // W
    double memUsedMb = -1;
    double memTotalMb = -1;
};

inline GpuSample parseNvidiaCsv(const QByteArray& line)
{
    GpuSample sample;
    const QList<QByteArray> fields = line.trimmed().split(',');
    if (fields.size() < 6) {
        return sample;
    }
    auto number = [&fields](int i) {
        bool ok = false;
        const double value = fields.at(i).trimmed().toDouble(&ok);
        return ok && value >= 0 ? value : -1;
    };
    sample.index = int(number(0));
    sample.busy = number(1);
    sample.temp = number(2);
    sample.power = number(3);
    sample.memUsedMb = number(4);
    sample.memTotalMb = number(5);
    return sample;
}

// The last `capacity` values of a number for the sparklines, oldest first.
class RingBuffer {
public:
    explicit RingBuffer(int capacity)
        : m_values(capacity, 0.0)
    {
    }
    void push(double value)
    {
        m_values[m_next] = value;
        m_next = (m_next + 1) % m_values.size();
        m_count = std::min(m_count + 1, int(m_values.size()));
    }
    void clear()
    {
        m_next = 0;
        m_count = 0;
    }
    int size() const { return m_count; }
    QVariantList values() const
    {
        QVariantList out;
        out.reserve(m_count);
        const int start = (m_next - m_count + int(m_values.size())) % int(m_values.size());
        for (int i = 0; i < m_count; ++i) {
            out.append(m_values.at((start + i) % m_values.size()));
        }
        return out;
    }

private:
    QList<double> m_values;
    int m_next = 0;
    int m_count = 0;
};

// The [gamebar] keys of vela-shell.conf for QML (the shell's Config and
// Settings): an ini file gives strings, so "true"/"false" become booleans
// and numbers numbers; the rest stays text.
inline QVariantMap readSettings(QSettings& settings)
{
    QVariantMap map;
    settings.beginGroup(QStringLiteral("gamebar"));
    for (const QString& key : settings.childKeys()) {
        QVariant value = settings.value(key);
        // "a,b" unquoted in the file is a list to QSettings: back to a comma list.
        if (value.typeId() == QMetaType::QStringList) {
            value = value.toStringList().join(u',');
        }
        const QString text = value.toString();
        bool number = false;
        const double n = text.toDouble(&number);
        if (text == QLatin1String("true") || text == QLatin1String("false")) {
            value = text == QLatin1String("true");
        } else if (number) {
            value = n;
        }
        map.insert(key, value);
    }
    settings.endGroup();
    return map;
}

// --- Game Bar widgets: where they are, which are pinned over the game ---

// The widgets, in the order the bar lays them out by default.
inline QStringList widgetIds()
{
    return { QStringLiteral("capture"), QStringLiteral("audio"), QStringLiteral("performance"),
        QStringLiteral("media"), QStringLiteral("crosshair"), QStringLiteral("gamemode") };
}

// Opened by default (gamebar/widgets not written yet).
inline QStringList defaultOpenWidgets()
{
    return { QStringLiteral("capture"), QStringLiteral("audio"), QStringLiteral("performance") };
}

// gamebar/pos-<id>: "screen|x|y", the card's top left in the output's pixels.
struct WidgetPos {
    QString screen;
    int x = 0;
    int y = 0;
    bool valid = false;
};

inline WidgetPos parseWidgetPos(const QString& text)
{
    const QStringList parts = text.split(u'|');
    WidgetPos pos;
    if (parts.size() != 3) {
        return pos;
    }
    bool okX = false;
    bool okY = false;
    pos.x = parts.at(1).toInt(&okX);
    pos.y = parts.at(2).toInt(&okY);
    pos.screen = parts.at(0);
    pos.valid = okX && okY;
    return pos;
}

inline QString formatWidgetPos(const WidgetPos& pos)
{
    return pos.screen + u'|' + QString::number(pos.x) + u'|' + QString::number(pos.y);
}

inline QStringList widgetList(const QVariantMap& bar, const QString& key, const QStringList& fallback)
{
    const QVariant value = bar.value(key);
    if (!value.isValid()) {
        return fallback;
    }
    const QStringList ids = widgetIds();
    QStringList out;
    for (const QString& id : value.toString().split(u',', Qt::SkipEmptyParts)) {
        if (ids.contains(id) && !out.contains(id)) {
            out.append(id);
        }
    }
    return out;
}

// The outputs that need an overlay window: where a pinned widget (the
// crosshair has its own window) stays over the game. A widget whose output
// is gone goes to the main one. Empty when "Show overlay" is off.
inline QStringList overlayScreens(const QVariantMap& bar, const QStringList& screens, const QString& primary)
{
    QStringList out;
    if (!bar.value(QStringLiteral("overlay"), true).toBool()) {
        return out;
    }
    const QStringList open = widgetList(bar, QStringLiteral("widgets"), defaultOpenWidgets());
    for (const QString& id : widgetList(bar, QStringLiteral("pinned-widgets"), {})) {
        if (id == QLatin1String("crosshair") || !open.contains(id)) {
            continue;
        }
        QString screen = parseWidgetPos(bar.value(QStringLiteral("pos-") + id).toString()).screen;
        if (!screens.contains(screen)) {
            screen = primary;
        }
        if (!out.contains(screen)) {
            out.append(screen);
        }
    }
    return out;
}

// Is the focused app a game: a Steam game, a "Game" in its .desktop file or
// already known to gamemoded.
inline bool looksLikeGame(const QString& appId, const QStringList& categories, bool registered)
{
    return !appId.isEmpty()
        && (registered || appId.startsWith(QLatin1String("steam_app_")) || categories.contains(QLatin1String("Game")));
}

} // namespace gamestats
