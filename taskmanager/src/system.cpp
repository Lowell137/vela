// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "system.h"

#include "graph.h"
#include "procfs.h"

#include <QCollator>
#include <QCoreApplication>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLocale>
#include <QNetworkInterface>
#include <QProcess>
#include <QStandardPaths>
#include <QUrl>

#include <csignal>
#include <pwd.h>
#include <unistd.h>

namespace {

QByteArray read(const QString& path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

QString userName(uint uid)
{
    static QHash<uint, QString> names;
    auto it = names.constFind(uid);
    if (it == names.constEnd()) {
        const passwd* entry = getpwuid(uid);
        it = names.insert(uid, entry ? QString::fromLocal8Bit(entry->pw_name) : QString::number(uid));
    }
    return *it;
}

// A [Desktop Entry]'s keys; `Name` in the session's language when it has one.
QHash<QString, QString> desktopEntry(const QString& path)
{
    QHash<QString, QString> keys;
    const QString language = QLocale().name().left(2);
    bool inside = false;
    const QList<QByteArray> lines = read(path).split('\n');
    for (const QByteArray& raw : lines) {
        const QString line = QString::fromUtf8(raw).trimmed();
        if (line.startsWith(u'[')) {
            inside = line == QLatin1String("[Desktop Entry]");
            continue;
        }
        const qsizetype equals = line.indexOf(u'=');
        if (!inside || equals <= 0) {
            continue;
        }
        const QString key = line.left(equals).trimmed();
        const QString value = line.mid(equals + 1).trimmed();
        if (key == QLatin1String("Name[") + language + u']') {
            keys.insert(QStringLiteral("Name"), value);
            keys.insert(QStringLiteral("NameIsLocal"), QStringLiteral("1"));
        } else if (key != QLatin1String("Name") || !keys.contains(QStringLiteral("NameIsLocal"))) {
            keys.insert(key, value);
        }
    }
    return keys;
}

// The program a command starts: "env A=b /usr/bin/foo --bar %U" is "foo".
QString program(const QString& command)
{
    const QStringList words = QProcess::splitCommand(command);
    for (const QString& word : words) {
        if (word == QLatin1String("env") || word.contains(u'=')) {
            continue;
        }
        return QFileInfo(word).fileName().toLower();
    }
    return {};
}

} // namespace

System::System(QObject* parent)
    : QObject(parent)
    , m_me(getuid())
{
    loadApps();
    sample();
    connect(&m_timer, &QTimer::timeout, this, &System::sample);
    m_timer.start(1000);
}

void System::setFilter(const QString& filter)
{
    if (m_filter != filter) {
        m_filter = filter;
        emit filterChanged();
        buildRows();
    }
}

void System::sortBy(const QString& column)
{
    m_descending = m_sort == column ? !m_descending : column != QLatin1String("name") && column != QLatin1String("user");
    m_sort = column;
    emit sortChanged();
    buildRows();
}

void System::toggle(const QString& key)
{
    if (!m_open.remove(key)) {
        m_open.insert(key);
    }
    buildRows();
}

void System::record(const QString& name, double value)
{
    QVariantList values = m_history.value(name).toList();
    values.append(value);
    while (values.size() > Graph::samples) {
        values.removeFirst();
    }
    m_history.insert(name, values);
}

void System::loadApps()
{
    const QStringList dirs = QStandardPaths::standardLocations(QStandardPaths::ApplicationsLocation);
    for (auto dir = dirs.crbegin(); dir != dirs.crend(); ++dir) { // the user's own last: they win
        const QFileInfoList files = QDir(*dir).entryInfoList({ QStringLiteral("*.desktop") }, QDir::Files);
        for (const QFileInfo& file : files) {
            const QHash<QString, QString> entry = desktopEntry(file.filePath());
            const QString name = program(entry.value(QStringLiteral("Exec")));
            if (!name.isEmpty() && entry.value(QStringLiteral("NoDisplay")) != QLatin1String("true")) {
                m_apps.insert(name, { entry.value(QStringLiteral("Name")), entry.value(QStringLiteral("Icon")) });
            }
        }
    }
}

// --- every second ---

void System::sample()
{
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const double seconds = m_lastSample ? std::max(0.1, (now - m_lastSample) / 1000.0) : 1;
    m_lastSample = now;

    // Processors.
    const QList<procfs::Cpu> cpus = procfs::parseCpu(read(QStringLiteral("/proc/stat")));
    double totalTicks = 0;
    QVariantList cores;
    for (qsizetype i = 0; i < cpus.size(); i++) {
        double usage = 0;
        if (i < m_cpuTotal.size() && cpus[i].total > m_cpuTotal[i]) {
            const double total = cpus[i].total - m_cpuTotal[i];
            usage = 100.0 * (cpus[i].busy - m_cpuBusy[i]) / total;
            if (i == 0) {
                totalTicks = total;
            }
        }
        if (i == 0) {
            m_cpu.insert(QStringLiteral("usage"), usage);
            record(QStringLiteral("cpu"), usage);
        } else {
            cores.append(usage);
            record(QStringLiteral("core:%1").arg(i - 1), usage);
        }
    }
    m_cpuBusy.clear();
    m_cpuTotal.clear();
    for (const procfs::Cpu& cpu : cpus) {
        m_cpuBusy.append(cpu.busy);
        m_cpuTotal.append(cpu.total);
    }
    m_cpu.insert(QStringLiteral("cores"), cores);
    m_cpu.insert(QStringLiteral("count"), cores.size());
    if (!m_cpu.contains(QStringLiteral("name"))) {
        const QByteArray info = read(QStringLiteral("/proc/cpuinfo"));
        const qsizetype at = info.indexOf("model name");
        m_cpu.insert(QStringLiteral("name"), at < 0 ? QString()
            : QString::fromUtf8(info.mid(info.indexOf(':', at) + 1, info.indexOf('\n', at) - info.indexOf(':', at) - 1)).trimmed());
    }
    double speed = 0;
    int speeds = 0;
    for (qsizetype i = 0; i < cores.size(); i++) {
        const QByteArray khz = read(QStringLiteral("/sys/devices/system/cpu/cpu%1/cpufreq/scaling_cur_freq").arg(i));
        if (!khz.isEmpty()) {
            speed += khz.trimmed().toDouble() / 1000;
            speeds++;
        }
    }
    m_cpu.insert(QStringLiteral("speed"), speeds ? speed / speeds : 0);
    m_cpu.insert(QStringLiteral("uptime"), read(QStringLiteral("/proc/uptime")).split(' ').first().toDouble());

    // Memory.
    const QHash<QByteArray, quint64> mem = procfs::parseMeminfo(read(QStringLiteral("/proc/meminfo")));
    const double total = mem.value("MemTotal");
    const double available = mem.value("MemAvailable");
    m_memory = {
        { QStringLiteral("total"), total },
        { QStringLiteral("available"), available },
        { QStringLiteral("used"), total - available },
        { QStringLiteral("cached"), double(mem.value("Cached") + mem.value("Buffers")) },
        { QStringLiteral("swapTotal"), double(mem.value("SwapTotal")) },
        { QStringLiteral("swapUsed"), double(mem.value("SwapTotal") - mem.value("SwapFree")) },
        { QStringLiteral("usage"), total > 0 ? 100 * (total - available) / total : 0 },
    };
    record(QStringLiteral("memory"), m_memory.value(QStringLiteral("usage")).toDouble());

    // Disks: the whole ones, not their partitions or the loop and RAM devices.
    m_disks.clear();
    const QList<procfs::Disk> disks = procfs::parseDiskstats(read(QStringLiteral("/proc/diskstats")));
    for (const procfs::Disk& disk : disks) {
        const QString sys = QStringLiteral("/sys/block/") + disk.name;
        if (!QFileInfo::exists(sys + QStringLiteral("/device"))) {
            continue;
        }
        const QList<quint64> last = m_diskLast.value(disk.name, { disk.read, disk.written, disk.busy });
        m_diskLast.insert(disk.name, { disk.read, disk.written, disk.busy });
        const double usage = std::min(100.0, (disk.busy - last[2]) / (seconds * 10));
        m_disks.append(QVariantMap {
            { QStringLiteral("name"), disk.name },
            { QStringLiteral("model"), QString::fromUtf8(read(sys + QStringLiteral("/device/model"))).trimmed() },
            { QStringLiteral("size"), read(sys + QStringLiteral("/size")).trimmed().toDouble() * 512 },
            { QStringLiteral("read"), (disk.read - last[0]) / seconds },
            { QStringLiteral("written"), (disk.written - last[1]) / seconds },
            { QStringLiteral("usage"), usage },
        });
        record(QStringLiteral("disk:") + disk.name, usage);
    }

    // Network: the interfaces that are up, without the loopback.
    m_networks.clear();
    const QList<procfs::Net> nets = procfs::parseNetDev(read(QStringLiteral("/proc/net/dev")));
    for (const procfs::Net& net : nets) {
        const QList<quint64> last = m_netLast.value(net.name, { net.received, net.sent });
        m_netLast.insert(net.name, { net.received, net.sent });
        const QNetworkInterface interface = QNetworkInterface::interfaceFromName(net.name);
        if (net.name == QLatin1String("lo") || !(interface.flags() & QNetworkInterface::IsRunning)) {
            continue;
        }
        QString address;
        const QList<QNetworkAddressEntry> entries = interface.addressEntries();
        for (const QNetworkAddressEntry& entry : entries) {
            if (entry.ip().protocol() == QAbstractSocket::IPv4Protocol) {
                address = entry.ip().toString();
            }
        }
        const double received = (net.received - last[0]) / seconds;
        const double sent = (net.sent - last[1]) / seconds;
        m_networks.append(QVariantMap {
            { QStringLiteral("name"), net.name },
            { QStringLiteral("address"), address },
            { QStringLiteral("received"), received },
            { QStringLiteral("sent"), sent },
        });
        record(QStringLiteral("net:") + net.name, received);
        record(QStringLiteral("sent:") + net.name, sent);
    }

    sampleProcesses(totalTicks, seconds);
    sampleGpus();
    buildRows();
    emit sampled();
}

void System::sampleProcesses(double totalTicks, double seconds)
{
    QHash<int, Process> now;
    int threads = 0;
    const QStringList entries = QDir(QStringLiteral("/proc")).entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString& entry : entries) {
        bool numeric = false;
        const int pid = entry.toInt(&numeric);
        if (!numeric) {
            continue;
        }
        const QString dir = QStringLiteral("/proc/") + entry;
        const std::optional<procfs::Stat> stat = procfs::parseStat(read(dir + QStringLiteral("/stat")));
        if (!stat) {
            continue; // gone meanwhile
        }
        Process process;
        const auto old = m_processes.constFind(pid);
        const bool known = old != m_processes.constEnd() && old->start == stat->start;
        if (known) {
            process = *old;
        } else { // what doesn't change while it runs
            process.pid = pid;
            process.start = stat->start;
            process.uid = QFileInfo(dir).ownerId();
            QByteArray command = read(dir + QStringLiteral("/cmdline"));
            command.replace('\0', ' ');
            process.command = QString::fromUtf8(command).trimmed();
            const QString exe = QFileInfo(QFile::symLinkTarget(dir + QStringLiteral("/exe"))).fileName().toLower();
            const QString first = program(process.command);
            process.app = m_apps.contains(exe) ? exe : m_apps.contains(first) ? first : QString();
        }
        // The kernel cuts names at 15 characters: the program's file has the rest.
        const QString file = QFileInfo(QFile::symLinkTarget(dir + QStringLiteral("/exe"))).fileName();
        process.name = known ? process.name : stat->name.size() == 15 && file.startsWith(stat->name) ? file : stat->name;
        process.parent = stat->parent;
        process.state = stat->state;
        process.threads = stat->threads;
        process.memory = read(dir + QStringLiteral("/statm")).split(' ').value(1).toULongLong() * sysconf(_SC_PAGESIZE);
        process.cpu = known && totalTicks > 0 ? 100.0 * (stat->ticks - process.ticks) / totalTicks : 0;
        process.ticks = stat->ticks;
        // Readable for our own processes only: the others' disk use stays at 0.
        quint64 io = 0;
        const QList<QByteArray> lines = read(dir + QStringLiteral("/io")).split('\n');
        for (const QByteArray& line : lines) {
            if (line.startsWith("read_bytes:") || line.startsWith("write_bytes:")) {
                io += line.mid(line.indexOf(':') + 1).trimmed().toULongLong();
            }
        }
        process.disk = known && io >= process.io ? (io - process.io) / seconds : 0;
        process.io = io;
        threads += process.threads;
        now.insert(pid, process);
    }
    m_processes = now;
    m_cpu.insert(QStringLiteral("processes"), m_processes.size());
    m_cpu.insert(QStringLiteral("threads"), threads);
}

