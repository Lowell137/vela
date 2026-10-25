// SPDX-FileCopyrightText: 2026 Brando Giuffrida
// SPDX-License-Identifier: GPL-3.0-or-later

#include "updates.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>

namespace {

// The updater script: the same one the command line uses.
QString updaterPath()
{
    return QStandardPaths::findExecutable(QStringLiteral("vela-update"));
}

// Vela's own terminal if it is installed, else one of the common ones. The
// argument that runs a command: -e for all but GNOME's.
struct Terminal {
    QString path;
    QString runArgument;
};

Terminal findTerminal()
{
    static const QStringList names = {
        QStringLiteral("vela-terminal"), QStringLiteral("kitty"), QStringLiteral("foot"),
        QStringLiteral("alacritty"), QStringLiteral("konsole"), QStringLiteral("gnome-terminal"),
        QStringLiteral("xterm")
    };
    for (const QString& name : names) {
        const QString path = QStandardPaths::findExecutable(name);
        if (!path.isEmpty()) {
            return { path, name == QStringLiteral("gnome-terminal") ? QStringLiteral("--") : QStringLiteral("-e") };
        }
    }
    return {};
}

// Where the updater remembers where the updates come from.
QString configPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)
        + QStringLiteral("/vela/updates.conf");
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

} // namespace

Updates::Updates(QObject* parent) : QObject(parent)
{
    m_status = QStringLiteral("unknown");
    m_installedVersion = packageVersion();
    // The source of the updates, if the updater has one.
    QFile file(configPath());
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        const QString line = QString::fromUtf8(file.readAll()).trimmed();
        if (line.startsWith(QStringLiteral("source="))) {
            m_source = line.mid(7).trimmed();
        }
    }
}

Updates::~Updates()
{
    if (m_process) {
        m_process->kill();
        m_process->waitForFinished(1000);
    }
}

void Updates::setStatus(const QString& status, const QString& message)
{
    m_status = status;
    m_message = message;
    emit changed();
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
    m_changes.clear();
    setStatus(QStringLiteral("checking"));
    m_process = new QProcess(this);
    // finished() is not emitted when the script can't be started at all.
    connect(m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart) {
            return;
        }
        m_process->deleteLater();
        m_process = nullptr;
        setStatus(QStringLiteral("error"), tr("The vela-update script could not be started."));
    });
    connect(m_process, &QProcess::finished, this, [this](int code, QProcess::ExitStatus) {
        const QString output = QString::fromUtf8(m_process->readAllStandardOutput());
        const QString errors = QString::fromUtf8(m_process->readAllStandardError());
        m_process->deleteLater();
        m_process = nullptr;
        parseOutput(output);
        if (m_status == QStringLiteral("checking")) {
            // The updater exits with 0 when there is an update. "Up to date"
            // is reported by its output (parseOutput): any other exit, 1
            // included, is an error.
            if (code == 0) {
                setStatus(QStringLiteral("available"));
            } else {
                const QStringList lines = errors.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
                const QString last = lines.isEmpty() ? QString() : lines.last().trimmed();
                setStatus(QStringLiteral("error"), last.isEmpty() ? tr("The update could not be checked.") : last);
            }
        }
        m_lastChecked = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm"));
        emit changed();
    });
    m_process->start(updater, { QStringLiteral("--check") });
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
        static const QString checkingPrefix = QStringLiteral(":: Checking for updates in ");
        if (line.startsWith(checkingPrefix)) {
            m_source = line.mid(checkingPrefix.size()).trimmed();
        } else if (line.startsWith(QStringLiteral(":: Selected revision: "))) {
            // "default branch (the commit)"
            const int open = line.lastIndexOf(QLatin1Char('('));
            if (open > 0) {
                m_newRevision = line.mid(open + 1).trimmed();
                if (m_newRevision.endsWith(QLatin1Char(')'))) {
                    m_newRevision.chop(1);
                }
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
            const QString rest = line.mid(21).trimmed();
            const int separator = rest.indexOf(QStringLiteral("; new:"));
            if (separator > 0) {
                m_installedVersion = rest.left(separator).trimmed();
                m_newRevision = rest.mid(separator + 6).trimmed();
            }
            setStatus(QStringLiteral("available"));
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
    }
}

void Updates::install()
{
    const Terminal terminal = findTerminal();
    if (terminal.path.isEmpty()) {
        setStatus(QStringLiteral("error"), tr("No terminal is installed."));
        return;
    }
    // The update is installed from a terminal: the package manager asks for
    // the password there, and the user sees the build.
    if (!QProcess::startDetached(terminal.path, { terminal.runArgument, QStringLiteral("vela-update") })) {
        setStatus(QStringLiteral("error"), tr("The terminal could not be started."));
        return;
    }
    setStatus(QStringLiteral("updating"), tr("The update runs in a terminal."));
}

void Updates::setSource(const QString& source)
{
    if (source.isEmpty()) {
        return;
    }
    const QString path = configPath();
    if (!QDir().mkpath(QFileInfo(path).absolutePath())) {
        setStatus(QStringLiteral("error"), tr("The update settings could not be saved."));
        return;
    }
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        setStatus(QStringLiteral("error"), tr("The update settings could not be saved."));
        return;
    }
    // The updater keeps the file for itself alone.
    file.setPermissions(QFile::ReadOwner | QFile::WriteOwner);
    file.write(QStringLiteral("source=%1\n").arg(source).toUtf8());
    file.close();
    m_source = source;
    emit changed();
}
