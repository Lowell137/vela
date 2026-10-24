// SPDX-FileCopyrightText: 2026 Brando Giuffrida
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QObject>
#include <QTimer>

// The system status card of the widgets board: processor load, memory and the
// main disk. It only samples while the board is open (`active`).
class SystemStats : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool active READ active WRITE setActive NOTIFY changed)
    Q_PROPERTY(int cpu READ cpu NOTIFY changed) // percent since the last sample
    Q_PROPERTY(int memory READ memory NOTIFY changed) // percent in use
    Q_PROPERTY(QString memoryText READ memoryText NOTIFY changed) // "6.2 of 31.2 GB"
    Q_PROPERTY(int disk READ disk NOTIFY changed)
    Q_PROPERTY(QString diskText READ diskText NOTIFY changed)

public:
    explicit SystemStats(QObject* parent = nullptr);

    bool active() const { return m_timer.isActive(); }
    void setActive(bool on);
    int cpu() const { return m_cpu; }
    int memory() const { return m_memory; }
    QString memoryText() const { return m_memoryText; }
    int disk() const { return m_disk; }
    QString diskText() const { return m_diskText; }

    // The percent of busy time between two readings of /proc/stat's first
    // line ("cpu user nice system idle iowait irq softirq steal ...").
    static int cpuPercent(const QList<qulonglong>& before, const QList<qulonglong>& after);
    // {total, available} in kB from /proc/meminfo's text.
    static QPair<qulonglong, qulonglong> memoryKb(const QByteArray& meminfo);

signals:
    void changed();

private:
    void sample();

    QTimer m_timer;
    QList<qulonglong> m_last;
    int m_cpu = 0, m_memory = 0, m_disk = 0;
    QString m_memoryText, m_diskText;
};