bool System::before(const Process& a, const Process& b) const
{
    static const QCollator order = [] {
        QCollator collator;
        collator.setCaseSensitivity(Qt::CaseInsensitive);
        return collator;
    }();
    int way = 0;
    if (m_sort == QLatin1String("cpu")) {
        way = a.cpu < b.cpu ? -1 : a.cpu > b.cpu ? 1 : 0;
    } else if (m_sort == QLatin1String("memory")) {
        way = a.memory < b.memory ? -1 : a.memory > b.memory ? 1 : 0;
    } else if (m_sort == QLatin1String("disk")) {
        way = a.disk < b.disk ? -1 : a.disk > b.disk ? 1 : 0;
    } else if (m_sort == QLatin1String("pid")) {
        way = a.pid - b.pid;
    } else if (m_sort == QLatin1String("threads")) {
        way = a.threads - b.threads;
    } else if (m_sort == QLatin1String("user")) {
        way = order.compare(userName(a.uid), userName(b.uid));
    }
    if (way == 0) { // by name, or the same value
        way = order.compare(a.name, b.name);
        return way != 0 ? way < 0 : a.pid < b.pid;
    }
    return m_descending ? way > 0 : way < 0;
}

void System::buildRows()
{
    auto matches = [this](const QString& text) { return m_filter.isEmpty() || text.contains(m_filter, Qt::CaseInsensitive); };
    auto stateText = [](char state) {
        switch (state) {
        case 'R':
            return QCoreApplication::translate("System", "Running");
        case 'T':
        case 't':
            return QCoreApplication::translate("System", "Stopped");
        case 'Z':
            return QCoreApplication::translate("System", "Not responding");
        default:
            return QCoreApplication::translate("System", "Sleeping");
        }
    };

    // --- Details: every process ---
    QList<Process> all = m_processes.values();
    std::sort(all.begin(), all.end(), [this](const Process& a, const Process& b) { return before(a, b); });
    QList<RowModel::Row> details;
    for (const Process& process : std::as_const(all)) {
        if (!matches(process.name) && !matches(process.command)) {
            continue;
        }
        details.append({ QString::number(process.pid), {
            { QStringLiteral("pid"), process.pid },
            { QStringLiteral("name"), process.name },
            { QStringLiteral("user"), userName(process.uid) },
            { QStringLiteral("state"), stateText(process.state) },
            { QStringLiteral("cpu"), process.cpu },
            { QStringLiteral("memory"), double(process.memory) },
            { QStringLiteral("threads"), process.threads },
            { QStringLiteral("command"), process.command },
            { QStringLiteral("mine"), process.uid == m_me },
        } });
    }
    m_detailRows.set(details);

    // --- Processes: apps with what they started, then the rest ---
    // An app's first process is the one whose parent isn't of the same app.
    QHash<int, int> rootOf; // each of our processes, to the app process above it (0: none)
    auto root = [&](int pid, auto&& self) -> int {
        const auto known = rootOf.constFind(pid);
        if (known != rootOf.constEnd()) {
            return *known;
        }
        const auto it = m_processes.constFind(pid);
        if (it == m_processes.constEnd() || it->uid != m_me) {
            return 0;
        }
        rootOf.insert(pid, 0); // a loop can't happen, but would end here
        const int above = self(it->parent, self);
        const int result = above ? above : it->app.isEmpty() ? 0 : pid;
        rootOf.insert(pid, result);
        return result;
    };
    struct Group {
        Process sum;
        QList<Process> members;
    };
    QHash<int, Group> groups;
    QList<Process> background;
    QList<Process> system;
    for (const Process& process : std::as_const(all)) {
        if (process.pid == 2 || process.parent == 2) {
            continue; // the kernel's own threads: in Details only
        }
        if (process.uid != m_me) {
            system.append(process);
            continue;
        }
        const int top = root(process.pid, root);
        if (!top) {
            background.append(process);
            continue;
        }
        Group& group = groups[top];
        if (group.members.isEmpty()) {
            const Process& first = m_processes[top];
            group.sum = first;
            group.sum.cpu = 0;
            group.sum.memory = 0;
            group.sum.disk = 0;
            group.sum.name = m_apps.value(first.app).name;
        }
        group.sum.cpu += process.cpu;
        group.sum.memory += process.memory;
        group.sum.disk += process.disk;
        group.members.append(process);
    }
    QList<Group> apps = groups.values();
    std::sort(apps.begin(), apps.end(), [this](const Group& a, const Group& b) { return before(a.sum, b.sum); });

    QList<RowModel::Row> rows;
    auto header = [&rows](const QString& key, const QString& title, int count) {
        if (count > 0) {
            rows.append({ key, { { QStringLiteral("kind"), QStringLiteral("header") },
                { QStringLiteral("name"), QStringLiteral("%1 (%2)").arg(title).arg(count) } } });
        }
    };
    auto plain = [&rows, this](const Process& process, const QString& kind) {
        rows.append({ kind + QString::number(process.pid), {
            { QStringLiteral("kind"), kind },
            { QStringLiteral("key"), QString::number(process.pid) },
            { QStringLiteral("name"), process.name },
            { QStringLiteral("pid"), process.pid },
            { QStringLiteral("pids"), QVariantList { process.pid } },
            { QStringLiteral("cpu"), process.cpu },
            { QStringLiteral("memory"), double(process.memory) },
            { QStringLiteral("disk"), process.disk },
            { QStringLiteral("mine"), process.uid == m_me },
        } });
    };
    auto shown = [&matches](const QList<Process>& list) {
        QList<Process> result;
        for (const Process& process : list) {
            if (matches(process.name)) {
                result.append(process);
            }
        }
        return result;
    };

    QList<Group> shownApps;
    for (const Group& group : std::as_const(apps)) {
        if (matches(group.sum.name) || !shown(group.members).isEmpty()) {
            shownApps.append(group);
        }
    }
    header(QStringLiteral("header:apps"), QCoreApplication::translate("System", "Apps"), shownApps.size());
    for (const Group& group : std::as_const(shownApps)) {
        const QString key = QString::number(group.sum.pid);
        QVariantList pids;
        for (const Process& member : group.members) {
            pids.append(member.pid);
        }
        rows.append({ QStringLiteral("app") + key, {
            { QStringLiteral("kind"), QStringLiteral("app") },
            { QStringLiteral("key"), key },
            { QStringLiteral("name"), group.sum.name },
            { QStringLiteral("icon"), m_apps.value(group.sum.app).icon },
            { QStringLiteral("pid"), group.sum.pid },
            { QStringLiteral("pids"), pids },
            { QStringLiteral("count"), group.members.size() },
            { QStringLiteral("cpu"), group.sum.cpu },
            { QStringLiteral("memory"), double(group.sum.memory) },
            { QStringLiteral("disk"), group.sum.disk },
            { QStringLiteral("open"), m_open.contains(key) },
            { QStringLiteral("mine"), true },
        } });
        if (m_open.contains(key)) {
            for (const Process& member : group.members) {
                plain(member, QStringLiteral("child"));
            }
        }
    }
    const QList<Process> shownBackground = shown(background);
    header(QStringLiteral("header:background"), QCoreApplication::translate("System", "Background processes"), shownBackground.size());
    for (const Process& process : shownBackground) {
        plain(process, QStringLiteral("process"));
    }
    const QList<Process> shownSystem = shown(system);
    header(QStringLiteral("header:system"), QCoreApplication::translate("System", "System processes"), shownSystem.size());
    for (const Process& process : shownSystem) {
        plain(process, QStringLiteral("process"));
    }
    m_processRows.set(rows);
}

