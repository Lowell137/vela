// SPDX-FileCopyrightText: 2026 Brando Giuffrida
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QFutureWatcher>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QVariantList>
#include <atomic>
#include <memory>

// The Start menu's "Files" section: file names under the home folder that
// contain the query. The scan runs in a worker thread, starts 250 ms after
// the user stops typing, and is bounded in depth, results and time. Hidden
// folders, node_modules and caches are skipped.
class FileSearch : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString query READ query WRITE setQuery NOTIFY queryChanged)
    // [{name, path}], at most maxResults.
    Q_PROPERTY(QVariantList results READ results NOTIFY resultsChanged)

public:
    static constexpr int maxDepth = 6;
    static constexpr int maxResults = 8;
    static constexpr int maxMillis = 1500;

    explicit FileSearch(QObject* parent = nullptr);
    ~FileSearch() override;

    QString query() const { return m_query; }
    void setQuery(const QString& query);
    QVariantList results() const { return m_results; }

    // The scan itself (public for the test): paths whose name contains
    // `query` (case-insensitive), breadth-first so shallow files come first.
    // Stops early when `cancel` is set.
    static QStringList scan(const QString& root, const QString& query, int depth, int limit,
        int millis, const std::atomic_bool& cancel);

signals:
    void queryChanged();
    void resultsChanged();

private:
    void start();
    void setResults(const QStringList& paths);

    QString m_query;
    QVariantList m_results;
    QTimer m_debounce;
    QFutureWatcher<QStringList> m_watcher;
    std::shared_ptr<std::atomic_bool> m_cancel;
};
