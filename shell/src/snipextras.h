// SPDX-FileCopyrightText: 2026 Brando Giuffrida
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QColor>
#include <QString>

// The small logic of the Snipping Tool's delay timer and color picker.
namespace SnipExtras {

// The next delay in the cycle None -> 3 -> 5 -> 10 -> None (seconds).
inline int nextDelay(int seconds)
{
    switch (seconds) {
    case 0: return 3;
    case 3: return 5;
    case 5: return 10;
    default: return 0;
    }
}

// "#RRGGBB", upper case, ignoring alpha.
inline QString hexColor(const QColor& color)
{
    return color.name(QColor::HexRgb).toUpper();
}

} // namespace SnipExtras