void System::sampleGpus()
{
    // NVIDIA's own tool, when it's there; asked again once it has answered.
    static const QString tool = QStandardPaths::findExecutable(QStringLiteral("nvidia-smi"));
    if (tool.isEmpty()) {
        // AMD and Intel: what the kernel says.
        m_gpus.clear();
        const QFileInfoList cards = QDir(QStringLiteral("/sys/class/drm")).entryInfoList({ QStringLiteral("card?") });
        for (const QFileInfo& card : cards) {
            const QString device = card.filePath() + QStringLiteral("/device/");
            const QByteArray busy = read(device + QStringLiteral("gpu_busy_percent"));
            if (busy.isEmpty()) {
                continue;
            }
            const double usage = busy.trimmed().toDouble();
            record(QStringLiteral("gpu:%1").arg(m_gpus.size()), usage);
            m_gpus.append(QVariantMap {
                { QStringLiteral("name"), card.fileName() },
                { QStringLiteral("usage"), usage },
                { QStringLiteral("memoryUsed"), read(device + QStringLiteral("mem_info_vram_used")).trimmed().toDouble() },
                { QStringLiteral("memoryTotal"), read(device + QStringLiteral("mem_info_vram_total")).trimmed().toDouble() },
                { QStringLiteral("temperature"), 0 },
            });
        }
        return;
    }
    if (m_gpuBusy) {
        return;
    }
    m_gpuBusy = true;
    auto* process = new QProcess(this);
    connect(process, &QProcess::finished, this, [this, process] {
        QVariantList gpus;
        const QList<QByteArray> lines = process->readAllStandardOutput().split('\n');
        for (const QByteArray& line : lines) {
            const QList<QByteArray> fields = line.split(',');
            if (fields.size() < 5) {
                continue;
            }
            const double usage = fields[1].trimmed().toDouble();
            record(QStringLiteral("gpu:%1").arg(gpus.size()), usage);
            gpus.append(QVariantMap {
                { QStringLiteral("name"), QString::fromUtf8(fields[0]).trimmed() },
                { QStringLiteral("usage"), usage },
                { QStringLiteral("memoryUsed"), fields[2].trimmed().toDouble() * 1024 * 1024 },
                { QStringLiteral("memoryTotal"), fields[3].trimmed().toDouble() * 1024 * 1024 },
                { QStringLiteral("temperature"), fields[4].trimmed().toDouble() },
            });
        }
        m_gpus = gpus;
        m_gpuBusy = false;
        process->deleteLater();
    });
    connect(process, &QProcess::errorOccurred, this, [this, process] {
        m_gpuBusy = false;
        process->deleteLater();
    });
    process->start(tool, { QStringLiteral("--query-gpu=name,utilization.gpu,memory.used,memory.total,temperature.gpu"),
        QStringLiteral("--format=csv,noheader,nounits") });
}

