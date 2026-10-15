// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// iCalendar (RFC 5545) files: the VEVENTs this calendar knows, read and written.

#pragma once

#include "event.h"

namespace cal::ics {

// Events of a .ics text; what isn't understood (VTODO, VTIMEZONE, ...) is skipped.
QList<Event> parse(const QByteArray& text);
QByteArray write(const QList<Event>& events);

} // namespace cal::ics
