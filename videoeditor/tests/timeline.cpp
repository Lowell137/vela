// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

// The timeline's edits, its undo and redo, the preview's EDL and the
// project file.

#include "timeline.h"

#include <QFile>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <QtTest>

namespace {

QVariantMap video(const QString& path, double duration, bool audio = true)
{
    return { { "path", path }, { "kind", "video" }, { "duration", duration }, { "width", 1920 }, { "height", 1080 },
        { "fps", 30.0 }, { "hasAudio", audio } };
}
QVariantMap sound(const QString& path, double duration)
{
    return { { "path", path }, { "kind", "audio" }, { "duration", duration }, { "hasAudio", true } };
}
QVariantMap picture(const QString& path)
{
    return { { "path", path }, { "kind", "image" }, { "width", 800 }, { "height", 600 } };
}

} // namespace

class TimelineTest : public QObject {
    Q_OBJECT

private slots:
    void insertsAtTheNearestBoundary()
    {
        Timeline t;
        t.insertMedia(video("/a.mp4", 10));
        t.insertMedia(video("/b.mp4", 4));
        QCOMPARE(t.length(), 14.0);
        t.insertMedia(picture("/c.png"), 9); // nearer 10 than 0
        QCOMPARE(t.tracks().video.size(), 3);
        QCOMPARE(t.tracks().video[1].source, "/c.png");
        QCOMPARE(t.tracks().video[1].length(), 5.0); // a picture's default
        QCOMPARE(t.start(Timeline::VideoTrack, 2), 15.0);
        QCOMPARE(t.length(), 19.0);

        t.insertMedia(sound("/m.mp3", 30), 2);
        t.insertMedia(sound("/n.mp3", 3)); // after the last one
        QCOMPARE(t.tracks().audio.size(), 2);
        QCOMPARE(t.start(Timeline::AudioTrack, 1), 32.0);
        QCOMPARE(t.length(), 35.0); // the longest track
    }

    void splitsAtTheTime()
    {
        Timeline t;
        t.insertMedia(video("/a.mp4", 10));
        t.set(Timeline::VideoTrack, 0, "fadeIn", 1.0);
        t.set(Timeline::VideoTrack, 0, "fadeOut", 2.0);
        QVERIFY(!t.split(Timeline::VideoTrack, 0, 0)); // at an edge: nothing
        QVERIFY(t.split(Timeline::VideoTrack, 0, 4));
        const auto& v = t.tracks().video;
        QCOMPARE(v.size(), 2);
        QCOMPARE(v[0].out, 4.0);
        QCOMPARE(v[1].in, 4.0);
        QCOMPARE(v[1].out, 10.0);
        QCOMPARE(v[0].fadeIn, 1.0);
        QCOMPARE(v[0].fadeOut, 0.0);
        QCOMPARE(v[1].fadeIn, 0.0);
        QCOMPARE(v[1].fadeOut, 2.0);
        QCOMPARE(t.length(), 10.0);

        // At double speed a timeline second is two of the source.
        t.set(Timeline::VideoTrack, 1, "speed", 2.0);
        QCOMPARE(t.length(), 7.0);
        QVERIFY(t.split(Timeline::VideoTrack, 1, 5));
        QCOMPARE(t.tracks().video[1].out, 6.0);
        QCOMPARE(t.tracks().video[2].in, 6.0);

        // A text splits around its own start.
        t.addText(2, "Hello");
        QVERIFY(t.split(Timeline::TextTrack, 0, 3));
        QCOMPARE(t.tracks().texts[1].start, 3.0);
        QCOMPARE(t.tracks().texts[1].length(), 2.0);
        QCOMPARE(t.tracks().texts[1].text, "Hello");
    }