// --- what the buttons do ---

QString System::endTask(const QVariantList& pids)
{
    QList<int> refused;
    QList<int> asked;
    for (const QVariant& value : pids) {
        const int pid = value.toInt();
        if (pid <= 1 || pid == getpid()) {
            continue;
        }
        if (kill(pid, SIGTERM) == 0) {
            asked.append(pid);
        } else if (errno == EPERM) {
            refused.append(pid);
        }
    }
    // Those that ignore the request are ended; a pid reused meanwhile isn't touched.
    QHash<int, quint64> starts;
    for (const int pid : std::as_const(asked)) {
        starts.insert(pid, m_processes.value(pid).start);
    }
    QTimer::singleShot(2000, this, [this, starts] {
        for (auto it = starts.constBegin(); it != starts.constEnd(); ++it) {
            const auto process = m_processes.constFind(it.key());
            if (process != m_processes.constEnd() && process->start == it.value()) {
                kill(it.key(), SIGKILL);
            }
        }
    });
    return refused.isEmpty() ? QString()
        : QCoreApplication::translate("System", "%n process(es) belong to another user and can't be ended from here.", nullptr, refused.size());
}

void System::run(const QString& command) const
{
    QStringList words = QProcess::splitCommand(command);
    if (!words.isEmpty()) {
        const QString program = words.takeFirst();
        QProcess::startDetached(program, words, QDir::homePath());
    }
}

