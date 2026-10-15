// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "exportcommand.h"

#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>

#include <cmath>

namespace {

const QString silence = QStringLiteral("anullsrc=r=48000:cl=stereo");
const QString stereo = QStringLiteral("aformat=sample_rates=48000:channel_layouts=stereo");

bool audioOnly(const ExportSettings& s)
{
    return s.format == QLatin1String("mp3") || s.format == QLatin1String("ogg");
}

bool heard(const Clip& music)
{
    return !music.muted && music.hasAudio;
}

int even(double value)
{
    return std::max(2, int(value / 2) * 2);
}

// The picture's size once turned and cropped.
QSize shaped(const Clip& c)
{
    double w = c.width;
    double h = c.height;
    if (c.rotate == 90 || c.rotate == 270) {
        std::swap(w, h);
    }
    const QStringList ratio = c.crop.split(u':');
    if (ratio.size() == 2 && h > 0) {
        const double aspect = ratio[0].toDouble() / ratio[1].toDouble();
        if (w / h > aspect) {
            w = h * aspect;
        } else {
            h = w / aspect;
        }
    }
    return { even(w), even(h) };
}

// A value inside the filter graph: escaped for the filter's options, then
// for the graph (ffmpeg-filters(1), "Notes on filtergraph escaping").
QString escaped(const QString& value)
{
    QString option;
    for (const QChar c : value) {
        if (c == u'\\' || c == u'\'' || c == u':') {
            option += u'\\';
        }
        option += c;
    }
    QString graph;
    for (const QChar c : option) {
        if (QStringLiteral("\\'[],;").contains(c)) {
            graph += u'\\';
        }
        graph += c;
    }
    return graph;
}

// A clip's sound: speed, volume, fades, the common format.
QString audioChain(const Clip& c)
{
    QStringList chain { QStringLiteral("asetpts=PTS-STARTPTS") };
    if (c.speed != 1) {
        chain.append(atempoChain(c.speed));
    }
    if (c.volume != 1) {
        chain.append(QStringLiteral("volume=") + num(c.volume));
    }
    if (c.fadeIn > 0) {
        chain.append(QStringLiteral("afade=t=in:st=0:d=") + num(c.fadeIn));
    }
    if (c.fadeOut > 0) {
        chain.append(QStringLiteral("afade=t=out:st=%1:d=%2").arg(num(c.length() - c.fadeOut), num(c.fadeOut)));
    }
    chain.append(stereo);
    return chain.join(u',');
}

QString quality(const ExportSettings& s, const char* high, const char* medium, const char* small)
{
    return QString::fromLatin1(s.quality == QLatin1String("small") ? small : s.quality == QLatin1String("medium") ? medium : high);
}

} // namespace

QSize exportCanvas(const Tracks& tracks, const ExportSettings& settings)
{
    if (!settings.size.isEmpty()) {
        return settings.size;
    }
    for (const Clip& c : tracks.video) {
        if (c.width > 0 && c.height > 0) {
            return shaped(c);
        }
    }
    return { 1920, 1080 };
}

double exportFps(const Tracks& tracks, const ExportSettings& settings)
{
    if (settings.fps > 0) {
        return settings.fps;
    }
    for (const Clip& c : tracks.video) {
        if (c.kind == Clip::Video && c.fps > 0) {
            return std::round(c.fps * 1000) / 1000;
        }
    }
    return 30;
}

QString atempoChain(double speed)
{
    QStringList chain;
    while (speed < 0.5 - 1e-9) {
        chain.append(QStringLiteral("atempo=0.5"));
        speed /= 0.5;
    }
    while (speed > 2 + 1e-9) {
        chain.append(QStringLiteral("atempo=2"));
        speed /= 2;
    }
    if (std::abs(speed - 1) > 1e-9) {
        chain.append(QStringLiteral("atempo=") + num(speed));
    }
    return chain.join(u',');
}

