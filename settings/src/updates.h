// SPDX-FileCopyrightText: 2026 Brando Giuffrida
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QDate>
#include <QDateTime>
#include <QHash>
#include <QObject>
#include <QProcess>
#include <QString>
#include <QStringList>

// System > Updates, like Windows' Windows Update: it asks the vela-update
// script (the one the command line uses) whether there is a newer commit,
// and what changed. The update itself is installed from a terminal, where
// the package manager asks for the password. The page also holds the
// automatic checks (daily, weekly, never, paused), the packages the updater
// keeps for rollbacks, and the commits that arrived since the last look.
class Updates : public QObject {
    Q_OBJECT
    // "unknown", "checking", "up-to-date", "available", "recently-installed",
    // "updating", "error"
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(bool available READ available NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(QString installedVersion READ installedVersion NOTIFY changed)
    Q_PROPERTY(QString newRevision READ newRevision NOTIFY changed)
    Q_PROPERTY(QString source READ source WRITE setSource NOTIFY changed)
    // The commits the update brings, one per line.
    Q_PROPERTY(QStringList changes READ changes NOTIFY changed)
    Q_PROPERTY(QString lastCheck READ lastCheck NOTIFY changed)
    Q_PROPERTY(QString message READ message NOTIFY changed)
    // How often the page checks by itself: "daily", "weekly" or "never".
    Q_PROPERTY(QString autoCheck READ autoCheck WRITE setAutoCheck NOTIFY changed)
    // The day the automatic checks resume after a pause ("" = not paused).
    Q_PROPERTY(QString pausedUntil READ pausedUntil WRITE setPausedUntil NOTIFY changed)
    Q_PROPERTY(bool paused READ paused NOTIFY changed)
    // The versions the updater keeps in its cache, ready to go back to.
    Q_PROPERTY(QStringList rollbackTargets READ rollbackTargets NOTIFY changed)

public:
    explicit Updates(QObject* parent = nullptr);
    ~Updates() override;

    // Asks the updater whether there is a newer commit.
    Q_INVOKABLE void check();
    // The automatic check: only when the chosen interval has passed and the
    // updates are not paused.
    Q_INVOKABLE void checkIfDue();
    // Installs it: a terminal opens with the update, where the package
    // manager asks for the password.
    Q_INVOKABLE void install();
    // Goes back to one of the packages the updater keeps (a terminal opens).
    Q_INVOKABLE void rollback(const QString& version);
    // Ends the session, so the installed update starts (the compositor does
    // it, like the shell's own "Log out").
    Q_INVOKABLE void logOut();

    // The setters of the source, the automatic check and the pause (the QML
    // assigns to the properties).
    void setSource(const QString& source);
    void setAutoCheck(const QString& howOften);
    void setPausedUntil(const QString& day);

    QString status() const { return m_status; }
    bool available() const { return m_status == QStringLiteral("available"); }
    bool busy() const { return m_process != nullptr; }
    QString installedVersion() const { return m_installedVersion; }
    QString newRevision() const { return m_newRevision; }
    QString source() const { return m_source; }
    QStringList changes() const { return m_changes; }
    QString lastCheck() const { return m_lastCheckText; }
    QString message() const { return m_message; }
    QString autoCheck() const { return m_autoCheck; }
    QString pausedUntil() const { return m_pausedUntil; }
    bool paused() const;
    QStringList rollbackTargets() const { return m_rollbackTargets; }

signals:
    void changed();

private:
    void loadConfig();
    void saveConfig();
    void parseOutput(const QString& output);
    void setStatus(const QString& status, const QString& message = QString());
    void refreshRollbackTargets();
    void refreshRecent();

    QProcess* m_process = nullptr;
    QString m_status;
    QString m_message;
    QString m_installedVersion;
    QString m_newRevision;
    QString m_source;
    QStringList m_changes;
    QStringList m_rollbackTargets;
    QString m_autoCheck;
    QString m_pausedUntil;
    QDateTime m_lastCheckBy; // the automatic check uses it
    QString m_lastCheckText;
    // The keys of the updater's configuration, kept as they are so the rest
    // survives a write.
    QHash<QString, QString> m_config;
};
