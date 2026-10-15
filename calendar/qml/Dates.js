// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// Day arithmetic and the layout of events, shared by the views.
.pragma library

// The 8 calendar colors (event.color indexes them); white text reads on all.
var colors = ["#0078d4", "#c42b1c", "#c75000", "#8a6d00", "#107c10", "#00828f", "#7160e8", "#b4009e"]

function startOfDay(d) { return new Date(d.getFullYear(), d.getMonth(), d.getDate()) }
function addDays(d, n) { return new Date(d.getFullYear(), d.getMonth(), d.getDate() + n) }
function sameDay(a, b) { return a.getFullYear() === b.getFullYear() && a.getMonth() === b.getMonth() && a.getDate() === b.getDate() }
function dayDiff(a, b) { return Math.round((startOfDay(b) - startOfDay(a)) / 86400000) } // round: a day can be 23 or 25 hours
// `first`: 0 Sunday .. 6 Saturday, as Locale.firstDayOfWeek.
function weekStart(d, first) { return addDays(d, -((d.getDay() - first + 7) % 7)) }
function addMonths(d, n) {
    const last = new Date(d.getFullYear(), d.getMonth() + n + 1, 0).getDate()
    return new Date(d.getFullYear(), d.getMonth() + n, Math.min(d.getDate(), last))
}
function isoWeek(d) {
    const t = new Date(Date.UTC(d.getFullYear(), d.getMonth(), d.getDate()))
    t.setUTCDate(t.getUTCDate() + 4 - (t.getUTCDay() || 7))
    return Math.ceil(((t - Date.UTC(t.getUTCFullYear(), 0, 1)) / 86400000 + 1) / 7)
}
function minutesOfDay(ms) { const d = new Date(ms); return d.getHours() * 60 + d.getMinutes() }

// Shown as a bar on top: all-day, or over midnight (an end at exactly 00:00 stays on the day before).
function lastDay(o) {
    return new Date(o.allDay || o.endMs <= o.startMs ? o.endMs : o.endMs - 1)
}
function isBar(o) { return o.allDay || dayDiff(new Date(o.startMs), lastDay(o)) >= 1 }

// Bars over `n` days from `first`: [{occ, col, span, lane, cutLeft, cutRight}] and the lanes used.
function layoutBars(occs, first, n) {
    const bars = []
    for (const o of occs) {
        if (!isBar(o)) continue
        const c0 = dayDiff(first, new Date(o.startMs))
        const c1 = dayDiff(first, lastDay(o))
        if (c1 < 0 || c0 > n - 1) continue
        bars.push({ occ: o, col: Math.max(0, c0), span: Math.min(n - 1, c1) - Math.max(0, c0) + 1, lane: 0, cutLeft: c0 < 0, cutRight: c1 > n - 1 })
    }
    bars.sort((a, b) => a.col - b.col || b.span - a.span)
    const ends = []
    for (const b of bars) {
        let lane = ends.findIndex(e => e < b.col)
        if (lane < 0) { lane = ends.length; ends.push(0) }
        ends[lane] = b.col + b.span - 1
        b.lane = lane
    }
    return { bars: bars, lanes: ends.length }
}

// Single-day events of one day side by side: [{occ, lane, lanes}]; overlapping ones share the width.
function layoutColumn(items) {
    const out = items.map(o => ({ occ: o, lane: 0, lanes: 1, s: o.startMs, e: Math.max(o.endMs, o.startMs + 20 * 60000) }))
    out.sort((a, b) => a.s - b.s || b.e - a.e)
    let cluster = []
    let laneEnds = []
    let clusterEnd = -Infinity
    const flush = () => {
        for (const c of cluster) c.lanes = laneEnds.length
        cluster = []
        laneEnds = []
    }
    for (const it of out) {
        if (it.s >= clusterEnd) flush()
        let lane = laneEnds.findIndex(e => e <= it.s)
        if (lane < 0) { lane = laneEnds.length; laneEnds.push(0) }
        laneEnds[lane] = it.e
        it.lane = lane
        cluster.push(it)
        clusterEnd = Math.max(clusterEnd, it.e)
    }
    flush()
    return out
}

// The single-day events of days first .. first+n-1, per day.
function perDay(occs, first, n) {
    const days = []
    for (let i = 0; i < n; i++) days.push([])
    for (const o of occs) {
        if (isBar(o)) continue
        const c = dayDiff(first, new Date(o.startMs))
        if (c >= 0 && c < n) days[c].push(o)
    }
    return days
}

// The time as the locale writes it, without seconds (1 is Locale.ShortFormat).
function timeText(ms) { return Qt.formatTime(new Date(ms), Qt.locale().timeFormat(1)) }