void System::openLocation(int pid) const
{
    const QString exe = QFile::symLinkTarget(QStringLiteral("/proc/%1/exe").arg(pid));
    if (!exe.isEmpty()) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(exe).path()));
    }
}

// --- startup apps: the session's autostart entries ---

void System::refreshStartup()
{
    const QString own = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + QStringLiteral("/autostart");
    QHash<QString, RowModel::Row> found;
    for (const QString& dir : { QStringLiteral("/etc/xdg/autostart"), own }) { // the user's own replaces the system's
        const QFileInfoList files = QDir(dir).entryInfoList({ QStringLiteral("*.desktop") }, QDir::Files);
        for (const QFileInfo& file : files) {
            const QHash<QString, QString> entry = desktopEntry(file.filePath());
            const bool system = dir != own;
            found.insert(file.fileName(), { file.fileName(), {
                { QStringLiteral("key"), file.fileName() },
                { QStringLiteral("name"), entry.value(QStringLiteral("Name"), file.completeBaseName()) },
                { QStringLiteral("icon"), entry.value(QStringLiteral("Icon")) },
                { QStringLiteral("command"), entry.value(QStringLiteral("Exec")) },
                { QStringLiteral("enabled"), entry.value(QStringLiteral("Hidden")) != QLatin1String("true")
                    && entry.value(QStringLiteral("X-GNOME-Autostart-enabled")) != QLatin1String("false") },
                { QStringLiteral("system"), system && !found.contains(file.fileName()) },
            } });
        }
    }
    QList<RowModel::Row> rows = found.values();
    QCollator order;
    order.setCaseSensitivity(Qt::CaseInsensitive);
    std::sort(rows.begin(), rows.end(), [&order](const RowModel::Row& a, const RowModel::Row& b) {
        return order.compare(a.data.value(QStringLiteral("name")).toString(), b.data.value(QStringLiteral("name")).toString()) < 0;
    });
    m_startupRows.set(rows);
}

