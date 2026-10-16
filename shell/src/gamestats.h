// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QByteArray>
#include <QList>
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

} // namespace gamestats
