// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QString>
#include <QStringList>

// ~/.config/MangoHud/MangoHud.conf, edited in place: "key", "key=value",
// comments (#) and keys we don't know stay as they are; a changed key keeps
// its line. Shared by Settings (the Gaming page) and its test.
class MangoHudConf {
public:
    // What the Gaming page offers as switches.
    static QStringList flagKeys()
    {
        return { QStringLiteral("fps"), QStringLiteral("frametime"), QStringLiteral("frame_timing"),
            QStringLiteral("cpu_stats"), QStringLiteral("cpu_temp"), QStringLiteral("gpu_stats"),
            QStringLiteral("gpu_temp"), QStringLiteral("vram"), QStringLiteral("ram"), QStringLiteral("battery") };
    }

    static MangoHudConf parse(const QString& text)
    {
        MangoHudConf conf;
        conf.m_lines = text.split(u'\n');
        if (!conf.m_lines.isEmpty() && conf.m_lines.last().isEmpty()) {
            conf.m_lines.removeLast(); // the final newline; text() puts it back
        }
        return conf;
    }

    QString text() const { return m_lines.isEmpty() ? QString() : m_lines.join(u'\n') + u'\n'; }

    // The value of an active "key=value" line; "" for a bare "key"; null if
    // the key isn't set.
    QString value(const QString& key) const
    {
        const int i = find(key);
        if (i < 0) {
            return {};
        }
        const qsizetype eq = m_lines.at(i).indexOf(u'=');
        return eq < 0 ? QStringLiteral("") : m_lines.at(i).mid(eq + 1).trimmed();
    }

    // A switch: a bare key or any value but 0 is on. MangoHud shows fps,
    // frame_timing, cpu_stats and gpu_stats unless told otherwise.
    bool flag(const QString& key) const
    {
        const QString v = value(key);
        if (v.isNull()) {
            return defaultOn().contains(key);
        }
        return v != QLatin1String("0");
    }
    // On: a bare "key"; off: "key=0" (removing it would bring a default back).
    void setFlag(const QString& key, bool on) { setLine(key, on ? key : key + QStringLiteral("=0")); }

    void setValue(const QString& key, const QString& value)
    {
        setLine(key, key + u'=' + value);
    }

    // "minimal" (fps only), "full" (every switch), "battery" (fps, frame time
    // and the battery).
    void applyPreset(const QString& name)
    {
        const QStringList battery { QStringLiteral("fps"), QStringLiteral("frametime"), QStringLiteral("battery") };
        for (const QString& key : flagKeys()) {
            const bool on = name == QLatin1String("full") || key == QLatin1String("fps")
                || (name == QLatin1String("battery") && battery.contains(key));
            setFlag(key, on);
        }
    }

private:
    static QStringList defaultOn()
    {
        return { QStringLiteral("fps"), QStringLiteral("frame_timing"), QStringLiteral("cpu_stats"), QStringLiteral("gpu_stats") };
    }

    // The first active (not commented) line for `key`.
    int find(const QString& key) const
    {
        for (int i = 0; i < m_lines.size(); ++i) {
            const QString line = m_lines.at(i).trimmed();
            if (line.startsWith(u'#')) {
                continue;
            }
            if (line.section(u'=', 0, 0).trimmed() == key) {
                return i;
            }
        }
        return -1;
    }

    void setLine(const QString& key, const QString& line)
    {
        const int i = find(key);
        if (i >= 0) {
            m_lines[i] = line;
        } else {
            m_lines.append(line);
        }
    }

    QStringList m_lines;
};
