// SPDX-FileCopyrightText: 2026 Brando Giuffrida
// SPDX-License-Identifier: GPL-3.0-or-later

#include "updates.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLocalSocket>
#include <QProcess>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QStandardPaths>

namespace {

// The updater script: the same one the command line uses.
QString updaterPath()
{
    return QStandardPaths::findExecutable(QStringLiteral("vela-update"));
}

QString terminalPath()
{
    return QStandardPaths::findExecutable(QStringLiteral("vela-terminal"));
}

// Where the updater remembers where the updates come from, when to check
// again and what arrived since the last look.
QString configPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)
        + QStringLiteral("/vela/updates.conf");
}

// The updater's cache: the packages it leaves behind for rollbacks.
QString cacheDirectory()
{
    return QDir::homePath() + QStringLiteral("/.cache/vela-update");
}

// The installed version, like the updater's packageVersion(): the package
// manager knows it.
QString packageVersion()
{
    QProcess pacman;
    pacman.start(QStringLiteral("pacman"), { QStringLiteral("-Q"), QStringLiteral("vela-git") });
    if (pacman.waitForFinished(2000) && pacman.exitCode() == 0) {
        const QString line = QString::fromUtf8(pacman.readAllStandardOutput()).trimmed();
        const int space = line.indexOf(QLatin1Char(' '));
        if (space > 0) {
            return line.mid(space + 1).trimmed();
        }
    }
    QProcess rpm;
    rpm.start(QStringLiteral("rpm"), { QStringLiteral("-q"), QStringLiteral("--qf"), QStringLiteral("%{VERSION}-%{RELEASE}\n"),
        QStringLiteral("vela-git") });
    if (rpm.waitForFinished(2000) && rpm.exitCode() == 0) {
        return QString::fromUtf8(rpm.readAllStandardOutput()).trimmed();
    }
    return QString();
}

// The version in a cached package's name, "vela-git-<version>-<arch>.rpm".
QString packageVersionFromFile(const QString& file)
{
    QString name = file;
    name.chop(QFileInfo(file).suffix().length() + 1);
    if (name.endsWith(QStringLiteral(".pkg.tar"))) {
        name.chop(8);
    }
    // The architecture is the last "-<word>" of an Arch package.
    if (name.endsWith(QStringLiteral(".pkg.tar.zst"))) {
        name = name.left(name.length() - 12);
        const int dash = name.lastIndexOf(QLatin1Char('-'));
        if (dash > 0) {
            name = name.left(dash);
        }
    } else {
        const int dot = name.lastIndexOf(QLatin1Char('.'));
        if (dot > 0) {
            name = name.left(dot); // the RPM's ".x86_64" and ".fc44"
        }
    }
    return name.startsWith(QStringLiteral("vela-git-")) ? name.mid(9) : name;
}

// The last few lines of a log, without the colors of the build tools.
QString logTail(const QString& path, int lines)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    file.seek(qMax<qint64>(0, file.size() - 4000));
    QString text = QString::fromUtf8(file.readAll());
    text.remove(QRegularExpression(QStringLiteral("\\x1b\\[[0-9;?]*[A-Za-z]")));
    QStringList kept;
    for (const QString& line : text.split(u'\n', Qt::SkipEmptyParts)) {
        if (!line.trimmed().isEmpty()) {
            kept << line.trimmed();
        }
    }
    return kept.mid(qMax(0, kept.size() - lines)).join(u'\n');
}

} // namespace

Updates::Updates(QObject* parent) : QObject(parent)
{
    m_status = QStringLiteral("unknown");
    m_installedVersion = packageVersion();
    loadConfig();
    refreshRollbackTargets();
    refreshRecent();
}

Updates::~Updates()
{
    if (m_process) {
        m_process->kill();
        m_process->waitForFinished(1000);
    }
}

void Updates::loadConfig()
{
    m_config.clear();
    QFile file(configPath());
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        while (!file.atEnd()) {
            const QByteArray line = file.readLine().trimmed();
            if (line.isEmpty()) {
                continue;
            }
            const int equals = line.indexOf('=');
            if (equals > 0) {
                m_config.insert(QString::fromUtf8(line.left(equals)), QString::fromUtf8(line.mid(equals + 1)));
            }
        }
    }
    m_source = m_config.value(QStringLiteral("source"));
    m_autoCheck = m_config.value(QStringLiteral("autocheck"), QStringLiteral("daily"));
    if (m_autoCheck != QStringLiteral("daily") && m_autoCheck != QStringLiteral("weekly")
        && m_autoCheck != QStringLiteral("never")) {
        m_autoCheck = QStringLiteral("daily");
    }
    m_pausedUntil = m_config.value(QStringLiteral("paused-until"));
    const QString checked = m_config.value(QStringLiteral("last-check"));
    m_lastCheckBy = QDateTime::fromString(checked, Qt::ISODate);
    m_lastCheckText = m_lastCheckBy.isValid() ? m_lastCheckBy.toString(QStringLiteral("yyyy-MM-dd HH:mm")) : QString();
}

