// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// The ffmpeg command an export runs, letter by letter, and one real export.

#include "exportcommand.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>

namespace {

Clip video(const QString& path, double duration, int width = 1920, int height = 1080, double fps = 30, bool audio = true)
{
    Clip c;
    c.source = path;
    c.out = c.duration = duration;
    c.width = width;
    c.height = height;
    c.fps = fps;
    c.hasAudio = audio;
    return c;
}

const QString fit = QStringLiteral("scale=1920:1080:force_original_aspect_ratio=decrease,pad=1920:1080:(ow-iw)/2:(oh-ih)/2,setsar=1,fps=30,format=yuv420p");
const QString stereo = QStringLiteral("aformat=sample_rates=48000:channel_layouts=stereo");

bool run(const QString& program, const QStringList& arguments, QByteArray* out = nullptr)
{
    QProcess process;
    process.start(program, arguments);
    if (!process.waitForFinished(120000) || process.exitCode() != 0) {
        qWarning().noquote() << process.readAllStandardError().right(2000);
        return false;
    }
    if (out) {
        *out = process.readAllStandardOutput();
    }
    return true;
}

} // namespace

class ExportTest : public QObject {
    Q_OBJECT

private slots:
    void oneClip()
    {
        Tracks t;
        t.video = { video("/a.mp4", 10) };
        const QString graph = "[0:v]setpts=PTS-STARTPTS," + fit + "[v0];"
            "[0:a]asetpts=PTS-STARTPTS," + stereo + ",apad,atrim=duration=10[a0];"
            "[v0][a0]concat=n=1:v=1:a=1[v][a]";
        QCOMPARE(exportFilter(t, {}), graph);
        QCOMPARE(exportArguments(t, {}, "/out.mp4"), (QStringList {
            "-hide_banner", "-nostdin", "-y", "-nostats", "-progress", "pipe:1",
            "-t", "10", "-i", "/a.mp4",
            "-filter_complex", graph, "-map", "[v]", "-map", "[a]",
            "-c:v", "libx264", "-preset", "medium", "-crf", "18",
            "-c:a", "aac", "-b:a", "192k", "-movflags", "+faststart", "/out.mp4" }));

        ExportSettings small;
        small.quality = "small";
        small.hardware = true;
        small.size = QSize(1280, 720);
        small.fps = 24;
        const QStringList args = exportArguments(t, small, "/out.mp4");
        QVERIFY(args.join(' ').contains("-c:v h264_nvenc -preset p5 -rc vbr -cq 30 -b:v 0 -c:a aac -b:a 128k"));
        QVERIFY(exportFilter(t, small).contains("scale=1280:720:force_original_aspect_ratio=decrease,pad=1280:720:(ow-iw)/2:(oh-ih)/2,setsar=1,fps=24,"));
    }

    void trimmedClip()
    {
        Tracks t;
        t.video = { video("/a.mp4", 10) };
        t.video[0].in = 2;
        t.video[0].out = 5.5;
        QVERIFY(exportArguments(t, {}, "/o.mp4").join(' ').contains("-ss 2 -t 3.5 -i /a.mp4"));
        QVERIFY(exportFilter(t, {}).contains(stereo + ",apad,atrim=duration=3.5[a0]"));
    }

