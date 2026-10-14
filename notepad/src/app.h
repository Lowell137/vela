// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

// What Notepad's QML asks the app rather than a tab (`App`).

#include <QCoreApplication>
#include <QFontDatabase>
#include <QObject>
#include <QProcess>
#include <QStringList>

class App : public QObject {
    Q_OBJECT

public:
    using QObject::QObject;

    // Ctrl+Shift+N: another Notepad, empty (it doesn't reopen the last
    // session's files, this window has them).
    Q_INVOKABLE void newWindow() const
    {
        QProcess::startDetached(QCoreApplication::applicationFilePath(), { QStringLiteral("--new-window") });
    }

    // The font until one is chosen: the system's fixed-width font.
    Q_INVOKABLE QString fixedFont() const
    {
        return QFontDatabase::systemFont(QFontDatabase::FixedFont).family();
    }

    // Edit > Font: the installed families.
    Q_INVOKABLE QStringList fontFamilies() const
    {
        QStringList shown;
        const QStringList families = QFontDatabase::families();
        for (const QString& family : families) {
            if (!QFontDatabase::isPrivateFamily(family)) {
                shown.append(family);
            }
        }
        return shown;
    }
};
