// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mangohudconf.h"

#include <QClipboard>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QObject>
#include <QSaveFile>
#include <QStandardPaths>
#include <QVariantMap>

// Settings > Gaming > MangoHud: the common keys of
// ~/.config/MangoHud/MangoHud.conf (MangoHudConf keeps the rest of the file
// as it is). Every change rereads the file first, so edits made elsewhere
// in the meantime stay.
class MangoHud : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool installed READ installed CONSTANT)
    Q_PROPERTY(QString path READ path CONSTANT)
    // {fps: true, frametime: false, ...} (MangoHudConf::flagKeys) and
    // {position, font_size, background_alpha, toggle_hud}: "" when unset.
    Q_PROPERTY(QVariantMap flags READ flags NOTIFY changed)
    Q_PROPERTY(QVariantMap values READ values NOTIFY changed)

public:
    using QObject::QObject;

    bool installed() const { return !QStandardPaths::findExecutable(QStringLiteral("mangohud")).isEmpty(); }
    QString path() const
    {
        return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + QStringLiteral("/MangoHud/MangoHud.conf");
    }

    QVariantMap flags() const
    {
        const MangoHudConf conf = load();
        QVariantMap map;
        for (const QString& key : MangoHudConf::flagKeys()) {
            map.insert(key, conf.flag(key));
        }
        return map;
    }
    QVariantMap values() const
    {
        const MangoHudConf conf = load();
        QVariantMap map;
        for (const char* key : { "position", "font_size", "background_alpha", "toggle_hud" }) {
            map.insert(QLatin1String(key), conf.value(QLatin1String(key)));
        }
        return map;
    }

    Q_INVOKABLE void setFlag(const QString& key, bool on)
    {
        MangoHudConf conf = load();
        conf.setFlag(key, on);
        save(conf);
    }
    Q_INVOKABLE void setValue(const QString& key, const QString& value)
    {
        MangoHudConf conf = load();
        conf.setValue(key, value);
        save(conf);
    }
    Q_INVOKABLE void applyPreset(const QString& name)
    {
        MangoHudConf conf = load();
        conf.applyPreset(name);
        save(conf);
    }
    // For Steam's launch options.
    Q_INVOKABLE void copyLaunchOption() const
    {
        QGuiApplication::clipboard()->setText(QStringLiteral("mangohud %command%"));
    }

signals:
    void changed();

private:
    MangoHudConf load() const
    {
        QFile file(path());
        return MangoHudConf::parse(file.open(QIODevice::ReadOnly) ? QString::fromUtf8(file.readAll()) : QString());
    }
    void save(const MangoHudConf& conf)
    {
        QDir().mkpath(QFileInfo(path()).absolutePath());
        QSaveFile file(path());
        if (!file.open(QIODevice::WriteOnly) || file.write(conf.text().toUtf8()) < 0 || !file.commit()) {
            qWarning("vela-settings: can't write %s", qPrintable(path()));
        }
        emit changed();
    }
};