    void clipsOfDifferentSizesOneSilent()
    {
        Tracks t;
        t.video = { video("/a.mp4", 10), video("/b.mp4", 4, 640, 480, 25, false) };
        QCOMPARE(exportFilter(t, {}), "[0:v]setpts=PTS-STARTPTS," + fit + "[v0];"
            "[0:a]asetpts=PTS-STARTPTS," + stereo + ",apad,atrim=duration=10[a0];"
            "[1:v]setpts=PTS-STARTPTS," + fit + "[v1];"
            "anullsrc=r=48000:cl=stereo,atrim=duration=4[a1];"
            "[v0][a0][v1][a1]concat=n=2:v=1:a=1[v][a]");
        QVERIFY(exportArguments(t, {}, "/o.mp4").join(' ').contains("-t 10 -i /a.mp4 -t 4 -i /b.mp4 -filter_complex"));

        // A gap: black and silence. A muted clip: silence.
        Clip gap;
        gap.kind = Clip::Gap;
        gap.out = 3;
        t.video.insert(1, gap);
        t.video[0].muted = true;
        const QString graph = exportFilter(t, {});
        QVERIFY(graph.startsWith("[0:v]setpts=PTS-STARTPTS," + fit + "[v0];anullsrc=r=48000:cl=stereo,atrim=duration=10[a0];"
            "color=c=black:s=1920x1080:r=30:d=3,format=yuv420p[v1];anullsrc=r=48000:cl=stereo,atrim=duration=3[a1];"
            "[1:v]"));
        QVERIFY(graph.endsWith("[v0][a0][v1][a1][v2][a2]concat=n=3:v=1:a=1[v][a]"));
    }

    void speed()
    {
        QCOMPARE(atempoChain(1), QString());
        QCOMPARE(atempoChain(2), "atempo=2");
        QCOMPARE(atempoChain(0.25), "atempo=0.5,atempo=0.5");
        QCOMPARE(atempoChain(3), "atempo=2,atempo=1.5");
        QCOMPARE(atempoChain(0.75), "atempo=0.75");

        Tracks t;
        t.video = { video("/a.mp4", 10) };
        t.video[0].speed = 2;
        QCOMPARE(exportFilter(t, {}), "[0:v]setpts=(PTS-STARTPTS)/2," + fit + "[v0];"
            "[0:a]asetpts=PTS-STARTPTS,atempo=2," + stereo + ",apad,atrim=duration=5[a0];"
            "[v0][a0]concat=n=1:v=1:a=1[v][a]");
        QVERIFY(exportArguments(t, {}, "/o.mp4").join(' ').contains("-t 10 -i /a.mp4")); // the source's seconds
    }

    void fadesVolumeAndTransforms()
    {
        Tracks t;
        t.video = { video("/a.mp4", 10) };
        Clip& c = t.video[0];
        c.fadeIn = 1;
        c.fadeOut = 2;
        c.volume = 1.5;
        QCOMPARE(exportFilter(t, {}), "[0:v]setpts=PTS-STARTPTS," + fit + ",fade=t=in:st=0:d=1,fade=t=out:st=8:d=2[v0];"
            "[0:a]asetpts=PTS-STARTPTS,volume=1.5,afade=t=in:st=0:d=1,afade=t=out:st=8:d=2," + stereo + ",apad,atrim=duration=10[a0];"
            "[v0][a0]concat=n=1:v=1:a=1[v][a]");

        // Turned upright and cut square: the canvas follows.
        c.fadeIn = c.fadeOut = 0;
        c.volume = 1;
        c.rotate = 90;
        c.flipH = true;
        c.crop = "1:1";
        QCOMPARE(exportCanvas(t, {}), QSize(1080, 1080));
        QVERIFY(exportFilter(t, {}).startsWith("[0:v]setpts=PTS-STARTPTS,transpose=1,hflip,crop=1080:1080,scale=1080:1080:"));
        c.rotate = 180;
        c.flipH = false;
        c.crop = "9:16";
        QCOMPARE(exportCanvas(t, {}), QSize(606, 1080));
        QVERIFY(exportFilter(t, {}).startsWith("[0:v]setpts=PTS-STARTPTS,hflip,vflip,crop=606:1080,scale=606:1080:"));
    }

