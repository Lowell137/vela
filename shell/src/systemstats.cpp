// SPDX-FileCopyrightText: 2026 Brando Giuffrida
// SPDX-License-Identifier: GPL-3.0-or-later

#include "systemstats.h"

#include <QFile>
#include <QLocale>
#include <QStorageInfo>

#include <algorithm>

namespace {

QList<qulonglong> readCpu()
{
    QFile file(QStringLiteral("/proc/stat"));
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    const QList<QByteArray> parts = file.readLine().simplified().split(' ');
    QList<qulonglong> values;
    for (int i = 1; i < parts.size(); ++i) {
        values.append(parts.at(i).toULongLong());
    }
    return values;
}

QString gigabytes(double bytes)
{
    return QLocale().toString(bytes / (1024.0 * 1024.0 * 1024.0), 'f', 1);
}

} // namespace

SystemStats::SystemStats(QObject* parent)
    : QObject(parent)
{
    m_timer.setInterval(1500);
    connect(&m_timer, &QTimer::timeout, this, &SystemStats::sample);
}

void SystemStats::setActive(bool on)
{
    if (on == active()) {
        return;
    }
    if (on) {
        m_last = readCpu();
        m_timer.start();
        sample();
    } else {
        m_timer.stop();
    }
    emit changed();
}

int SystemStats::cpuPercent(const QList<qulonglong>& before, const QList<qulonglong>& after)
{
    if (before.size() < 5 || after.size() < before.size()) {
        return 0;
    }
    qulonglong total = 0;
    for (int i = 0; i < before.size(); ++i) {
        total += after.at(i) - std::min(after.at(i), before.at(i));
    }
    // idle and iowait are the 4th and 5th numbers.
    const qulonglong idle = (after.at(3) - std::min(after.at(3), before.at(3))) + (after.at(4) - std::min(after.at(4), before.at(4)));
    return total == 0 ? 0 : int(std::clamp<qulonglong>((total - std::min(total, idle)) * 100 / total, 0, 100));
}

QPair<qulonglong, qulonglong> SystemStats::memoryKb(const QByteArray& meminfo)
{
    qulonglong total = 0, available = 0;
    for (const QByteArray& line : meminfo.split('\n')) {
        if (line.startsWith("MemTotal:")) {
            total = line.mid(9).simplified().split(' ').value(0).toULongLong();
        } else if (line.startsWith("MemAvailable:")) {
            available = line.mid(13).simplified().split(' ').value(0).toULongLong();
        }
    }
    return { total, available };
}

void SystemStats::sample()
{
    const QList<qulonglong> now = readCpu();
    m_cpu = cpuPercent(m_last, now);
    m_last = now;

    QFile meminfo(QStringLiteral("/proc/meminfo"));
    if (meminfo.open(QIODevice::ReadOnly)) {
        const auto [total, available] = memoryKb(meminfo.readAll());
        if (total > 0) {
            const qulonglong used = total - std::min(total, available);
            m_memory = int(used * 100 / total);
            m_memoryText = tr("%1 of %2 GB").arg(gigabytes(used * 1024.0), gigabytes(total * 1024.0));
        }
    }

    const QStorageInfo root(QStringLiteral("/"));
    if (root.isValid() && root.bytesTotal() > 0) {
        const qint64 used = root.bytesTotal() - root.bytesAvailable();
        m_disk = int(used * 100 / root.bytesTotal());
        m_diskText = tr("%1 of %2 GB").arg(gigabytes(double(used)), gigabytes(double(root.bytesTotal())));
    }
    emit changed();
}