void Updates::saveConfig()
{
    const QString path = configPath();
    if (!QDir().mkpath(QFileInfo(path).absolutePath())) {
        return;
    }
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return;
    }
    // The updater keeps the file for itself alone.
    file.setPermissions(QFile::ReadOwner | QFile::WriteOwner);
    for (auto it = m_config.constBegin(); it != m_config.constEnd(); ++it) {
        if (!it.key().isEmpty()) {
            file.write(QStringLiteral("%1=%2\n").arg(it.key(), it.value()).toUtf8());
        }
    }
    file.close();
}

bool Updates::paused() const
{
    const QDate until = QDate::fromString(m_pausedUntil, QStringLiteral("yyyy-MM-dd"));
    return until.isValid() && QDate::currentDate() <= until;
}

void Updates::setStatus(const QString& status, const QString& message)
{
    m_status = status;
    m_message = message;
    emit changed();
}

void Updates::refreshRollbackTargets()
{
    m_rollbackTargets.clear();
    const QFileInfoList files = QDir(cacheDirectory())
        .entryInfoList({ QStringLiteral("vela-git-*.pkg.tar.zst"), QStringLiteral("vela-git-*.rpm") }, QDir::Files,
            QDir::Time);
    for (const QFileInfo& file : files) {
        const QString version = packageVersionFromFile(file.fileName());
        if (!version.isEmpty() && version != m_installedVersion && !m_rollbackTargets.contains(version)) {
            m_rollbackTargets.append(version);
        }
    }
    emit changed();
}

void Updates::refreshRecent()
{
    // The version changed since the last news were collected: the update the
    // page offered was installed, so show what it contained.
    const QString seen = m_config.value(QStringLiteral("seen-version"));
    if (!seen.isEmpty() && !m_installedVersion.isEmpty() && seen != m_installedVersion) {
        const QString news = m_config.value(QStringLiteral("news"));
        if (!news.isEmpty()) {
            m_changes = news.split(QChar(0x001F));
        }
        setStatus(QStringLiteral("recently-installed"), tr("Vela was updated since you last looked."));
        m_config.insert(QStringLiteral("seen-version"), m_installedVersion);
        saveConfig();
    }
}

void Updates::check()
{
    if (m_process) {
        return; // already checking
    }
    const QString updater = updaterPath();
    if (updater.isEmpty()) {
        setStatus(QStringLiteral("error"), tr("The vela-update script is not installed."));
        return;
    }
    loadConfig(); // the updater may have changed the file since
    refreshRollbackTargets();
    refreshRecent();
    m_newRevision.clear();
    m_changes.clear();
    setStatus(QStringLiteral("checking"));
    m_process = new QProcess(this);
    connect(m_process, &QProcess::finished, this, [this](int code, QProcess::ExitStatus) {
        const QString output = QString::fromUtf8(m_process->readAllStandardOutput());
        const QString errors = QString::fromUtf8(m_process->readAllStandardError());
        m_process->deleteLater();
        m_process = nullptr;
        parseOutput(output);
        if (m_status == QStringLiteral("checking")) {
            // The updater exits with 0 when there is an update, 1 when there
            // is none.
            if (code == 0) {
                setStatus(QStringLiteral("available"));
            } else if (code == 1) {
                setStatus(QStringLiteral("up-to-date"));
            } else {
                const QStringList lines = errors.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
                const QString last = lines.isEmpty() ? QString() : lines.last().trimmed();
                setStatus(QStringLiteral("error"), last.isEmpty() ? tr("The update could not be checked.") : last);
            }
        }
        m_lastCheckBy = QDateTime::currentDateTime();
        m_lastCheckText = m_lastCheckBy.toString(QStringLiteral("yyyy-MM-dd HH:mm"));
        m_config.insert(QStringLiteral("last-check"), m_lastCheckBy.toString(Qt::ISODate));
        saveConfig();
        refreshRollbackTargets();
        refreshRecent();
        emit changed();
    });
    m_process->start(updater, { QStringLiteral("--check") });
}

