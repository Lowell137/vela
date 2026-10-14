// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

// vela-videoeditor: the Windows 11 video editor's basics, on mpv and ffmpeg.
//
// How it fits together:
// - Timeline (this file, pure Qt Core, tested): the project. Three tracks:
//   the video track is a sequence (a clip starts where the previous one
//   ends; a hole is a Gap clip), the audio track (music, voice-over) and the
//   text track hold clips at a free `start`. Every edit goes through one
//   function that snapshots the tracks first: undo and redo walk those
//   snapshots. It also writes the project file (.velaedit, JSON) and the
//   preview's EDL.
// - exportcommand.h (pure, tested): the ffmpeg arguments and the one
//   filter_complex that renders the timeline (trim, speed, effects, concat,
//   text, music mix).
// - mediapool.h (app): files added to the project, probed with ffprobe, with
//   a thumbnail and, for pictures, a still video the preview can play.
// - exporter.h (app): runs ffmpeg and follows its -progress output.
// - QML: the preview is mpv (mediaplayer/src/mpvplayer.h) playing the video
//   track's EDL; per-clip speed, volume, rotation and crop are mpv properties
//   set when the playhead enters a clip; flips, fades and texts are drawn by
//   QML over it; a second, hidden mpv plays the audio track in step.

#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QStringList>
#include <QVariantList>

QString num(double value); // seconds for mpv and ffmpeg: up to 3 decimals, no trailing zeros

struct Clip {
    enum Kind { Video, Audio, Image, Gap, Text };
    Kind kind = Video;
    QString source; // absolute path; none for gaps and texts
    double in = 0; // the source's seconds; images, gaps and texts: 0..length
    double out = 0;
    double start = 0; // on the audio and text tracks only
    double speed = 1;
    double volume = 1; // 0..2
    bool muted = false;
    double fadeIn = 0;
    double fadeOut = 0;
    int rotate = 0; // 0, 90, 180, 270 clockwise
    bool flipH = false;
    bool flipV = false;
    QString crop; // "", "16:9", "9:16", "1:1", "4:3"
    QString text;
    int textSize = 72; // pixels on a 1080 lines high picture
    QString color = QStringLiteral("#ffffff");
    QString position = QStringLiteral("bottom"); // center, bottom (third), top
    // What ffprobe said about the source.
    double duration = 0;
    int width = 0;
    int height = 0;
    double fps = 0;
    bool hasAudio = false;

    double length() const { return (out - in) / speed; }
    bool sourced() const { return kind == Video || kind == Audio; } // bounded by the file's length
    bool operator==(const Clip&) const = default;
};

struct Tracks {
    QList<Clip> video;
    QList<Clip> audio;
    QList<Clip> texts;
    bool operator==(const Tracks&) const = default;
};

class Timeline : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList video READ videoClips NOTIFY changed)
    Q_PROPERTY(QVariantList audio READ audioClips NOTIFY changed)
    Q_PROPERTY(QVariantList texts READ textClips NOTIFY changed)
    Q_PROPERTY(double length READ length NOTIFY changed)
    Q_PROPERTY(double videoLength READ videoLength NOTIFY changed) // what's exported
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY changed)
    Q_PROPERTY(bool canRedo READ canRedo NOTIFY changed)
    Q_PROPERTY(bool modified READ modified NOTIFY changed)
    Q_PROPERTY(QStringList media READ media WRITE setMedia NOTIFY mediaChanged) // the media pool's files