    void textOverlay()
    {
        Tracks t;
        t.video = { video("/a.mp4", 10) };
        Clip text;
        text.kind = Clip::Text;
        text.start = 1;
        text.out = 3;
        text.text = "Hi: it's 100%";
        text.color = "#ff0000";
        t.texts = { text };
        text.start = 5;
        text.text = "[top]";
        text.position = "top";
        text.textSize = 36;
        t.texts.append(text);
        ExportSettings settings;
        settings.font = "/f.ttf";
        settings.size = QSize(1280, 720);
        QVERIFY(exportFilter(t, settings).endsWith(
            R"([v0][a0]concat=n=1:v=1:a=1[vcat][a];)"
            R"([vcat]drawtext=fontfile=/f.ttf:expansion=none:text=Hi\\: it\\\'s 100%:fontsize=48:fontcolor=0xff0000:borderw=2:bordercolor=black@0.6:x=(w-text_w)/2:y=h-text_h-h/10:enable=between(t\,1\,4),)"
            R"(drawtext=fontfile=/f.ttf:expansion=none:text=\[top\]:fontsize=24:fontcolor=0xff0000:borderw=1:bordercolor=black@0.6:x=(w-text_w)/2:y=h/10:enable=between(t\,5\,8)[v])"));
    }

    void imageClip()
    {
        Tracks t;
        Clip c;
        c.kind = Clip::Image;
        c.source = "/p.png";
        c.out = 5;
        c.width = 800;
        c.height = 600;
        t.video = { c };
        QCOMPARE(exportFilter(t, {}), "[0:v]scale=800:600:force_original_aspect_ratio=decrease,pad=800:600:(ow-iw)/2:(oh-ih)/2,setsar=1,fps=30,format=yuv420p[v0];"
            "anullsrc=r=48000:cl=stereo,atrim=duration=5[a0];"
            "[v0][a0]concat=n=1:v=1:a=1[v][a]");
        QVERIFY(exportArguments(t, {}, "/o.mp4").join(' ').contains("-loop 1 -framerate 30 -t 5 -i /p.png"));
    }

    void audioTrackMix()
    {
        Tracks t;
        t.video = { video("/a.mp4", 10) };
        Clip m;
        m.kind = Clip::Audio;
        m.source = "/m.mp3";
        m.out = m.duration = 30;
        m.start = 2;
        m.volume = 0.5;
        m.hasAudio = true;
        t.audio = { m };
        m.muted = true; // left out
        m.start = 12;
        t.audio.append(m);
        const QString graph = exportFilter(t, {});
        QVERIFY(graph.endsWith("[v0][a0]concat=n=1:v=1:a=1[v][acat];"
            "[1:a]asetpts=PTS-STARTPTS,volume=0.5," + stereo + ",adelay=2000:all=1[m0];"
            "[acat][m0]amix=inputs=2:duration=first:normalize=0[a]"));
        QVERIFY(exportArguments(t, {}, "/o.mp4").join(' ').contains("-t 10 -i /a.mp4 -t 30 -i /m.mp3 -filter_complex"));

        ExportSettings mp3;
        mp3.format = "mp3";
        QCOMPARE(exportFilter(t, mp3), "[0:a]asetpts=PTS-STARTPTS," + stereo + ",apad,atrim=duration=10[a0];"
            "[a0]concat=n=1:v=0:a=1[acat];"
            "[1:a]asetpts=PTS-STARTPTS,volume=0.5," + stereo + ",adelay=2000:all=1[m0];"
            "[acat][m0]amix=inputs=2:duration=first:normalize=0[a]");
        const QString args = exportArguments(t, mp3, "/o.mp3").join(' ');
        QVERIFY(args.contains("-map [a] -c:a libmp3lame -q:a 2 /o.mp3"));
        QVERIFY(!args.contains("[v]"));
    }

