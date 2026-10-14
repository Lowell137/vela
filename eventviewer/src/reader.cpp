// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "reader.h"

#include <QClipboard>
#include <QGuiApplication>
#include <QSaveFile>
#include <QUrl>

Reader::Reader(QObject* parent) : QObject(parent)
{
    m_sorted.setSourceModel(&m_model);
    m_sorted.setSortRole(EventModel::TimeRole);
    m_sorted.setSortCaseSensitivity(Qt::CaseInsensitive);
    m_sorted.sort(0, Qt::DescendingOrder);
    connect(&m_model, &EventModel::countChanged, this, &Reader::countChanged);

    m_flushTimer.setSingleShot(true);
    m_flushTimer.setInterval(80);
    connect(&m_flushTimer, &QTimer::timeout, this, &Reader::flush);
    // Typing in the search or the unit box starts a journalctl: after the pause.
    m_reloadTimer.setSingleShot(true);
    m_reloadTimer.setInterval(350);
    connect(&m_reloadTimer, &QTimer::timeout, this, &Reader::start);
    QTimer::singleShot(0, this, &Reader::start);
}

Reader::~Reader()
{
    stop();
}

void Reader::change(QString& field, const QString& value)
{
    if (field == value) {
        return;
    }
    field = value;
    emit filterChanged();
    m_reloadTimer.start();
}

void Reader::setLevel(int level)
{
    if (m_filter.level != level) {
        m_filter.level = qBound(0, level, 7);
        emit filterChanged();
        m_reloadTimer.start();
    }
}

QString Reader::scope() const
{
    return m_filter.scope == journal::Scope::System ? QStringLiteral("system")
        : m_filter.scope == journal::Scope::User ? QStringLiteral("user") : QStringLiteral("all");
}

void Reader::setScope(const QString& scope)
{
    const auto value = scope == QLatin1String("system") ? journal::Scope::System
        : scope == QLatin1String("user") ? journal::Scope::User : journal::Scope::All;
    if (m_filter.scope != value) {
        m_filter.scope = value;
        emit filterChanged();
        m_reloadTimer.start();
    }
}

void Reader::setCap(int cap)
{
    if (cap != m_model.cap()) {
        m_model.setCap(cap);
        emit capChanged();
        reload();
    }
}

void Reader::setFollow(bool follow)
{
    if (m_follow == follow) {
        return;
    }
    m_follow = follow;
    emit followChanged();
    if (follow) {
        begin(m_live, true);
    } else if (m_live) {
        QProcess* process = std::exchange(m_live, nullptr);
        process->disconnect(this);
        process->kill();
        connect(process, &QProcess::finished, process, &QObject::deleteLater);
    }
}

void Reader::reload()
{
    m_reloadTimer.stop();
    start();
}

void Reader::sortBy(const QString& column)
{
    static const QHash<QString, int> roles = {
        { QStringLiteral("time"), EventModel::TimeRole }, { QStringLiteral("level"), EventModel::LevelRole },
        { QStringLiteral("source"), EventModel::SourceRole }, { QStringLiteral("message"), EventModel::MessageRole },
    };
    if (!roles.contains(column)) {
        return;
    }
    // A second click on the same column turns it around; time starts with the newest.
    const bool same = column == m_sortColumn;
    Qt::SortOrder order = same ? (descending() ? Qt::AscendingOrder : Qt::DescendingOrder)
        : (column == QLatin1String("time") ? Qt::DescendingOrder : Qt::AscendingOrder);
    m_sortColumn = column;
    m_sorted.setSortRole(roles.value(column));
    m_sorted.sort(0, order);
    emit sortChanged();
}

void Reader::copy(const QString& text) const
{
    QGuiApplication::clipboard()->setText(text);
}

QVariantMap Reader::entry(int row) const
{
    return m_sorted.index(row, 0).data(EventModel::RowRole).toMap();
}

QString Reader::save(const QUrl& url) const
{
    QSaveFile file(url.toLocalFile());
    if (!file.open(QIODevice::WriteOnly)) {
        return file.errorString();
    }
    for (int row = 0; row < m_sorted.rowCount(); row++) {
        const QModelIndex index = m_sorted.mapToSource(m_sorted.index(row, 0));
        file.write(m_model.entries().at(index.row()).toLine().toUtf8() + '\n');
    }
    return file.commit() ? QString() : file.errorString();
}