    void trimsWithinTheSource()
    {
        Timeline t;
        t.insertMedia(video("/a.mp4", 10));
        t.insertMedia(video("/b.mp4", 10));
        t.trim(Timeline::VideoTrack, 0, true, 2); // the head: the source starts 2 s later
        QCOMPARE(t.tracks().video[0].in, 2.0);
        QCOMPARE(t.start(Timeline::VideoTrack, 1), 8.0); // the next one follows
        t.trim(Timeline::VideoTrack, 0, true, -5); // can't start before the source
        QCOMPARE(t.tracks().video[0].in, 0.0);
        t.trim(Timeline::VideoTrack, 0, false, 30); // nor end after it
        QCOMPARE(t.tracks().video[0].out, 10.0);
        t.trim(Timeline::VideoTrack, 0, false, 6);
        QCOMPARE(t.tracks().video[0].out, 6.0);
        t.trim(Timeline::VideoTrack, 0, false, -1); // never shorter than a tenth
        QVERIFY(qFuzzyCompare(t.tracks().video[0].length(), 0.1));

        // A picture has no end; a sound's head moves its start.
        t.insertMedia(picture("/c.png"));
        t.trim(Timeline::VideoTrack, 2, false, t.start(Timeline::VideoTrack, 2) + 12);
        QCOMPARE(t.tracks().video[2].length(), 12.0);
        t.insertMedia(sound("/m.mp3", 30), 4);
        t.trim(Timeline::AudioTrack, 0, true, 6);
        QCOMPARE(t.tracks().audio[0].start, 6.0);
        QCOMPARE(t.tracks().audio[0].in, 2.0);
    }

    void movesClips()
    {
        Timeline t;
        t.insertMedia(video("/a.mp4", 2));
        t.insertMedia(video("/b.mp4", 3));
        t.insertMedia(video("/c.mp4", 4));
        QCOMPARE(t.move(Timeline::VideoTrack, 0, 7), 2); // to the end
        QCOMPARE(t.tracks().video[0].source, "/b.mp4");
        QCOMPARE(t.tracks().video[2].source, "/a.mp4");
        QCOMPARE(t.move(Timeline::VideoTrack, 2, 0.4), 0);
        QCOMPARE(t.tracks().video[0].source, "/a.mp4");
        t.insertMedia(sound("/m.mp3", 5), 1);
        t.move(Timeline::AudioTrack, 0, -3); // not before the start
        QCOMPARE(t.tracks().audio[0].start, 0.0);
        t.move(Timeline::AudioTrack, 0, 2.5);
        QCOMPARE(t.tracks().audio[0].start, 2.5);
    }

    void removesWithAndWithoutRipple()
    {
        Timeline t;
        t.insertMedia(video("/a.mp4", 2));
        t.insertMedia(video("/b.mp4", 3));
        t.insertMedia(video("/c.mp4", 4));
        t.remove(Timeline::VideoTrack, 1, false); // a gap stays
        QCOMPARE(t.tracks().video[1].kind, Clip::Gap);
        QCOMPARE(t.length(), 9.0);
        QCOMPARE(t.start(Timeline::VideoTrack, 2), 5.0);
        t.remove(Timeline::VideoTrack, 0, false); // two gaps become one
        QCOMPARE(t.tracks().video.size(), 2);
        QCOMPARE(t.tracks().video[0].length(), 5.0);
        t.remove(Timeline::VideoTrack, 0, true);
        QCOMPARE(t.length(), 4.0);
        t.remove(Timeline::VideoTrack, 0, false); // a gap at the end is no gap
        QVERIFY(t.tracks().video.isEmpty());

        t.addText(1, "a");
        t.addText(5, "b");
        t.remove(Timeline::TextTrack, 0, true);
        QCOMPARE(t.tracks().texts[0].start, 2.0);
    }

