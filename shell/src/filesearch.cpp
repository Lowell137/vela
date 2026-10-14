// SPDX-FileCopyrightText: 2026 Brando Giuffrida
// SPDX-License-Identifier: GPL-3.0-or-later

#include "filesearch.h"

#include <QDeadlineTimer>
#include <QDir>
#include <QFileInfo>
#include <QtConcurrent>
#include <deque>

FileSearch::FileSearch(QObject* parent)
    : QObject(parent)
    , m_cancel(std::make_shared<std::atomic_bool>(false))
{
    m_debounce.setSingleShot(true);
    m_debounce.setInterval(250);
    connect(&m_debounce, &QTimer::timeout, this, &FileSearch::start);
    connect(&m_watcher, &QFutureWatcher<QStringList>::finished, this, [this] {
        setResults(m_watcher.result());
    });
}

FileSearch::~FileSearch()
{
    m_cancel->store(true);
    m_watcher.waitForFinished();
}

void FileSearch::setQuery(const QString& query)
{
    const QString q = query.trimmed();
    if (q == m_query) {
        return;
    }
    m_query = q;
    emit queryChanged();
    // A new query invalidates the scan in flight and the old results.
    m_cancel->store(true);
    m_debounce.stop();
    setResults({});
    // One letter would match half the disk.
    if (m_query.size() >= 2) {
        m_debounce.start();
    }
}

void FileSearch::start()
{
    // The old worker keeps its own flag (it was cancelled); the new one gets a fresh one.
    m_cancel = std::make_shared<std::atomic_bool>(false);
    const auto cancel = m_cancel;
    const QString q = m_query;
    m_watcher.setFuture(QtConcurrent::run([q, cancel] {
        return scan(QDir::homePath(), q, maxDepth, maxResults, maxMillis, *cancel);
    }));
}

void FileSearch::setResults(const QStringList& paths)
{
    // A scan cancelled by a newer query must not show stale results.
    if (!paths.isEmpty() && m_cancel->load()) {
        return;
    }
    QVariantList list;
    for (const QString& p : paths) {
        list.append(QVariantMap { { QStringLiteral("name"), QFileInfo(p).fileName() }, { QStringLiteral("path"), p } });
    }
    if (list == m_results) {
        return;
    }
    m_results = list;
    emit resultsChanged();
}

QStringList FileSearch::scan(const QString& root, const QString& query, int depth, int limit,
    int millis, const std::atomic_bool& cancel)
{
    QStringList found; // files first, then folders (many folders share a name)
    QStringList dirs;
    const QDeadlineTimer deadline(millis);
    // shortcut: no index, a bounded breadth-first walk; upgrade to a cached index if it feels slow.
    std::deque<std::pair<QString, int>> queue { { root, 0 } };
    while (!queue.empty() && found.size() < limit && dirs.size() < limit) {
        const auto [dir, level] = queue.front();
        queue.pop_front();
        if (cancel.load() || deadline.hasExpired()) {
            break;
        }
        const QFileInfoList entries = QDir(dir).entryInfoList(
            QDir::AllEntries | QDir::NoDotAndDotDot | QDir::NoSymLinks, QDir::Name);
        for (const QFileInfo& e : entries) {
            const QString name = e.fileName();
            if (name.startsWith(u'.')) {
                continue;
            }
            if (name.contains(query, Qt::CaseInsensitive)) {
                (e.isDir() ? dirs : found).append(e.filePath());
                if (found.size() >= limit || dirs.size() >= limit) {
                    break;
                }
            }
            if (e.isDir() && level + 1 < depth && name != QLatin1String("node_modules")
                && name != QLatin1String("cache") && name != QLatin1String("Cache")) {
                queue.emplace_back(e.filePath(), level + 1);
            }
        }
    }
    found += dirs;
    return found.mid(0, limit);
}
