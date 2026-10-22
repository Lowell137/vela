// SPDX-FileCopyrightText: 2026 Brando Giuffrida
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QObject>
#include <QProcess>
#include <QString>
#include <QStringList>

// System > Updates, like Windows' Windows Update: it asks the vela-update
// script (the one the command line uses) whether there is a newer commit,
// and what changed. The update itself is installed from a terminal, where
// the package manager asks for the password.
class Updates : public QObject {
    Q_OBJECT
    // "unknown", "checking", "up-to-date", "available", "updating", "error"
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(bool available READ available NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(QString installedVersion READ installedVersion NOTIFY changed)
    Q_PROPERTY(QString newRevision READ newRevision NOTIFY changed)
    Q_PROPERTY(QString source READ source NOTIFY changed)
    // The commits the update brings, one per line.
    Q_PROPERTY(QStringList changes READ changes NOTIFY changed)
    Q_PROPERTY(QString lastChecked READ lastChecked NOTIFY changed)
    Q_PROPERTY(QString message READ message NOTIFY changed)

public:
    explicit Updates(QObject* parent = nullptr);
    ~Updates() override;

    // Asks the updater whether there is a newer commit.
    Q_INVOKABLE void check();
    // Installs it: a terminal opens with the update, where the package
    // manager asks for the password.
    Q_INVOKABLE void install();
    // Where the updates come from: a repository or a folder. The updater
    // remembers it too (updates.conf).
    Q_INVOKABLE void setSource(const QString& source);

    QString status() const { return m_status; }
    bool available() const { return m_status == QStringLiteral("available"); }
    bool busy() const { return m_process != nullptr; }
    QString installedVersion() const { return m_installedVersion; }
    QString newRevision() const { return m_newRevision; }
    QString source() const { return m_source; }
    QStringList changes() const { return m_changes; }
    QString lastChecked() const { return m_lastChecked; }
    QString message() const { return m_message; }

signals:
    void changed();

private:
    void parseOutput(const QString& output);
    void setStatus(const QString& status, const QString& message = QString());

    QProcess* m_process = nullptr;
    QString m_status;
    QString m_message;
    QString m_installedVersion;
    QString m_newRevision;
    QString m_source;
    QStringList m_changes;
    QString m_lastChecked;
};
