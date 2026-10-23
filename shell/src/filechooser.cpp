// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "filechooser.h"

#include "globalshortcuts.h"

#include <QDBusArgument>
#include <QDBusConnection>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMimeDatabase>
#include <QProcess>
#include <QTemporaryFile>

namespace {

// a(sa(us)): filters, each a name and its (0, glob) or (1, mime type) rules.
QJsonArray parseFilters(const QVariant& value)
{
    QJsonArray filters;
    if (!value.canConvert<QDBusArgument>()) {
        return filters;
    }
    const QDBusArgument list = value.value<QDBusArgument>();
    if (list.currentType() != QDBusArgument::ArrayType) {
        return filters;
    }
    list.beginArray();
    while (!list.atEnd()) {
        QString name;
        QJsonArray globs;
        list.beginStructure();
        list >> name;
        list.beginArray();
        while (!list.atEnd()) {
            uint kind = 0;
            QString pattern;
            list.beginStructure();
            list >> kind >> pattern;
            list.endStructure();
            if (kind == 0) {
                globs.append(pattern);
            } else {
                for (const QString& glob : QMimeDatabase().mimeTypeForName(pattern).globPatterns()) {
                    globs.append(glob);
                }
            }
        }
        list.endArray();
        list.endStructure();
        filters.append(QJsonObject { { QStringLiteral("name"), name }, { QStringLiteral("globs"), globs } });
    }
    list.endArray();
    return filters;
}

// current_filter, (sa(us)): its name, to find it among the filters.
QString filterName(const QVariant& value)
{
    if (!value.canConvert<QDBusArgument>()) {
        return {};
    }
    const QDBusArgument arg = value.value<QDBusArgument>();
    QString name;
    arg.beginStructure();
    arg >> name;
    while (!arg.atEnd()) {
        arg.beginArray();
        while (!arg.atEnd()) {
            arg.beginStructure();
            uint kind;
            QString pattern;
            arg >> kind >> pattern;
            arg.endStructure();
        }
        arg.endArray();
    }
    arg.endStructure();
    return name;
}

// "ay": a path with its final NUL.
QString pathFrom(const QVariant& value)
{
    QByteArray bytes = value.toByteArray();
    if (bytes.endsWith('\0')) {
        bytes.chop(1);
    }
    return QString::fromLocal8Bit(bytes);
}

} // namespace

FileChooser::FileChooser(GlobalShortcuts* owner)
    : QDBusAbstractAdaptor(owner)
    , m_owner(owner)
{
}

QJsonObject FileChooser::requestFor(const QString& mode, const QString& title, const QVariantMap& options)
{
    QJsonObject request { { QStringLiteral("mode"), mode }, { QStringLiteral("title"), title } };
    request[QStringLiteral("accept")] = options.value(QStringLiteral("accept_label")).toString();
    request[QStringLiteral("multiple")] = options.value(QStringLiteral("multiple")).toBool();
    request[QStringLiteral("directory")] = options.value(QStringLiteral("directory")).toBool();
    QString folder = pathFrom(options.value(QStringLiteral("current_folder")));
    const QString file = pathFrom(options.value(QStringLiteral("current_file")));
    QString name = options.value(QStringLiteral("current_name")).toString();
    if (!file.isEmpty()) {
        // An existing file to replace: its folder and name.
        const int slash = file.lastIndexOf(u'/');
        if (folder.isEmpty() && slash > 0) {
            folder = file.left(slash);
        }
        if (name.isEmpty()) {
            name = file.mid(slash + 1);
        }
    }
    request[QStringLiteral("folder")] = folder;
    request[QStringLiteral("name")] = name;
    QJsonArray names;
    const QVariant files = options.value(QStringLiteral("files"));
    if (files.canConvert<QDBusArgument>()) {
        const QDBusArgument list = files.value<QDBusArgument>();
        list.beginArray();
        while (!list.atEnd()) {
            QByteArray bytes;
            list >> bytes;
            if (bytes.endsWith('\0')) {
                bytes.chop(1);
            }
            names.append(QString::fromLocal8Bit(bytes));
        }
        list.endArray();
    }
    request[QStringLiteral("names")] = names;
    const QJsonArray filters = parseFilters(options.value(QStringLiteral("filters")));
    request[QStringLiteral("filters")] = filters;
    int current = 0;
    const QString wanted = filterName(options.value(QStringLiteral("current_filter")));
    for (int i = 0; i < filters.size() && !wanted.isEmpty(); ++i) {
        if (filters.at(i).toObject().value(QStringLiteral("name")).toString() == wanted) {
            current = i;
        }
    }
    request[QStringLiteral("filter")] = current;
    return request;
}

