// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QAbstractListModel>
#include <QTimer>
#include <QVariantList>

// The notes, newest edit first, saved as one JSON file. The list shows the
// ones that match `filter`; typing is written 500 ms after the last change.
class NoteStore : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(QString filter READ filter WRITE setFilter NOTIFY filterChanged)
    Q_PROPERTY(QVariantList openIds READ openIds NOTIFY openIdsChanged)
    Q_PROPERTY(bool quitting READ quitting WRITE setQuitting)
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum Role { IdRole = Qt::UserRole + 1, TextRole, PreviewRole, ColorRole, ModifiedRole };

    struct Note {
        int id = 0;
        QString text;
        QString color;
        int x = 0, y = 0, width = 300, height = 300;
        bool open = false;
        qint64 modified = 0; // ms since the epoch
    };

    explicit NoteStore(const QString& path, int saveDelayMs = 500, QObject* parent = nullptr);
    ~NoteStore() override { flush(); }

    static QString defaultPath();
    // Names of the seven colors, with the body, strip and text colors for the UI.
    static const QStringList& colorNames();
    Q_INVOKABLE static QVariantList palette();
    static QString validColor(const QString& name);
    // Whether the list shows a note with this text for this filter.
    static bool matches(const QString& text, const QString& filter);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString filter() const { return m_filter; }
    void setFilter(const QString& filter);
    QVariantList openIds() const;
    bool quitting() const { return m_quitting; }
    void setQuitting(bool quitting) { m_quitting = quitting; }
    int count() const { return m_notes.size(); }
    bool dirty() const { return m_dirty; }

    Q_INVOKABLE int create(const QString& color);
    Q_INVOKABLE void remove(int id);
    Q_INVOKABLE QVariantMap get(int id) const;
    Q_INVOKABLE void setText(int id, const QString& text);
    Q_INVOKABLE void setColor(int id, const QString& color);
    Q_INVOKABLE void setGeometry(int id, int x, int y, int width, int height);
    Q_INVOKABLE void setOpen(int id, bool open);
    Q_INVOKABLE void closeAll();
    Q_INVOKABLE void flush();

    const QList<Note>& notes() const { return m_notes; }

signals:
    void filterChanged();
    void openIdsChanged();
    void countChanged();

private:
    int indexOf(int id) const;
    void load();
    bool save();
    void touch();
    void rebuildView();

    QString m_path;
    QList<Note> m_notes; // newest edit first
    QList<int> m_view; // indices into m_notes that match the filter
    QString m_filter;
    QTimer m_timer;
    bool m_dirty = false;
    bool m_quitting = false;
};
