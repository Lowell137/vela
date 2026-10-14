// SPDX-FileCopyrightText: 2026 Brando Giuffrida
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QFileInfo>
#include <QString>
#include <QStringList>

namespace vela {

// The arguments that make a terminal run the command following them. Most take
// "-e"; the others differ, and "-e" would be misread (kitty runs the command
// itself, gnome-terminal's -e is deprecated, ptyxis wants -x, wezterm needs
// "start --"). Unknown programs keep "-e".
inline QStringList terminalExecArguments(const QString& program)
{
    const QString name = QFileInfo(program).fileName();
    if (name == QLatin1String("kitty")) {
        return {};
    }
    if (name == QLatin1String("gnome-terminal")) {
        return { QStringLiteral("--") };
    }
    if (name == QLatin1String("ptyxis")) {
        return { QStringLiteral("-x") };
    }
    if (name == QLatin1String("wezterm")) {
        return { QStringLiteral("start"), QStringLiteral("--") };
    }
    return { QStringLiteral("-e") };
}

// The terminal program and its flag, ready to be followed by the command.
inline QStringList terminalCommand(const QString& program)
{
    return QStringList { program } + terminalExecArguments(program);
}

} // namespace vela