QString System::setStartup(const QString& key, bool enabled)
{
    // The user's own copy says Hidden=true: the entry stays, to be turned on again.
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + QStringLiteral("/autostart");
    const QString own = dir + u'/' + key;
    const QByteArray text = read(QFileInfo::exists(own) ? own : QStringLiteral("/etc/xdg/autostart/") + key);
    if (text.isEmpty()) {
        return QCoreApplication::translate("System", "%1 can't be read.").arg(key);
    }
    QByteArray result;
    for (const QByteArray& line : text.split('\n')) {
        if (line.startsWith("Hidden=") || line.startsWith("X-GNOME-Autostart-enabled=")) {
            continue;
        }
        result += line + '\n';
        if (line.trimmed() == "[Desktop Entry]") {
            result += enabled ? "Hidden=false\n" : "Hidden=true\n";
        }
    }
    while (result.endsWith("\n\n")) {
        result.chop(1);
    }
    QDir().mkpath(dir);
    QFile file(own);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate) || file.write(result) != result.size()) {
        return file.errorString();
    }
    file.close();
    refreshStartup();
    return {};
}

// --- services: systemd's, the session's and the system's ---

void System::refreshServices()
{
    for (const bool user : { true, false }) {
        auto* process = new QProcess(this);
        connect(process, &QProcess::finished, this, [this, process, user] {
            QList<RowModel::Row> rows;
            const QList<QByteArray> lines = process->readAllStandardOutput().split('\n');
            for (const QByteArray& raw : lines) {
                // unit load active sub description...
                const QStringList fields = QString::fromUtf8(raw).simplified().split(u' ');
                if (fields.size() < 4 || !fields[0].endsWith(QLatin1String(".service"))) {
                    continue;
                }
                const QString key = (user ? QStringLiteral("user:") : QStringLiteral("system:")) + fields[0];
                rows.append({ key, {
                    { QStringLiteral("key"), key },
                    { QStringLiteral("unit"), fields[0] },
                    { QStringLiteral("user"), user },
                    { QStringLiteral("active"), fields[2] == QLatin1String("active") },
                    { QStringLiteral("state"), fields[3] },
                    { QStringLiteral("description"), fields.mid(4).join(u' ') },
                } });
            }
            m_serviceParts.insert(user ? QStringLiteral("user") : QStringLiteral("system"), rows);
            QList<RowModel::Row> all;
            for (const RowModel::Row& row : m_serviceParts.value(QStringLiteral("user")) + m_serviceParts.value(QStringLiteral("system"))) {
                if (m_filter.isEmpty() || row.data.value(QStringLiteral("unit")).toString().contains(m_filter, Qt::CaseInsensitive)
                    || row.data.value(QStringLiteral("description")).toString().contains(m_filter, Qt::CaseInsensitive)) {
                    all.append(row);
                }
            }
            m_serviceRows.set(all);
            process->deleteLater();
        });
        connect(process, &QProcess::errorOccurred, process, &QObject::deleteLater);
        QStringList arguments { QStringLiteral("list-units"), QStringLiteral("--type=service"), QStringLiteral("--all"),
            QStringLiteral("--no-legend"), QStringLiteral("--plain"), QStringLiteral("--no-pager") };
        if (user) {
            arguments.prepend(QStringLiteral("--user"));
        }
        process->start(QStringLiteral("systemctl"), arguments);
    }
}

void System::service(const QString& verb, const QString& unit, bool user)
{
    // A system unit asks for the password itself, through polkit.
    auto* process = new QProcess(this);
    connect(process, &QProcess::finished, this, [this, process] {
        refreshServices();
        process->deleteLater();
    });
    connect(process, &QProcess::errorOccurred, process, &QObject::deleteLater);
    QStringList arguments { verb, unit };
    if (user) {
        arguments.prepend(QStringLiteral("--user"));
    }
    process->start(QStringLiteral("systemctl"), arguments);
}
