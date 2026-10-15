// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

// What ffmpeg is told to render the timeline: one input per clip (cut with
// -ss/-t), one filter_complex that turns each clip into the canvas's size,
// frame rate and stereo 48 kHz, concatenates them, draws the texts and mixes
// the audio track in. And what ffprobe says about a file.

#include "timeline.h"

#include <QSize>
#include <QVariantMap>

struct ExportSettings {
    QString format = QStringLiteral("mp4"); // mp4, webm, mp3, ogg
    QSize size; // empty: the first picture's
    double fps = 0; // 0: the first video's
    QString quality = QStringLiteral("high"); // high, medium, small
    bool hardware = false; // NVENC
    QString font; // the texts' font file
};

QSize exportCanvas(const Tracks& tracks, const ExportSettings& settings);
double exportFps(const Tracks& tracks, const ExportSettings& settings);
QString exportFilter(const Tracks& tracks, const ExportSettings& settings);
QStringList exportArguments(const Tracks& tracks, const ExportSettings& settings, const QString& output);
QString atempoChain(double speed); // atempo takes 0.5..2 here: chained beyond
QString defaultFont(); // fontconfig's sans-serif file

// ffprobe -print_format json -show_format -show_streams: the media pool's
// item (path, name, kind video/audio/image, duration, width, height, fps,
// hasAudio); empty when it's nothing playable.
QVariantMap parseProbe(const QByteArray& json, const QString& path);
