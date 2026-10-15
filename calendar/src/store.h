// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// The events on disk, what QML asks of them, and the reminders' timer.

#pragma once

#include "event.h"

#include <QObject>
#include <QSet>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>

class Store : public QObject {
    Q_OBJECT
    Q_PROPERTY(int revision READ revision NOTIFY changed)
public:
    // `path` is events.json; empty: ~/.local/share/Vela/calendar/events.json.
    explicit Store(const QString& path = {}, QObject* parent = nullptr);

    int revision() const { return m_revision; }
    const QList<cal::Event>& events() const { return m_events; }

    // Occurrences touching the days from..to, as maps for QML.
    Q_INVOKABLE QVariantList occurrences(int fromYear, int fromMonth, int fromDay, int toYear, int toMonth, int toDay) const;
    Q_INVOKABLE QVariantMap event(const QString& id) const;
    // Adds (empty id) or replaces; returns the id.
    Q_INVOKABLE QString save(const QVariantMap& values);
    Q_INVOKABLE void remove(const QString& id);
    Q_INVOKABLE void removeOccurrence(const QString& id, const QString& day); // day: yyyy-MM-dd
    Q_INVOKABLE int icsCount(const QString& fileOrUrl) const; // -1: unreadable
    Q_INVOKABLE int importIcs(const QString& fileOrUrl); // -1: unreadable
    Q_INVOKABLE bool exportIcs(const QString& fileOrUrl) const;
    // Whether a reminder is still to come: the process is kept for them.
    Q_INVOKABLE bool remindersPending() const;
    Q_INVOKABLE void setWindowVisible(bool visible); // closing it with no reminder pending ends the process

    // Reminders that fell due since the last call; shown by notify().
    void check(const QDateTime& now);

signals:
    void changed();
    void showRequested();
    void importRequested(const QString& path);

private:
    void load();
    void write();
    void touch();
    void notify(const cal::Reminder& reminder);

    QString m_path;
    QList<cal::Event> m_events;
    int m_revision = 0;
    QDateTime m_checked;
    QTimer m_timer;
    bool m_windowVisible = true;
};
