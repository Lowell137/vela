// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

// The backend of xdg-desktop-portal's FileChooser
// (org.freedesktop.impl.portal.FileChooser): an app asks for a file to open or
// a place to save, and Vela's own File Explorer answers, started as
// `vela-files --pick request.json` (explorer/src/picker.h). It lives on the
// object of the GlobalShortcuts backend, as another interface of the same
// portal name (session/vela.portal).

#include <QDBusAbstractAdaptor>
#include <QDBusContext>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QVariantMap>

class QProcess;
class QTemporaryFile;

class GlobalShortcuts;

class FileChooser : public QDBusAbstractAdaptor {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.impl.portal.FileChooser")
    Q_PROPERTY(uint version READ version CONSTANT)

public:
    explicit FileChooser(GlobalShortcuts* owner);
    uint version() const { return 3; }
    // The program to start: the Explorer, next to the shell or installed
    // (velaFilesExecutable()); a script in the tests.
    void setProgram(const QString& program) { m_program = program; }

    // The request an app's options become: the JSON of picker.h. Public for the tests.
    static QJsonObject requestFor(const QString& mode, const QString& title, const QVariantMap& options);

public Q_SLOTS:
    uint OpenFile(const QDBusObjectPath& handle, const QString& appId, const QString& parentWindow, const QString& title,
        const QVariantMap& options, QVariantMap& results);
    uint SaveFile(const QDBusObjectPath& handle, const QString& appId, const QString& parentWindow, const QString& title,
        const QVariantMap& options, QVariantMap& results);
    uint SaveFiles(const QDBusObjectPath& handle, const QString& appId, const QString& parentWindow, const QString& title,
        const QVariantMap& options, QVariantMap& results);

private:
    friend class FileChooserRequest;
    void start(const QString& mode, const QDBusObjectPath& handle, const QString& title, const QVariantMap& options);
    GlobalShortcuts* m_owner;
    QString m_program;
};

// One open dialog (org.freedesktop.impl.portal.Request), at the path the
// portal chose: Close() dismisses it.
class FileChooserRequest : public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.impl.portal.Request")

public:
    FileChooserRequest(const QString& path, const QDBusMessage& message, QObject* parent);
    ~FileChooserRequest() override;
    void run(const QString& program, const QJsonObject& request);

public Q_SLOTS:
    void Close();

private:
    void finished(int exitCode);
    void reply(uint response, const QVariantMap& results);

    QString m_path;
    QDBusMessage m_message;
    QProcess* m_process = nullptr;
    QTemporaryFile* m_file = nullptr;
    QJsonArray m_filters;
    bool m_replied = false;
};
