// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

// The journal for the window: runs journalctl for the filter chosen, streams
// what it prints into the model in batches and, with follow, keeps a second
// journalctl running for the new entries. Nothing here waits for it.

#include "eventmodel.h"
#include "journal.h"

#include <QObject>
#include <QProcess>
#include <QSortFilterProxyModel>
#include <QTimer>

class Reader : public QObject {
    Q_OBJECT
    Q_PROPERTY(QAbstractItemModel* model READ model CONSTANT)
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(int cap READ cap WRITE setCap NOTIFY capChanged)
    Q_PROPERTY(QString state READ state NOTIFY stateChanged) // loading, ready, denied, failed
    Q_PROPERTY(bool capped READ capped NOTIFY stateChanged) // there was more than the cap
    Q_PROPERTY(bool partial READ partial NOTIFY stateChanged) // journalctl said it sees only part
    Q_PROPERTY(QString error READ error NOTIFY stateChanged)
    Q_PROPERTY(bool follow READ follow WRITE setFollow NOTIFY followChanged)
    Q_PROPERTY(QString view READ view WRITE setView NOTIFY filterChanged)
    Q_PROPERTY(int level READ level WRITE setLevel NOTIFY filterChanged)
    Q_PROPERTY(QString range READ range WRITE setRange NOTIFY filterChanged)
    Q_PROPERTY(QString unit READ unit WRITE setUnit NOTIFY filterChanged)
    Q_PROPERTY(QString text READ text WRITE setText NOTIFY filterChanged)
    Q_PROPERTY(QString scope READ scope WRITE setScope NOTIFY filterChanged) // all, system, user
    Q_PROPERTY(QString sortColumn READ sortColumn NOTIFY sortChanged) // time, level, source, message
    Q_PROPERTY(bool descending READ descending NOTIFY sortChanged)

public:
    explicit Reader(QObject* parent = nullptr);
    ~Reader() override;

    QAbstractItemModel* model() { return &m_sorted; }
    EventModel* source() { return &m_model; }
    int count() const { return m_model.count(); }
    int cap() const { return m_model.cap(); }
    void setCap(int cap);
    QString state() const { return m_state; }
    bool capped() const { return m_capped; }
    bool partial() const { return m_partial; }
    QString error() const { return m_error; }
    bool follow() const { return m_follow; }
    void setFollow(bool follow);

    QString view() const { return m_filter.view; }
    void setView(const QString& view) { change(m_filter.view, view); }
    int level() const { return m_filter.level; }
    void setLevel(int level);
    QString range() const { return m_filter.range; }
    void setRange(const QString& range) { change(m_filter.range, range); }
    QString unit() const { return m_filter.unit; }
    void setUnit(const QString& unit) { change(m_filter.unit, unit); }
    QString text() const { return m_filter.text; }
    void setText(const QString& text) { change(m_filter.text, text); }
    QString scope() const;
    void setScope(const QString& scope);

    QString sortColumn() const { return m_sortColumn; }
    bool descending() const { return m_sorted.sortOrder() == Qt::DescendingOrder; }

    Q_INVOKABLE void reload();
    Q_INVOKABLE void sortBy(const QString& column);
    Q_INVOKABLE void copy(const QString& text) const;
    // The shown entries, in the shown order, to a file; "" or the reason.
    Q_INVOKABLE QString save(const QUrl& url) const;
    Q_INVOKABLE QVariantMap entry(int row) const;

signals:
    void countChanged();
    void capChanged();
    void stateChanged();
    void followChanged();
    void filterChanged();
    void sortChanged();

private:
    void change(QString& field, const QString& value);
    void start();
    void stop();
    void begin(QProcess*& process, bool follow);
    void read(QProcess* process, bool follow);
    void flush();
    void setState(const QString& state, const QString& error = {});
    void finished(QProcess* process, bool follow);

    EventModel m_model;
    QSortFilterProxyModel m_sorted;
    journal::Filter m_filter;
    QProcess* m_load = nullptr;
    QProcess* m_live = nullptr;
    QByteArray m_loadErrors;
    QList<journal::Entry> m_pending; // read, not in the model yet
    QList<journal::Entry> m_pendingLive;
    QTimer m_flushTimer;
    QTimer m_reloadTimer;
    QString m_state = QStringLiteral("loading");
    QString m_error;
    QString m_sortColumn = QStringLiteral("time");
    bool m_capped = false;
    bool m_partial = false;
    bool m_follow = false;
    bool m_loadCapped = false; // the cap was hit while loading, the process is being stopped
};