QString exportFilter(const Tracks& tracks, const ExportSettings& settings)
{
    const bool pictures = !audioOnly(settings);
    const QSize canvas = exportCanvas(tracks, settings);
    const QString fps = num(exportFps(tracks, settings));
    const QString size = QStringLiteral("%1:%2").arg(canvas.width()).arg(canvas.height());
    const QString fit = QStringLiteral("scale=%1:force_original_aspect_ratio=decrease,pad=%1:(ow-iw)/2:(oh-ih)/2,setsar=1,fps=%2,format=yuv420p").arg(size, fps);

    QStringList graph;
    QString concat;
    int input = 0;
    const QList<Clip>& video = tracks.video;
    for (int i = 0; i < video.size(); i++) {
        const Clip& c = video[i];
        const QString length = num(c.length());
        if (pictures) {
            if (c.kind == Clip::Gap) {
                graph.append(QStringLiteral("color=c=black:s=%1x%2:r=%3:d=%4,format=yuv420p[v%5]")
                                 .arg(canvas.width()).arg(canvas.height()).arg(fps, length).arg(i));
            } else {
                QStringList chain;
                if (c.kind == Clip::Video) {
                    chain.append(c.speed == 1 ? QStringLiteral("setpts=PTS-STARTPTS") : QStringLiteral("setpts=(PTS-STARTPTS)/") + num(c.speed));
                }
                if (c.rotate == 90) {
                    chain.append(QStringLiteral("transpose=1"));
                } else if (c.rotate == 180) {
                    chain.append(QStringLiteral("hflip,vflip"));
                } else if (c.rotate == 270) {
                    chain.append(QStringLiteral("transpose=2"));
                }
                if (c.flipH) {
                    chain.append(QStringLiteral("hflip"));
                }
                if (c.flipV) {
                    chain.append(QStringLiteral("vflip"));
                }
                if (!c.crop.isEmpty()) {
                    const QSize cut = shaped(c);
                    chain.append(QStringLiteral("crop=%1:%2").arg(cut.width()).arg(cut.height()));
                }
                chain.append(fit);
                if (c.fadeIn > 0) {
                    chain.append(QStringLiteral("fade=t=in:st=0:d=") + num(c.fadeIn));
                }
                if (c.fadeOut > 0) {
                    chain.append(QStringLiteral("fade=t=out:st=%1:d=%2").arg(num(c.length() - c.fadeOut), num(c.fadeOut)));
                }
                graph.append(QStringLiteral("[%1:v]%2[v%3]").arg(input).arg(chain.join(u','), QString::number(i)));
            }
            concat += QStringLiteral("[v%1]").arg(i);
        }
        if (c.kind == Clip::Video && c.hasAudio && !c.muted) {
            graph.append(QStringLiteral("[%1:a]%2,apad,atrim=duration=%3[a%4]").arg(input).arg(audioChain(c), length).arg(i));
        } else {
            graph.append(QStringLiteral("%1,atrim=duration=%2[a%3]").arg(silence, length).arg(i));
        }
        concat += QStringLiteral("[a%1]").arg(i);
        if (c.kind != Clip::Gap) {
            input++;
        }
    }

    QStringList texts;
    for (const Clip& t : tracks.texts) {
        const int fontSize = std::max(1, int(std::lround(t.textSize * canvas.height() / 1080.0)));
        const QString y = t.position == QLatin1String("top") ? QStringLiteral("h/10")
            : t.position == QLatin1String("center")          ? QStringLiteral("(h-text_h)/2")
                                                             : QStringLiteral("h-text_h-h/10");
        // One arg() call: the text may hold "%1".
        texts.append(QStringLiteral("drawtext=fontfile=%1:expansion=none:text=%2:fontsize=%3:fontcolor=%4:borderw=%5:bordercolor=black@0.6:x=(w-text_w)/2:y=%6:enable=%7")
                         .arg(escaped(settings.font), escaped(t.text), QString::number(fontSize),
                             QString(t.color).replace(u'#', QStringLiteral("0x")),
                             QString::number(std::max(1, int(std::lround(fontSize / 24.0)))), y,
                             escaped(QStringLiteral("between(t,%1,%2)").arg(num(t.start), num(t.start + t.length())))));
    }

    QStringList music;
    QString mixed;
    for (const Clip& m : tracks.audio) {
        if (heard(m)) {
            music.append(QStringLiteral("[%1:a]%2,adelay=%3:all=1[m%4]").arg(input).arg(audioChain(m)).arg(std::llround(m.start * 1000)).arg(music.size()));
            mixed += QStringLiteral("[m%1]").arg(music.size() - 1);
            input++;
        }
    }

    const QString videoOut = texts.isEmpty() ? QStringLiteral("[v]") : QStringLiteral("[vcat]");
    const QString audioOut = music.isEmpty() ? QStringLiteral("[a]") : QStringLiteral("[acat]");
    graph.append(QStringLiteral("%1concat=n=%2:v=%3:a=1%4%5").arg(concat).arg(video.size()).arg(pictures ? 1 : 0).arg(pictures ? videoOut : QString(), audioOut));
    if (pictures && !texts.isEmpty()) {
        graph.append(QStringLiteral("[vcat]") + texts.join(u',') + QStringLiteral("[v]"));
    }
    if (!music.isEmpty()) {
        graph.append(music);
        graph.append(QStringLiteral("[acat]%1amix=inputs=%2:duration=first:normalize=0[a]").arg(mixed).arg(music.size() + 1));
    }
    return graph.join(u';');
}