    void detachesTheSound()
    {
        Timeline t;
        t.insertMedia(video("/a.mp4", 10));
        t.insertMedia(video("/b.mp4", 10));
        t.trim(Timeline::VideoTrack, 1, true, 13);
        t.set(Timeline::VideoTrack, 1, "volume", 0.5);
        t.detachAudio(1);
        QVERIFY(t.tracks().video[1].muted);
        const Clip& sound = t.tracks().audio.at(0);
        QCOMPARE(sound.kind, Clip::Audio);
        QCOMPARE(sound.source, "/b.mp4");
        QCOMPARE(sound.start, 10.0);
        QCOMPARE(sound.in, 3.0);
        QCOMPARE(sound.volume, 0.5);
        t.detachAudio(1); // already muted: nothing more
        QCOMPARE(t.tracks().audio.size(), 1);
        t.undo();
        QVERIFY(t.tracks().audio.isEmpty());
        QVERIFY(!t.tracks().video[1].muted);
    }

    void undoesAndRedoes()
    {
        Timeline t;
        QVERIFY(!t.canUndo());
        t.insertMedia(video("/a.mp4", 10));
        const Tracks one = t.tracks();
        QVERIFY(t.split(Timeline::VideoTrack, 0, 5));
        const Tracks two = t.tracks();
        t.remove(Timeline::VideoTrack, 0, true);
        QCOMPARE(t.length(), 5.0);
        t.undo();
        QVERIFY(t.tracks() == two);
        t.undo();
        QVERIFY(t.tracks() == one);
        QVERIFY(t.canRedo());
        t.redo();
        QVERIFY(t.tracks() == two);
        t.set(Timeline::VideoTrack, 0, "volume", 1.5); // a new edit forgets the redo
        QVERIFY(!t.canRedo());
        t.set(Timeline::VideoTrack, 0, "volume", 1.5); // no change, no step
        t.undo();
        QCOMPARE(t.tracks().video[0].volume, 1.0);
        t.set(Timeline::VideoTrack, 0, "speed", 9.0); // within 0.25..4
        QCOMPARE(t.tracks().video[0].speed, 4.0);
    }

    void modifiedFollowsTheSavedState()
    {
        QTemporaryDir dir;
        Timeline t;
        QVERIFY(!t.modified());
        t.insertMedia(video("/a.mp4", 10));
        QVERIFY(t.modified());
        QCOMPARE(t.save(dir.filePath("p.velaedit")), QString());
        QVERIFY(!t.modified());
        t.addText(0, "x");
        QVERIFY(t.modified());
        t.undo();
        QVERIFY(!t.modified());
        t.undo();
        QVERIFY(t.modified());
        t.addText(0, "y"); // the saved state can't come back by redoing
        t.undo();
        QVERIFY(t.modified());
    }

    void mapsTimeToClips()
    {
        Timeline t;
        t.insertMedia(video("/a.mp4", 10));
        t.insertMedia(video("/b.mp4", 10));
        t.trim(Timeline::VideoTrack, 0, true, 4); // a: 4..10
        t.set(Timeline::VideoTrack, 1, "speed", 2.0); // b: 6 s to 5
        QCOMPARE(t.length(), 11.0);
        QVariantMap at = t.clipAt(8);
        QCOMPARE(at["index"].toInt(), 1);
        QCOMPARE(at["offset"].toDouble(), 2.0);
        QCOMPARE(at["source"].toDouble(), 4.0);
        QCOMPARE(t.clipAt(3)["source"].toDouble(), 7.0);
        QCOMPARE(t.clipAt(11)["index"].toInt(), -1);

        // The EDL plays the source's seconds.
        QCOMPARE(t.toEdl(8), 10.0);
        QCOMPARE(t.fromEdl(10), 8.0);
        QCOMPARE(t.toEdl(3), 3.0);
        QCOMPARE(t.fromEdl(16), 11.0);
    }

    void snapsToEdges()
    {
        Timeline t;
        t.insertMedia(video("/a.mp4", 10));
        t.insertMedia(video("/b.mp4", 10));
        t.addText(3, "t");
        QCOMPARE(t.snap(9.8, 0.3, Timeline::TextTrack, 0, 50), 10.0);
        QCOMPARE(t.snap(9.5, 0.3, Timeline::TextTrack, 0, 50), 9.5);
        QCOMPARE(t.snap(14.9, 0.3, Timeline::TextTrack, 0, 15), 15.0); // the playhead
        QCOMPARE(t.snap(3.1, 0.3, Timeline::TextTrack, 0, 50), 3.1); // not its own edge
        QCOMPARE(t.snap(3.1, 0.3, Timeline::VideoTrack, 0, 50), 3.0);
    }