void Updates::checkIfDue()
{
    if (m_process || paused() || m_autoCheck == QLatin1String("never")) {
        return;
    }
    const qint64 days = m_autoCheck == QLatin1String("weekly") ? 8 : 1;
    if (m_lastCheckBy.isValid() && m_lastCheckBy.msecsTo(QDateTime::currentDateTime()) < days * 20ll * 3600 * 1000) {
        return;
    }
    check();
}

void Updates::parseOutput(const QString& output)
{
    bool collecting = false;
    QStringList changes;
    for (const QString& raw : output.split(QLatin1Char('\n'))) {
        const QString line = raw.trimmed();
        if (collecting) {
            if (raw.startsWith(QStringLiteral("::"))) {
                collecting = false;
            } else if (!line.isEmpty()) {
                changes.append(line);
                continue;
            } else {
                continue;
            }
        }
        if (line.startsWith(QStringLiteral(":: Selected revision: "))) {
            // "default branch (the commit)"
            const int open = line.lastIndexOf(QLatin1Char('('));
            if (open > 0) {
                QString revision = line.mid(open + 1).trimmed();
                if (revision.endsWith(QLatin1Char(')'))) {
                    revision.chop(1);
                }
                m_newRevision = revision;
            }
        } else if (line.startsWith(QStringLiteral(":: Vela is up to date"))) {
            // ":: Vela is up to date (0.1.0.r400.g56d5634-1)."
            const int open = line.indexOf(QLatin1Char('('));
            const int close = line.lastIndexOf(QLatin1Char(')'));
            if (open > 0 && close > open) {
                m_installedVersion = line.mid(open + 1, close - open - 1).trimmed();
            }
            m_newRevision.clear();
            m_changes.clear();
            setStatus(QStringLiteral("up-to-date"));
            return;
        } else if (line.startsWith(QStringLiteral(":: Installed version: "))) {
            // ":: Installed version: 0.1.0.r400.g56d5634-1; new: f6c82a5"
            const QString rest = line.mid(QStringLiteral(":: Installed version: ").length()).trimmed();
            const int separator = rest.indexOf(QStringLiteral("; new:"));
            if (separator > 0) {
                m_installedVersion = rest.left(separator).trimmed();
                m_newRevision = rest.mid(separator + 6).trimmed();
            }
        } else if (line.startsWith(QStringLiteral(":: What's new"))) {
            collecting = true;
        } else if (line.startsWith(QStringLiteral(":: Vela isn't installed as a package"))) {
            setStatus(QStringLiteral("available"), tr("Vela isn't installed as a package: the update will install it."));
        }
    }
    if (!changes.isEmpty()) {
        m_changes = changes;
        if (m_status != QStringLiteral("available")) {
            setStatus(QStringLiteral("available"));
        }
        // Remembered for the page after the update (and the next starts).
        m_config.insert(QStringLiteral("news"), changes.join(QChar(0x001F)));
        m_config.insert(QStringLiteral("seen-version"), m_installedVersion);
        saveConfig();
    }
}

QString Updates::jobLogPath(const QString& name)
{
    return cacheDirectory() + QStringLiteral("/jobs/") + name + QStringLiteral(".log");
}

void Updates::setJob(const QString& name, const QString& state, const QString& message)
{
    m_jobs.insert(name, QVariantMap { { QStringLiteral("state"), state }, { QStringLiteral("message"), message } });
    emit jobsChanged();
}