QStringList exportArguments(const Tracks& tracks, const ExportSettings& settings, const QString& output)
{
    QStringList args { "-hide_banner", "-nostdin", "-y", "-nostats", "-progress", "pipe:1" };
    const QString fps = num(exportFps(tracks, settings));
    auto cut = [&](const Clip& c) {
        if (c.in > 0) {
            args << "-ss" << num(c.in);
        }
        args << "-t" << num(c.out - c.in) << "-i" << c.source;
    };
    for (const Clip& c : tracks.video) {
        if (c.kind == Clip::Image) {
            args << "-loop" << "1" << "-framerate" << fps << "-t" << num(c.length()) << "-i" << c.source;
        } else if (c.kind == Clip::Video) {
            cut(c);
        }
    }
    for (const Clip& c : tracks.audio) {
        if (heard(c)) {
            cut(c);
        }
    }
    args << "-filter_complex" << exportFilter(tracks, settings);
    if (!audioOnly(settings)) {
        args << "-map" << "[v]";
    }
    args << "-map" << "[a]";
    if (settings.format == QLatin1String("mp3")) {
        args << "-c:a" << "libmp3lame" << "-q:a" << quality(settings, "2", "4", "6");
    } else if (settings.format == QLatin1String("ogg")) {
        args << "-c:a" << "libvorbis" << "-q:a" << quality(settings, "6", "4", "2");
    } else if (settings.format == QLatin1String("webm")) {
        args << "-c:v" << "libvpx-vp9" << "-b:v" << "0" << "-crf" << quality(settings, "31", "36", "42") << "-row-mt" << "1"
             << "-c:a" << "libopus" << "-b:a" << quality(settings, "160k", "128k", "96k");
    } else {
        if (settings.hardware) {
            args << "-c:v" << "h264_nvenc" << "-preset" << "p5" << "-rc" << "vbr" << "-cq" << quality(settings, "19", "24", "30") << "-b:v" << "0";
        } else {
            args << "-c:v" << "libx264" << "-preset" << "medium" << "-crf" << quality(settings, "18", "23", "28");
        }
        args << "-c:a" << "aac" << "-b:a" << quality(settings, "192k", "160k", "128k") << "-movflags" << "+faststart";
    }
    args << output;
    return args;
}

QString defaultFont()
{
    QProcess fc;
    fc.start(QStringLiteral("fc-match"), { QStringLiteral("-f"), QStringLiteral("%{file}"), QStringLiteral("sans-serif:bold") });
    fc.waitForFinished(3000);
    return QString::fromUtf8(fc.readAllStandardOutput()).trimmed();
}

QVariantMap parseProbe(const QByteArray& json, const QString& path)
{
    const QJsonObject probe = QJsonDocument::fromJson(json).object();
    const QJsonObject format = probe["format"].toObject();
    QJsonObject picture;
    bool hasAudio = false;
    for (const QJsonValue& value : probe["streams"].toArray()) {
        const QJsonObject s = value.toObject();
        const QString type = s["codec_type"].toString();
        if (type == QLatin1String("audio")) {
            hasAudio = true;
        } else if (type == QLatin1String("video") && picture.isEmpty() && !s["disposition"].toObject()["attached_pic"].toInt()) {
            picture = s; // a song's cover isn't a picture
        }
    }
    if (picture.isEmpty() && !hasAudio) {
        return {};
    }
    const QString formatName = format["format_name"].toString();
    const bool still = !picture.isEmpty() && (formatName.endsWith(QLatin1String("_pipe")) || formatName == QLatin1String("image2"));
    int width = picture.value("width").toInt();
    int height = picture.value("height").toInt();
    int rotation = picture.value("tags").toObject()["rotate"].toString().toInt();
    for (const QJsonValue& side : picture.value("side_data_list").toArray()) {
        if (side.toObject().contains("rotation")) {
            rotation = side.toObject()["rotation"].toInt();
        }
    }
    if (std::abs(rotation) % 180 == 90) {
        std::swap(width, height); // ffmpeg stands it up when decoding
    }
    const QStringList rate = picture.value("avg_frame_rate").toString().split(u'/');
    const double fps = rate.size() == 2 && rate[1].toDouble() > 0 ? rate[0].toDouble() / rate[1].toDouble() : 0;
    return {
        { "path", path },
        { "name", QFileInfo(path).fileName() },
        { "kind", still ? "image" : picture.isEmpty() ? "audio" : "video" },
        { "duration", still ? 0.0 : format["duration"].toString().toDouble() },
        { "width", width },
        { "height", height },
        { "fps", fps },
        { "hasAudio", hasAudio },
    };
}
