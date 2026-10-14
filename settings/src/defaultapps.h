// SPDX-FileCopyrightText: 2026 Brando Giuffrida
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QObject>
#include <QStringList>
#include <QVariantList>

class AppModel;
class Preferences;

// Apps > Default apps, like Windows 11:
// - common uses (browser, mail, music, video, photos, PDF, archives,
//   documents...): the app opening them, among those that can;
// - a specific file or link type (".mkv", "mp3", "mailto");
// - per app: the types it can open, and "Set default" for all of them.
// The choices go into ~/.config/mimeapps.list (mimeapps.h), read by KDE,
// GNOME, browsers and xdg-open.
class DefaultApps : public QObject {
    Q_OBJECT
    // [{key, label, icon, current: {id, name, icon} or empty, candidates:
    // [...]}]
    Q_PROPERTY(QVariantList categories READ categories NOTIFY changed)
    // The app open in the subpage (per app).
    Q_PROPERTY(QString selectedApp READ selectedApp WRITE setSelectedApp NOTIFY selectedAppChanged)
    Q_PROPERTY(QString selectedName READ selectedName NOTIFY selectedAppChanged)
    Q_PROPERTY(QString selectedIcon READ selectedIcon NOTIFY selectedAppChanged)

public:
    explicit DefaultApps(AppModel* apps, Preferences* prefs = nullptr, QObject* parent = nullptr);

    QVariantList categories() const;
    Q_INVOKABLE void setDefault(const QString& key, const QString& appId);

    // A type from typed text: extension (".pdf", "pdf"), MIME type
    // ("audio/mpeg") or protocol ("mailto", "https"). {found, mime, label,
    // patterns, icon, current, candidates}.
    Q_INVOKABLE QVariantMap lookup(const QString& text) const;
    Q_INVOKABLE void setDefaultForType(const QString& mime, const QString& appId);

    // The apps opening some type: [{id, name, icon, count}], filtered.
    Q_INVOKABLE QVariantList apps(const QString& filter) const;
    // The types the app can open: [{mime, label, patterns, icon, isDefault,
    // current}].
    Q_INVOKABLE QVariantList typesOf(const QString& appId) const;
    // The app becomes the default for all the types it declares.
    Q_INVOKABLE void setDefaultForAll(const QString& appId);

    QString selectedApp() const { return m_selected; }
    void setSelectedApp(const QString& appId);
    QString selectedName() const;
    QString selectedIcon() const;

signals:
    void changed();
    void selectedAppChanged();

private:
    QVariantList candidatesFor(const QStringList& mimes, const QString& current) const;
    QVariantMap appInfo(const QString& appId) const;
    // A system tool (terminal, task manager): the vela.conf program choice,
    // "" for automatic, among the installed programs (plus console tools).
    QVariantMap toolCategory(const QString& key, const char* label, const char* icon,
        const QStringList& programs, const QStringList& cli) const;

    AppModel* m_apps;
    Preferences* m_prefs = nullptr;
    QString m_selected;
};
