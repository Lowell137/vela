// SPDX-FileCopyrightText: 2026 Brando Giuffrida
// SPDX-License-Identifier: GPL-3.0-or-later

// The lines for the shell's Game Bar (game_stats.h).

extern "C" {
#include "game_stats.h"
}

#include <gtest/gtest.h>

#include <string>

namespace {

std::string perf(double fps, int missed)
{
    char line[64];
    EXPECT_TRUE(vela_game_perf_line(line, sizeof(line), fps, missed));
    return line;
}

std::string app(const char *id, pid_t pid)
{
    char line[160];
    EXPECT_TRUE(vela_game_app_line(line, sizeof(line), id, pid));
    return line;
}

} // namespace

TEST(GameStats, PerfRoundsToOneDecimal)
{
    EXPECT_EQ(perf(59.94, 2), "perf 59.9 16.7 2");
    EXPECT_EQ(perf(144.0, 0), "perf 144.0 6.9 0");
}

TEST(GameStats, PerfWithoutFramesIsZero)
{
    EXPECT_EQ(perf(0.0, 0), "perf 0.0 0.0 0");
    EXPECT_EQ(perf(-1.0, -3), "perf 0.0 0.0 0");
}

TEST(GameStats, AppIdNeverHasSpacesOrNewlines)
{
    EXPECT_EQ(app("steam_app_1245620", 4242), "perfapp steam_app_1245620 4242");
    EXPECT_EQ(app("my game\nperf 1 2 3\t", 7), "perfapp my_game_perf_1_2_3_ 7");
    EXPECT_EQ(app("", 0), "perfapp - 0");
    EXPECT_EQ(app(nullptr, -1), "perfapp - 0");
}

TEST(GameStats, LongIdIsCutAndStillFits)
{
    char line[160];
    EXPECT_TRUE(vela_game_app_line(line, sizeof(line), std::string(500, 'a').c_str(), 1));
    EXPECT_EQ(std::string(line).size(), size_t(8 + 127 + 2));
}