void Reader::setState(const QString& state, const QString& error)
{
    m_state = state;
    m_error = error;
    emit stateChanged();
}

void Reader::stop()
{
    m_flushTimer.stop();
    m_pending.clear();
    m_pendingLive.clear();
    for (QProcess** slot : { &m_load, &m_live }) {
        if (QProcess* process = std::exchange(*slot, nullptr)) {
            process->disconnect(this);
            process->kill();
            connect(process, &QProcess::finished, process, &QObject::deleteLater);
            if (QCoreApplication::closingDown()) { // nobody is left to reap it later
                process->waitForFinished(500);
            }
        }
    }
}

void Reader::start()
{
    stop();
    m_model.clear();
    m_capped = false;
    m_partial = false;
    m_loadCapped = false;
    m_loadErrors.clear();
    setState(QStringLiteral("loading"));
    begin(m_load, false);
    if (m_follow) {
        begin(m_live, true);
    }
}

void Reader::begin(QProcess*& slot, bool follow)
{
    auto* process = new QProcess(this);
    slot = process;
    connect(process, &QProcess::readyReadStandardOutput, this, [this, process, follow] { read(process, follow); });
    connect(process, &QProcess::readyReadStandardError, this, [this, process] {
        m_loadErrors += process->readAllStandardError();
        m_loadErrors = m_loadErrors.right(4096);
    });
    connect(process, &QProcess::errorOccurred, this, [this, process](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart && process == m_load) {
            setState(QStringLiteral("failed"), process->errorString());
        }
    });
    connect(process, &QProcess::finished, this, [this, process, follow] { finished(process, follow); });
    process->start(QStringLiteral("journalctl"), journal::arguments(m_filter, follow));
}

void Reader::read(QProcess* process, bool follow)
{
    // A batch is made here and put in the model by flush(): the view moves
    // once per batch, not once per line.
    while (process->canReadLine()) {
        const QByteArray line = process->readLine();
        if (m_loadCapped && !follow) {
            continue; // what journalctl wrote before the kill took
        }
        const std::optional<journal::Entry> entry = journal::parseLine(line);
        if (entry && journal::matches(*entry, m_filter.text)) {
            (follow ? m_pendingLive : m_pending).append(*entry);
        }
    }
    if (!follow && !m_loadCapped && m_model.count() + m_pending.size() > m_model.cap()) {
        // The cap is full of the newest: the rest is older and not wanted.
        m_loadCapped = true;
        m_capped = true;
        process->kill();
    }
    if (!m_flushTimer.isActive()) {
        m_flushTimer.start();
    }
}

void Reader::flush()
{
    // The live ones are newer than anything loaded: on top.
    if (!m_pendingLive.isEmpty()) {
        m_model.prependNewer(std::exchange(m_pendingLive, {}));
    }
    if (!m_pending.isEmpty()) {
        m_capped = m_model.appendOlder(std::exchange(m_pending, {})) || m_capped;
    }
}

void Reader::finished(QProcess* process, bool follow)
{
    if (process != (follow ? m_live : m_load)) {
        return; // a stopped one
    }
    read(process, follow);
    (follow ? m_live : m_load) = nullptr;
    process->deleteLater();
    if (follow) {
        flush();
        return;
    }
    flush();
    const QString errors = QString::fromUtf8(m_loadErrors);
    const bool permission = errors.contains(QLatin1String("insufficient permissions"))
        || errors.contains(QLatin1String("not seeing messages from other users"))
        || errors.contains(QLatin1String("Permission denied"));
    m_partial = permission && m_model.count() > 0;
    if (permission && m_model.count() == 0) {
        setState(QStringLiteral("denied"));
    } else if (!m_loadCapped && process->exitStatus() == QProcess::NormalExit && process->exitCode() != 0 && m_model.count() == 0) {
        setState(QStringLiteral("failed"), errors.trimmed());
    } else {
        setState(QStringLiteral("ready"));
    }
}