uint FileChooser::OpenFile(const QDBusObjectPath& handle, const QString&, const QString&, const QString& title,
    const QVariantMap& options, QVariantMap&)
{
    start(QStringLiteral("open"), handle, title, options);
    return 0;
}

uint FileChooser::SaveFile(const QDBusObjectPath& handle, const QString&, const QString&, const QString& title,
    const QVariantMap& options, QVariantMap&)
{
    start(QStringLiteral("save"), handle, title, options);
    return 0;
}

uint FileChooser::SaveFiles(const QDBusObjectPath& handle, const QString&, const QString&, const QString& title,
    const QVariantMap& options, QVariantMap&)
{
    start(QStringLiteral("savefiles"), handle, title, options);
    return 0;
}

void FileChooser::start(const QString& mode, const QDBusObjectPath& handle, const QString& title, const QVariantMap& options)
{
    // The answer comes when the dialog closes.
    auto* request = new FileChooserRequest(handle.path(), m_owner->delayReply(), this);
    request->run(m_program, requestFor(mode, title, options));
}

// ----------------------------------------------------------------- request --

FileChooserRequest::FileChooserRequest(const QString& path, const QDBusMessage& message, QObject* parent)
    : QObject(parent)
    , m_path(path)
    , m_message(message)
{
    QDBusConnection::sessionBus().registerObject(m_path, this, QDBusConnection::ExportAllSlots);
}

FileChooserRequest::~FileChooserRequest()
{
    QDBusConnection::sessionBus().unregisterObject(m_path);
}

void FileChooserRequest::run(const QString& program, const QJsonObject& request)
{
    m_filters = request.value(QStringLiteral("filters")).toArray();
    m_file = new QTemporaryFile(this);
    if (!m_file->open()) {
        reply(2, {});
        return;
    }
    m_file->write(QJsonDocument(request).toJson(QJsonDocument::Compact));
    m_file->flush();
    m_process = new QProcess(this);
    m_process->setProcessChannelMode(QProcess::SeparateChannels);
    connect(m_process, &QProcess::finished, this, [this](int code, QProcess::ExitStatus) { finished(code); });
    connect(m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            reply(2, {});
        }
    });
    m_process->start(program, { QStringLiteral("--pick"), m_file->fileName() });
}

void FileChooserRequest::finished(int exitCode)
{
    // The answer is the last line it printed: { "uris": [...], "filter": n }.
    const QList<QByteArray> lines = m_process->readAllStandardOutput().trimmed().split('\n');
    const QJsonObject answer = QJsonDocument::fromJson(lines.isEmpty() ? QByteArray() : lines.last()).object();
    if (exitCode != 0 || !answer.contains(QStringLiteral("uris"))) {
        reply(1, {});
        return;
    }
    QStringList uris;
    for (const QJsonValue& uri : answer.value(QStringLiteral("uris")).toArray()) {
        uris.append(uri.toString());
    }
    reply(0, { { QStringLiteral("uris"), uris } });
}

void FileChooserRequest::Close()
{
    if (m_process && m_process->state() != QProcess::NotRunning) {
        m_process->terminate();
    }
    reply(1, {});
}

void FileChooserRequest::reply(uint response, const QVariantMap& results)
{
    if (!m_replied) {
        m_replied = true;
        QDBusConnection::sessionBus().send(m_message.createReply(QVariantList { response, results }));
    }
    deleteLater();
}