void Updates::runJob(const QString& name, const QString& program, const QStringList& arguments,
    std::function<void(bool)> done)
{
    if (m_jobProcesses.contains(name)) {
        return; // already running
    }
    QDir().mkpath(cacheDirectory() + QStringLiteral("/jobs"));
    auto* process = new QProcess(this);
    auto* log = new QFile(jobLogPath(name), process);
    if (!log->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        process->deleteLater();
        setJob(name, QStringLiteral("error"), tr("The log could not be written."));
        return;
    }
    // No terminal: the updater asks for the password through pkexec (the polkit
    // window) and nothing waits for a yes (vela-update reads these two).
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    environment.insert(QStringLiteral("VELA_UPDATE_AUTH"), QStringLiteral("pkexec"));
    environment.insert(QStringLiteral("VELA_UPDATE_NOCONFIRM"), QStringLiteral("1"));
    environment.insert(QStringLiteral("GIT_PAGER"), QStringLiteral("cat"));
    environment.insert(QStringLiteral("PAGER"), QStringLiteral("cat"));
    process->setProcessEnvironment(environment);
    process->setProcessChannelMode(QProcess::MergedChannels);
    connect(process, &QProcess::readyRead, this, [process, log] {
        log->write(process->readAll());
        log->flush();
    });
    connect(process, &QProcess::finished, this, [this, name, process, log, done](int code, QProcess::ExitStatus status) {
        log->write(process->readAll());
        log->close();
        m_jobProcesses.remove(name);
        const bool ok = status == QProcess::NormalExit && code == 0;
        // pkexec: 126 is "not authorized or dismissed", 127 "could not authenticate".
        const QString message = ok ? QString()
            : (code == 126 || code == 127) ? tr("The password window was closed or the password was refused.")
                                           : logTail(jobLogPath(name), 3);
        setJob(name, ok ? QStringLiteral("done") : QStringLiteral("error"), message);
        process->deleteLater();
        if (done) {
            done(ok);
        }
    });
    connect(process, &QProcess::errorOccurred, this, [this, name](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            m_jobProcesses.remove(name);
            setJob(name, QStringLiteral("error"), tr("The command could not be started."));
        }
    });
    m_jobProcesses.insert(name, process);
    setJob(name, QStringLiteral("running"));
    process->start(program, arguments);
    process->closeWriteChannel(); // nothing is asked on the keyboard
}

void Updates::showJob(const QString& name)
{
    const QString terminal = terminalPath();
    if (terminal.isEmpty()) {
        setStatus(QStringLiteral("error"), tr("The terminal is not installed."));
        return;
    }
    const QString log = jobLogPath(name);
    const QString command = m_jobProcesses.contains(name)
        ? QStringLiteral("tail -n +1 -f '%1'").arg(log)
        : QStringLiteral("cat '%1'; printf '\\nEnter to close'; read _").arg(log);
    QProcess::startDetached(terminal, { QStringLiteral("-e"), QStringLiteral("sh"), QStringLiteral("-c"), command });
}

void Updates::install()
{
    // The updater, in the background: it builds (or downloads) the package and
    // installs it; the output is in the log.
    setStatus(QStringLiteral("updating"), tr("Updating..."));
    runJob(QStringLiteral("vela"), QStringLiteral("vela-update"), { QStringLiteral("--download") }, [this](bool ok) {
        if (ok) {
            setStatus(QStringLiteral("updating"), tr("Installed. Log out to start the new version."));
        } else {
            setStatus(QStringLiteral("error"), m_jobs.value(QStringLiteral("vela")).toMap().value(QStringLiteral("message")).toString());
        }
    });
}

bool Updates::pacmanAvailable() const
{
    return !QStandardPaths::findExecutable(QStringLiteral("checkupdates")).isEmpty();
}

QString Updates::aurHelper()
{
    for (const QString& helper : { QStringLiteral("paru"), QStringLiteral("yay") }) {
        if (!QStandardPaths::findExecutable(helper).isEmpty()) {
            return helper;
        }
    }
    return {};
}

void Updates::runPackageCheck(const QString& program, const QStringList& arguments, QStringList& into)
{
    into.clear();
    auto* process = new QProcess(this);
    ++m_packagesPending;
    // checkupdates answers 2 for "nothing new", paru/yay 1: only the lines count.
    connect(process, &QProcess::finished, this, [this, process, &into] {
        const QStringList lines = QString::fromUtf8(process->readAllStandardOutput()).split(u'\n', Qt::SkipEmptyParts);
        for (const QString& line : lines) {
            into << line.trimmed();
        }
        process->deleteLater();
        --m_packagesPending;
        emit packagesChanged();
    });
    process->start(program, arguments);
}

void Updates::checkPackages()
{
    if (m_packagesPending > 0) {
        return;
    }
    if (pacmanAvailable()) {
        runPackageCheck(QStringLiteral("checkupdates"), {}, m_pacmanUpdates);
    }
    if (aurAvailable()) {
        runPackageCheck(aurHelper(), { QStringLiteral("-Qua") }, m_aurUpdates);
    }
    emit packagesChanged();
}

void Updates::updatePacman()
{
    runJob(QStringLiteral("pacman"), QStringLiteral("pkexec"),
        { QStringLiteral("pacman"), QStringLiteral("-Syu"), QStringLiteral("--noconfirm") }, [this](bool) { checkPackages(); });
}

void Updates::updateAur()
{
    if (!aurAvailable()) {
        return;
    }
    // The helper builds as the user and asks pkexec to install. shortcut:
    // --noconfirm skips the PKGBUILD review too (these are installed packages);
    // upgrade to a review in the UI if that is not enough.
    runJob(QStringLiteral("aur"), aurHelper(),
        { QStringLiteral("--sudo"), QStringLiteral("pkexec"), QStringLiteral("-Sua"), QStringLiteral("--noconfirm") },
        [this](bool) { checkPackages(); });
}

