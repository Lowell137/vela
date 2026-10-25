// SPDX-FileCopyrightText: 2026 Brando Giuffrida
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QObject>
#include <QProcess>
#include <QTimer>
#include <QVariantList>

#include <optional>

// The system status card of the widgets board: processor load, memory and the
// main disk, and the GPU card beside it (nvidia-smi, or amdgpu's sysfs). It only samples while the board is open (`active`).
class SystemStats : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool active READ active WRITE setActive NOTIFY changed)
    Q_PROPERTY(int cpu READ cpu NOTIFY changed) // percent since the last sample
    Q_PROPERTY(int memory READ memory NOTIFY changed) // percent in use
    Q_PROPERTY(QString memoryText READ memoryText NOTIFY changed) // "6.2 of 31.2 GB"
    Q_PROPERTY(int disk READ disk NOTIFY changed)
    Q_PROPERTY(QString diskText READ diskText NOTIFY changed)
    Q_PROPERTY(bool gpu READ gpu NOTIFY changed) // a GPU could be read
    Q_PROPERTY(QString gpuName READ gpuName NOTIFY changed)
    Q_PROPERTY(int gpuLoad READ gpuLoad NOTIFY changed)
    Q_PROPERTY(int gpuMemory READ gpuMemory NOTIFY changed)
    Q_PROPERTY(QString gpuMemoryText READ gpuMemoryText NOTIFY changed)
    Q_PROPERTY(int gpuTemperature READ gpuTemperature NOTIFY changed) // °C, -1 unknown
    // The apps using the most video memory: [{ name, text: "173 MB" }]
    Q_PROPERTY(QVariantList gpuApps READ gpuApps NOTIFY changed)

public:
    explicit SystemStats(QObject* parent = nullptr);

    bool active() const { return m_timer.isActive(); }
    void setActive(bool on);
    int cpu() const { return m_cpu; }
    int memory() const { return m_memory; }
    QString memoryText() const { return m_memoryText; }
    int disk() const { return m_disk; }
    QString diskText() const { return m_diskText; }
    bool gpu() const { return !m_gpuName.isEmpty(); }
    QString gpuName() const { return m_gpuName; }
    int gpuLoad() const { return m_gpuLoad; }
    int gpuMemory() const { return m_gpuMemory; }
    QString gpuMemoryText() const { return m_gpuMemoryText; }
    int gpuTemperature() const { return m_gpuTemperature; }
    QVariantList gpuApps() const { return m_gpuApps; }

    // The percent of busy time between two readings of /proc/stat's first
    // line ("cpu user nice system idle iowait irq softirq steal ...").
    static int cpuPercent(const QList<qulonglong>& before, const QList<qulonglong>& after);
    // {total, available} in kB from /proc/meminfo's text.
    static QPair<qulonglong, qulonglong> memoryKb(const QByteArray& meminfo);

    // One line of `nvidia-smi --query-gpu=name,utilization.gpu,memory.used,
    // memory.total,temperature.gpu --format=csv,noheader,nounits`: the name,
    // then the four numbers (MiB for memory). Empty when the line is not that.
    struct Gpu {
        QString name;
        int load = 0;
        double memoryUsedMiB = 0, memoryTotalMiB = 0;
        int temperature = -1;
    };
    static std::optional<Gpu> parseNvidiaSmi(const QByteArray& line);
    // `nvidia-smi -q -d PIDS`: MiB per program (the name's last part), summed
    // over its processes, most first.
    static QList<QPair<QString, double>> parseNvidiaApps(const QByteArray& text);

signals:
    void changed();

private:
    void sample();
    void sampleGpu();
    void setGpu(const Gpu& gpu);

    QTimer m_timer;
    QList<qulonglong> m_last;
    int m_cpu = 0, m_memory = 0, m_disk = 0;
    QString m_memoryText, m_diskText;
    QProcess m_smi, m_smiApps;
    QVariantList m_gpuApps;
    bool m_noSmi = false; // nvidia-smi is missing or failed: amdgpu's sysfs instead
    QString m_gpuName, m_gpuMemoryText;
    int m_gpuLoad = 0, m_gpuMemory = 0, m_gpuTemperature = -1;
};