    void writesTheEdl()
    {
        Timeline t;
        QCOMPARE(t.edl("/c"), QString());
        t.insertMedia(video("/v/a,b.mp4", 10));
        t.trim(Timeline::VideoTrack, 0, true, 1.5);
        t.insertMedia(picture("/p.png"));
        t.trim(Timeline::VideoTrack, 1, false, 8.5 + 70); // longer than a still video
        t.insertMedia(video("/b.mp4", 10));
        t.remove(Timeline::VideoTrack, 1, false);
        const QString still = Timeline::stillPath("/c", "");
        const QString black = "%" + QString::number(still.toUtf8().size()) + "%" + still;
        QCOMPARE(t.edl("/c"), "edl://%10%/v/a,b.mp4,1.5,8.5;" + black + ",0,60;" + black + ",0,10;%6%/b.mp4,0,10");
        QVERIFY(Timeline::stillPath("/c", "/p.png").startsWith("/c/"));
        QVERIFY(Timeline::stillPath("/c", "/p.png") != still);

        t.insertMedia(sound("/m.mp3", 30), 2);
        t.trim(Timeline::AudioTrack, 0, false, 5);
        QCOMPARE(t.musicEdl("/c"), "edl://" + black + ",0,2;%6%/m.mp3,0,3");
    }

    void projectFileRoundTrip()
    {
        QTemporaryDir dir;
        const QString media = dir.filePath("media/a.mp4");
        QDir().mkpath(dir.filePath("media"));
        QVERIFY(QFile(media).open(QIODevice::WriteOnly));

        Timeline t;
        t.setMedia({ media, "/gone/b.mp3" });
        t.insertMedia(video(media, 10));
        t.set(Timeline::VideoTrack, 0, "rotate", 90);
        t.set(Timeline::VideoTrack, 0, "crop", "9:16");
        t.insertMedia(sound("/gone/b.mp3", 20), 1);
        t.addText(2, "Hi: it's \"me\"");
        t.set(Timeline::TextTrack, 0, "color", "#ff0000");
        const QString file = dir.filePath("project/p.velaedit");
        QDir().mkpath(dir.filePath("project"));
        QCOMPARE(t.save(file), QString());

        QFile saved(file);
        QVERIFY(saved.open(QIODevice::ReadOnly));
        const QJsonObject json = QJsonDocument::fromJson(saved.readAll()).object();
        QCOMPARE(json["video"].toArray()[0].toObject()["source"].toString(), "../media/a.mp4");

        Timeline back;
        QCOMPARE(back.load(file), QString());
        QVERIFY(back.tracks() == t.tracks());
        QCOMPARE(back.media(), t.media());
        QVERIFY(!back.modified());
        QVERIFY(!back.canUndo());
        QCOMPARE(back.missing(), QStringList { "/gone/b.mp3" });

        back.relink("/gone/b.mp3", media);
        QCOMPARE(back.tracks().audio[0].source, media);
        QCOMPARE(back.media(), (QStringList { media, media }));
        QVERIFY(back.missing().isEmpty());

        // Moved together with the project: found by the relative path.
        QTemporaryDir other;
        QVERIFY(QDir().rename(dir.filePath("media"), other.filePath("media")));
        QVERIFY(QDir().rename(dir.filePath("project"), other.filePath("project")));
        Timeline moved;
        QCOMPARE(moved.load(other.filePath("project/p.velaedit")), QString());
        QCOMPARE(moved.tracks().video[0].source, other.filePath("media/a.mp4"));
        QCOMPARE(moved.missing(), QStringList { "/gone/b.mp3" });

        QCOMPARE(back.load(dir.filePath("nothing.velaedit")).isEmpty(), false);
    }
};

QTEST_GUILESS_MAIN(TimelineTest)
#include "timeline.moc"