    void probes()
    {
        const QByteArray video = R"({"streams":[{"codec_type":"video","codec_name":"h264","width":1920,"height":1080,
            "avg_frame_rate":"30000/1001","side_data_list":[{"rotation":-90}]},{"codec_type":"audio"}],
            "format":{"format_name":"mov,mp4,m4a,3gp,3g2,mj2","duration":"12.5"}})";
        const QVariantMap v = parseProbe(video, "/x/clip.mp4");
        QCOMPARE(v["kind"].toString(), "video");
        QCOMPARE(v["name"].toString(), "clip.mp4");
        QCOMPARE(v["width"].toInt(), 1080); // stood up
        QCOMPARE(v["height"].toInt(), 1920);
        QCOMPARE(qRound(v["fps"].toDouble() * 100), 2997);
        QCOMPARE(v["duration"].toDouble(), 12.5);
        QVERIFY(v["hasAudio"].toBool());

        const QVariantMap song = parseProbe(R"({"streams":[{"codec_type":"audio"},{"codec_type":"video","width":500,"height":500,
            "disposition":{"attached_pic":1}}],"format":{"format_name":"mp3","duration":"200"}})", "/s.mp3");
        QCOMPARE(song["kind"].toString(), "audio");
        const QVariantMap picture = parseProbe(R"({"streams":[{"codec_type":"video","width":200,"height":100}],
            "format":{"format_name":"png_pipe"}})", "/p.png");
        QCOMPARE(picture["kind"].toString(), "image");
        QVERIFY(parseProbe(R"({"streams":[],"format":{}})", "/t.txt").isEmpty());
    }

    // A real export: a split and a trimmed clip of a generated video, a text
    // with characters ffmpeg's filters are picky about.
    void realExport()
    {
        const QString ffmpeg = QStandardPaths::findExecutable("ffmpeg");
        const QString ffprobe = QStandardPaths::findExecutable("ffprobe");
        if (ffmpeg.isEmpty() || ffprobe.isEmpty()) {
            QSKIP("ffmpeg isn't installed");
        }
        QTemporaryDir dir;
        const QString source = dir.filePath("source.mp4");
        QVERIFY(run(ffmpeg, { "-v", "error", "-f", "lavfi", "-i", "testsrc=d=2:s=320x240:r=25", "-f", "lavfi", "-i", "sine=d=2",
            "-c:v", "libx264", "-c:a", "aac", "-shortest", source }));

        QByteArray json;
        QVERIFY(run(ffprobe, { "-v", "quiet", "-print_format", "json", "-show_format", "-show_streams", source }, &json));
        Timeline timeline;
        timeline.insertMedia(parseProbe(json, source));
        QVERIFY(timeline.split(Timeline::VideoTrack, 0, 1));
        timeline.trim(Timeline::VideoTrack, 1, false, 1.6); // 1 + 0.6
        timeline.set(Timeline::VideoTrack, 1, "speed", 0.5); // 0.6 s become 1.2
        timeline.addText(0.2, "It's: 50% [done]");
        timeline.set(Timeline::TextTrack, 0, "length", 1.0);
        QVERIFY(qAbs(timeline.length() - 2.2) < 0.001);

        ExportSettings settings;
        settings.font = defaultFont();
        settings.size = QSize(640, 360);
        settings.quality = "small";
        const QString output = dir.filePath("out put.mp4");
        QVERIFY(run(ffmpeg, exportArguments(timeline.tracks(), settings, output)));

        QVERIFY(run(ffprobe, { "-v", "quiet", "-print_format", "json", "-show_format", "-show_streams", output }, &json));
        const QJsonObject probe = QJsonDocument::fromJson(json).object();
        QVERIFY(qAbs(probe["format"].toObject()["duration"].toString().toDouble() - 2.2) < 0.1);
        const QJsonArray streams = probe["streams"].toArray();
        QCOMPARE(streams.size(), 2);
        QCOMPARE(streams[0].toObject()["codec_name"].toString(), "h264");
        QCOMPARE(streams[0].toObject()["width"].toInt(), 640);
        QCOMPARE(streams[0].toObject()["height"].toInt(), 360);
        QCOMPARE(streams[1].toObject()["codec_name"].toString(), "aac");
        QCOMPARE(streams[1].toObject()["channels"].toInt(), 2);
    }
};

QTEST_GUILESS_MAIN(ExportTest)
#include "export.moc"