public:
    enum Track { VideoTrack, AudioTrack, TextTrack };
    Q_ENUM(Track)

    using QObject::QObject;

    const Tracks& tracks() const { return m_tracks; }
    QList<Clip>& track(int track);
    const QList<Clip>& track(int track) const { return const_cast<Timeline*>(this)->track(track); }
    double start(int track, int index) const; // on the timeline
    double length() const;
    double videoLength() const { return start(VideoTrack, int(m_tracks.video.size())); }
    bool canUndo() const { return !m_undo.isEmpty(); }
    bool canRedo() const { return !m_redo.isEmpty(); }
    bool modified() const { return m_position != m_saved; }
    QStringList media() const { return m_media; }
    void setMedia(const QStringList& media);

    QVariantList videoClips() const { return clips(VideoTrack); }
    QVariantList audioClips() const { return clips(AudioTrack); }
    QVariantList textClips() const { return clips(TextTrack); }
    Q_INVOKABLE QVariantList clips(int track) const;

    // `media`: a media pool item (path, kind, duration, width, height, fps,
    // hasAudio). Pictures and videos go to the video track before the clip
    // boundary nearest `at` (at the end if `at` < 0), sounds to the audio
    // track at `at` (after its last clip if < 0).
    Q_INVOKABLE void insertMedia(const QVariantMap& media, double at = -1);
    Q_INVOKABLE void addText(double at, const QString& text);
    Q_INVOKABLE bool split(int track, int index, double at);
    // Moves the clip's head or tail edge to `at` (timeline seconds), within
    // its source.
    Q_INVOKABLE void trim(int track, int index, bool head, double at);
    // The video track reorders (the clip goes to the boundary nearest `at`);
    // the others put the clip's start at `at`. Returns the clip's new index.
    Q_INVOKABLE int move(int track, int index, double at);
    // Ripple closes the hole; otherwise a video clip leaves a gap.
    Q_INVOKABLE void remove(int track, int index, bool ripple);
    Q_INVOKABLE void set(int track, int index, const QString& key, const QVariant& value);
    // A video clip's sound becomes a clip of its own on the audio track.
    Q_INVOKABLE void detachAudio(int index);
    Q_INVOKABLE void undo();
    Q_INVOKABLE void redo();
    void clear();

    // The video clip at timeline second `at`: {index, offset (in the clip)},
    // index -1 past the end.
    Q_INVOKABLE QVariantMap clipAt(double at) const;
    // `at` pulled to the nearest clip edge (of any track, not the clip
    // `track`/`index` itself), the playhead or 0 when within `tolerance`.
    Q_INVOKABLE double snap(double at, double tolerance, int track, int index, double playhead) const;

    // The preview: mpv's EDL of the video track (and of the audio track),
    // pictures and gaps played from still videos made in `cache`
    // (stillPath()), and the mapping between the EDL's and the timeline's
    // seconds (they differ where the speed isn't 1).
    Q_INVOKABLE QString edl(const QString& cache) const;
    Q_INVOKABLE QString musicEdl(const QString& cache) const;
    Q_INVOKABLE double toEdl(double at) const;
    Q_INVOKABLE double fromEdl(double position) const;
    static QString stillPath(const QString& cache, const QString& source); // a picture's; "" source: black
    static constexpr double stillLength = 60; // seconds in a still video

    // The project file. Paths are kept relative to `file`'s folder and
    // absolute; load() returns an error ("" when fine).
    Q_INVOKABLE QString save(const QString& file);
    Q_INVOKABLE QString load(const QString& file);
    Q_INVOKABLE QString saveCopy(const QString& file) const; // autosave: not marked saved
    Q_INVOKABLE QString restore(const QString& autosave); // loaded, but still to be saved
    QJsonObject toJson(const QString& folder) const;
    void fromJson(const QJsonObject& json, const QString& folder);
    Q_INVOKABLE QStringList missing() const; // sources that aren't there
    Q_INVOKABLE void relink(const QString& from, const QString& to);

signals:
    void changed();
    void mediaChanged();

private:
    // Every edit: snapshot, change, tidy, notify.
    template <typename Edit> void edit(Edit change);
    void tidy();

    Tracks m_tracks;
    QList<Tracks> m_undo; // the states before each edit
    QList<Tracks> m_redo;
    int m_position = 0; // edits applied; where it was saved
    int m_saved = 0;
    QStringList m_media;
};
