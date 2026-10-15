// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

// What the Task Manager shows (`Sys` in QML), read again every second from
// /proc and /sys: the processes (grouped under their apps, and as a flat
// list), the processors, memory, disks, network and graphics cards with their
// last minute, the apps started with the session and systemd's services.

#include "rowmodel.h"

#include <QHash>
#include <QObject>
#include <QSet>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>

class System : public QObject {
    Q_OBJECT
    // {usage, cores: [usage], name, speed (MHz), count, processes, threads, uptime (s)}
    Q_PROPERTY(QVariantMap cpu READ cpu NOTIFY sampled)
    // {total, used, available, cached, swapTotal, swapUsed} in bytes, {usage} in percent
    Q_PROPERTY(QVariantMap memory READ memory NOTIFY sampled)
    // [{name, model, size, read, written (bytes/s), usage}]
    Q_PROPERTY(QVariantList disks READ disks NOTIFY sampled)
    // [{name, address, received, sent (bytes/s)}], the ones that are up
    Q_PROPERTY(QVariantList networks READ networks NOTIFY sampled)
    // [{name, usage, memoryUsed, memoryTotal (bytes), temperature}]
    Q_PROPERTY(QVariantList gpus READ gpus NOTIFY sampled)
    // The last minute of each: "cpu", "memory", "disk:<name>", "net:<name>" (received),
    // "sent:<name>", "gpu:<index>".
    Q_PROPERTY(QVariantMap history READ history NOTIFY sampled)

    // Rows: {kind: "header"|"app"|"process"|"child", key, name, icon, pid, pids, count,
    //        cpu (percent of the machine), memory (bytes), disk (bytes/s), open, mine}
    Q_PROPERTY(RowModel* processes READ processes CONSTANT)
    // Rows: {pid, name, user, state, cpu, memory, threads, command}
    Q_PROPERTY(RowModel* details READ details CONSTANT)
    // Rows: {key, name, icon, command, enabled, system}
    Q_PROPERTY(RowModel* startup READ startup CONSTANT)
    // Rows: {key, unit, user (a unit of the session), active, state, description}
    Q_PROPERTY(RowModel* services READ services CONSTANT)

    Q_PROPERTY(QString filter READ filter WRITE setFilter NOTIFY filterChanged) // of the name, in every list
    Q_PROPERTY(QString sortColumn READ sortColumn NOTIFY sortChanged) // name, cpu, memory, disk, pid, user, threads
    Q_PROPERTY(bool descending READ descending NOTIFY sortChanged)

public:
    explicit System(QObject* parent = nullptr);

    QVariantMap cpu() const { return m_cpu; }
    QVariantMap memory() const { return m_memory; }
    QVariantList disks() const { return m_disks; }
    QVariantList networks() const { return m_networks; }
    QVariantList gpus() const { return m_gpus; }
    QVariantMap history() const { return m_history; }
    RowModel* processes() { return &m_processRows; }
    RowModel* details() { return &m_detailRows; }
    RowModel* startup() { return &m_startupRows; }
    RowModel* services() { return &m_serviceRows; }
    QString filter() const { return m_filter; }
    void setFilter(const QString& filter);
    QString sortColumn() const { return m_sort; }
    bool descending() const { return m_descending; }

    // The same column again turns the order around.
    Q_INVOKABLE void sortBy(const QString& column);
    Q_INVOKABLE void toggle(const QString& key); // an app's processes, shown or not
    // Asks the processes to end, and ends the ones still there two seconds
    // later. The error for those that aren't ours to end, empty if none.
    Q_INVOKABLE QString endTask(const QVariantList& pids);
    Q_INVOKABLE void run(const QString& command) const;
    Q_INVOKABLE void openLocation(int pid) const; // the program's folder

    Q_INVOKABLE void refreshStartup();
    Q_INVOKABLE QString setStartup(const QString& key, bool enabled);
    Q_INVOKABLE void refreshServices();
    Q_INVOKABLE void service(const QString& verb, const QString& unit, bool user); // start, stop, restart

signals:
    void sampled();
    void filterChanged();
    void sortChanged();

private:
    struct Process {
        int pid = 0;
        int parent = 0;
        uint uid = 0;
        char state = '?';
        QString name;
        QString command;
        QString app; // the .desktop entry it's the program of, if any
        quint64 ticks = 0;
        quint64 start = 0;
        quint64 memory = 0;
        quint64 io = 0;
        int threads = 0;
        double cpu = 0;
        double disk = 0;
    };
    struct App {
        QString name;
        QString icon;
    };

    void sample();
    void sampleProcesses(double totalTicks, double seconds);
    void buildRows();
    void sampleGpus();
    void record(const QString& name, double value);
    void loadApps();
    bool before(const Process& a, const Process& b) const;

    QTimer m_timer;
    qint64 m_lastSample = 0;
    QList<quint64> m_cpuBusy;
    QList<quint64> m_cpuTotal;
    QHash<QString, QList<quint64>> m_diskLast;
    QHash<QString, QList<quint64>> m_netLast;
    QHash<int, Process> m_processes;
    QHash<QString, App> m_apps; // by program name, lower case
    QSet<QString> m_open;
    uint m_me = 0;

    QVariantMap m_cpu;
    QVariantMap m_memory;
    QVariantList m_disks;
    QVariantList m_networks;
    QVariantList m_gpus;
    QVariantMap m_history;
    RowModel m_processRows;
    RowModel m_detailRows;
    RowModel m_startupRows;
    RowModel m_serviceRows;
    QString m_filter;
    QString m_sort = QStringLiteral("cpu");
    bool m_descending = true;
    bool m_gpuBusy = false;
    QHash<QString, QList<RowModel::Row>> m_serviceParts; // "user" and "system", as they arrive
};