bool Updates::flatpakInstalled() const
{
    return !QStandardPaths::findExecutable(QStringLiteral("flatpak")).isEmpty();
}

void Updates::checkFlatpak()
{
    if (m_flatpakProcess || !flatpakInstalled()) {
        return;
    }
    m_flatpakProcess = new QProcess(this);
    connect(m_flatpakProcess, &QProcess::finished, this, [this](int code, QProcess::ExitStatus) {
        m_flatpakUpdates.clear();
        if (code == 0) {
            const QStringList lines = QString::fromUtf8(m_flatpakProcess->readAllStandardOutput()).split(u'\n', Qt::SkipEmptyParts);
            for (const QString& line : lines) {
                m_flatpakUpdates << line.trimmed();
            }
        }
        m_flatpakProcess->deleteLater();
        m_flatpakProcess = nullptr;
        emit flatpakChanged();
    });
    m_flatpakProcess->start(QStringLiteral("flatpak"),
        { QStringLiteral("remote-ls"), QStringLiteral("--updates"), QStringLiteral("--columns=name") });
    emit flatpakChanged();
}

void Updates::updateFlatpak()
{
    runJob(QStringLiteral("flatpak"), QStringLiteral("flatpak"),
        { QStringLiteral("update"), QStringLiteral("-y"), QStringLiteral("--noninteractive") }, [this](bool) { checkFlatpak(); });
}

void Updates::rollback(const QString& version)
{
    QString package;
    const QFileInfoList files = QDir(cacheDirectory())
        .entryInfoList({ QStringLiteral("vela-git-*.pkg.tar.zst"), QStringLiteral("vela-git-*.rpm") }, QDir::Files,
            QDir::Time);
    for (const QFileInfo& file : files) {
        if (packageVersionFromFile(file.fileName()) == version) {
            package = file.absoluteFilePath();
            break;
        }
    }
    if (package.isEmpty()) {
        setStatus(QStringLiteral("error"), tr("The package of that version is not in the cache any more."));
        return;
    }
    // Going back, like pacman -U does (a background job: polkit asks for the password).
    setStatus(QStringLiteral("updating"), tr("Going back..."));
    const bool arch = !QStandardPaths::findExecutable(QStringLiteral("pacman")).isEmpty(); // dnf has "downgrade" for it
    runJob(QStringLiteral("rollback"), QStringLiteral("pkexec"),
        arch ? QStringList { QStringLiteral("pacman"), QStringLiteral("-U"), QStringLiteral("--noconfirm"), package }
             : QStringList { QStringLiteral("dnf"), QStringLiteral("downgrade"), QStringLiteral("-y"), package },
        [this](bool ok) {
            setStatus(ok ? QStringLiteral("updating") : QStringLiteral("error"),
                ok ? tr("Went back. Log out to start that version.")
                   : m_jobs.value(QStringLiteral("rollback")).toMap().value(QStringLiteral("message")).toString());
        });
}

void Updates::logOut()
{
    // The compositor ends the session (shell's ShellController::logout does
    // the same).
    const QString socket = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation)
        + QStringLiteral("/vela-") + qEnvironmentVariable("WAYLAND_DISPLAY", QStringLiteral("wayland-0"))
        + QStringLiteral(".sock");
    QLocalSocket connection;
    connection.connectToServer(socket);
    if (!connection.waitForConnected(500)) {
        setStatus(QStringLiteral("error"), tr("The compositor is not answering."));
        return;
    }
    connection.write("logout\n");
    connection.waitForBytesWritten(500);
    connection.disconnectFromServer();
}

void Updates::setSource(const QString& source)
{
    if (source.isEmpty()) {
        return;
    }
    m_config.insert(QStringLiteral("source"), source);
    saveConfig();
    m_source = source;
    emit changed();
}

void Updates::setAutoCheck(const QString& howOften)
{
    if (howOften != QStringLiteral("daily") && howOften != QStringLiteral("weekly") && howOften != QStringLiteral("never")) {
        return;
    }
    m_config.insert(QStringLiteral("autocheck"), howOften);
    saveConfig();
    m_autoCheck = howOften;
    emit changed();
}

void Updates::setPausedUntil(const QString& day)
{
    m_config.insert(QStringLiteral("paused-until"), day);
    saveConfig();
    m_pausedUntil = day;
    emit changed();
}
