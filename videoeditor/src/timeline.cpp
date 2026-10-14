// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "timeline.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>

#include <algorithm>
#include <cmath>

namespace {

constexpr double shortest = 0.1; // seconds a clip can be trimmed to
constexpr double pictureLength = 5;
constexpr double textLength = 3;

const char* const kinds[] = { "video", "audio", "image", "gap", "text" };

} // namespace

QString num(double value)
{
    QString text = QString::number(value, 'f', 3);
    while (text.endsWith(u'0')) {
        text.chop(1);
    }
    if (text.endsWith(u'.')) {
        text.chop(1);
    }
    return text == QLatin1String("-0") ? QStringLiteral("0") : text;
}

namespace {

// An EDL entry: its path with its length in bytes, so commas and
// semicolons in it are fine.
QString segment(const QString& path, double from, double length)
{
    return QStringLiteral("%%1%%2,%3,%4").arg(path.toUtf8().size()).arg(path, num(from), num(length));
}

// A still video stands for `length` seconds: as many pieces as it takes.
void still(QStringList& out, const QString& path, double length)
{
    while (length > 0.0005) {
        const double piece = std::min(length, Timeline::stillLength);
        out.append(segment(path, 0, piece));
        length -= piece;
    }
}

} // namespace

QList<Clip>& Timeline::track(int track)
{
    return track == AudioTrack ? m_tracks.audio : track == TextTrack ? m_tracks.texts : m_tracks.video;
}

double Timeline::start(int track, int index) const
{
    const QList<Clip>& clips = this->track(track);
    if (track != VideoTrack) {
        return clips.value(index).start;
    }
    double at = 0;
    for (int i = 0; i < index && i < clips.size(); i++) {
        at += clips[i].length();
    }
    return at;
}

double Timeline::length() const
{
    double end = start(VideoTrack, m_tracks.video.size());
    for (const QList<Clip>* clips : { &m_tracks.audio, &m_tracks.texts }) {
        for (const Clip& clip : *clips) {
            end = std::max(end, clip.start + clip.length());
        }
    }
    return end;
}

void Timeline::setMedia(const QStringList& media)
{
    if (m_media != media) {
        m_media = media;
        emit mediaChanged();
    }
}

QVariantList Timeline::clips(int track) const
{
    QVariantList list;
    const QList<Clip>& clips = this->track(track);
    for (int i = 0; i < clips.size(); i++) {
        const Clip& c = clips[i];
        list.append(QVariantMap {
            { "index", i },
            { "kind", QString::fromLatin1(kinds[c.kind]) },
            { "source", c.source },
            { "name", QFileInfo(c.source).fileName() },
            { "start", start(track, i) },
            { "length", c.length() },
            { "in", c.in },
            { "out", c.out },
            { "speed", c.speed },
            { "volume", c.volume },
            { "muted", c.muted },
            { "fadeIn", c.fadeIn },
            { "fadeOut", c.fadeOut },
            { "rotate", c.rotate },
            { "flipH", c.flipH },
            { "flipV", c.flipV },
            { "crop", c.crop },
            { "text", c.text },
            { "textSize", c.textSize },
            { "color", c.color },
            { "position", c.position },
            { "duration", c.duration },
            { "width", c.width },
            { "height", c.height },
            { "hasAudio", c.hasAudio },
        });
    }
    return list;
}

template <typename Edit>
void Timeline::edit(Edit change)
{
    const Tracks before = m_tracks;
    change();
    tidy();
    if (m_tracks == before) {
        return;
    }
    m_undo.append(before);
    m_redo.clear();
    if (m_saved > m_position) {
        m_saved = -1; // that state is gone with the redo steps
    }
    m_position++;
    emit changed();
}

void Timeline::tidy()
{
    for (QList<Clip>* clips : { &m_tracks.video, &m_tracks.audio, &m_tracks.texts }) {
        for (Clip& c : *clips) {
            if (!c.sourced()) {
                c.out -= c.in;
                c.in = 0;
                c.speed = 1;
            }
            const double length = c.length();
            c.fadeIn = std::clamp(c.fadeIn, 0.0, length);
            c.fadeOut = std::clamp(c.fadeOut, 0.0, length - c.fadeIn);
        }
    }
    // Neighbouring gaps are one; nothing ends with a gap.
    QList<Clip>& video = m_tracks.video;
    for (int i = video.size() - 1; i >= 0; i--) {
        if (video[i].kind != Clip::Gap) {
            continue;
        }
        if (i + 1 < video.size() && video[i + 1].kind == Clip::Gap) {
            video[i].out += video[i + 1].length();
            video.removeAt(i + 1);
        }
        if (i == video.size() - 1 || video[i].length() < 0.001) {
            video.removeAt(i);
        }
    }
    for (QList<Clip>* clips : { &m_tracks.audio, &m_tracks.texts }) {
        std::stable_sort(clips->begin(), clips->end(), [](const Clip& a, const Clip& b) { return a.start < b.start; });
    }
}

void Timeline::insertMedia(const QVariantMap& media, double at)
{
    Clip clip;
    const QString kind = media.value("kind").toString();
    clip.kind = kind == QLatin1String("audio") ? Clip::Audio : kind == QLatin1String("image") ? Clip::Image : Clip::Video;
    clip.source = media.value("path").toString();
    clip.duration = media.value("duration").toDouble();
    clip.out = clip.kind == Clip::Image ? pictureLength : clip.duration;
    clip.width = media.value("width").toInt();
    clip.height = media.value("height").toInt();
    clip.fps = media.value("fps").toDouble();
    clip.hasAudio = media.value("hasAudio").toBool();
    if (clip.length() <= 0) {
        return;
    }
    edit([&] {
        if (clip.kind == Clip::Audio) {
            QList<Clip>& audio = m_tracks.audio;
            clip.start = at >= 0 ? at : audio.isEmpty() ? 0 : audio.last().start + audio.last().length();
            audio.append(clip);
            return;
        }
        QList<Clip>& video = m_tracks.video;
        qsizetype index = video.size();
        if (at >= 0) {
            double best = at;
            double edge = 0;
            for (int i = 0; i <= video.size(); i++) {
                if (std::abs(edge - at) < best) {
                    best = std::abs(edge - at);
                    index = i;
                }
                if (i < video.size()) {
                    edge += video[i].length();
                }
            }
        }
        video.insert(index, clip);
    });
}

void Timeline::addText(double at, const QString& text)
{
    Clip clip;
    clip.kind = Clip::Text;
    clip.start = std::max(0.0, at);
    clip.out = textLength;
    clip.text = text;
    edit([&] { m_tracks.texts.append(clip); });
}

bool Timeline::split(int track, int index, double at)
{
    QList<Clip>& clips = this->track(track);
    if (index < 0 || index >= clips.size()) {
        return false;
    }
    const double from = start(track, index);
    const Clip& clip = clips[index];
    if (at < from + shortest / 2 || at > from + clip.length() - shortest / 2) {
        return false;
    }
    edit([&] {
        Clip left = clips[index];
        Clip right = left;
        left.out = right.in = left.in + (at - from) * left.speed;
        left.fadeOut = 0;
        right.fadeIn = 0;
        right.start = at;
        clips[index] = left;
        clips.insert(index + 1, right);
    });
    return true;
}

void Timeline::trim(int track, int index, bool head, double at)
{
    QList<Clip>& clips = this->track(track);
    if (index < 0 || index >= clips.size()) {
        return;
    }
    const double from = start(track, index);
    edit([&] {
        Clip& c = clips[index];
        if (head) {
            double in = c.in + (at - from) * c.speed;
            in = std::min(in, c.out - shortest * c.speed);
            if (c.sourced()) {
                in = std::max(in, 0.0);
            }
            if (track != VideoTrack) {
                in = std::max(in, c.in - c.start * c.speed); // not before the timeline's start
                c.start += (in - c.in) / c.speed;
            }
            c.in = in;
        } else {
            double out = std::max(c.in + (at - from) * c.speed, c.in + shortest * c.speed);
            if (c.sourced()) {
                out = std::min(out, std::max(c.duration, c.in + shortest * c.speed));
            }
            c.out = out;
        }
    });
}

int Timeline::move(int track, int index, double at)
{
    QList<Clip>& clips = this->track(track);
    if (index < 0 || index >= clips.size()) {
        return index;
    }
    if (track != VideoTrack) {
        Clip moved = clips[index];
        moved.start = std::max(0.0, at);
        edit([&] { clips[index] = moved; });
        return int(clips.indexOf(moved));
    }
    QList<Clip> rest = clips;
    const Clip clip = rest.takeAt(index);
    int target = 0;
    double best = std::abs(at);
    double edge = 0;
    for (int i = 0; i < rest.size(); i++) {
        edge += rest[i].length();
        if (std::abs(edge - at) < best) {
            best = std::abs(edge - at);
            target = i + 1;
        }
    }
    edit([&] { clips.move(index, target); });
    return target;
}

void Timeline::remove(int track, int index, bool ripple)
{
    QList<Clip>& clips = this->track(track);
    if (index < 0 || index >= clips.size()) {
        return;
    }
    edit([&] {
        const Clip clip = clips.takeAt(index);
        if (track == VideoTrack) {
            if (!ripple) {
                Clip gap;
                gap.kind = Clip::Gap;
                gap.out = clip.length();
                clips.insert(index, gap);
            }
        } else if (ripple) {
            for (Clip& c : clips) {
                if (c.start >= clip.start) {
                    c.start = std::max(0.0, c.start - clip.length());
                }
            }
        }
    });
}

void Timeline::set(int track, int index, const QString& key, const QVariant& value)
{
    QList<Clip>& clips = this->track(track);
    if (index < 0 || index >= clips.size()) {
        return;
    }
    edit([&] {
        Clip& c = clips[index];
        const double number = value.toDouble();
        if (key == QLatin1String("speed") && c.sourced()) {
            c.speed = std::clamp(number, 0.25, 4.0);
        } else if (key == QLatin1String("volume")) {
            c.volume = std::clamp(number, 0.0, 2.0);
        } else if (key == QLatin1String("muted")) {
            c.muted = value.toBool();
        } else if (key == QLatin1String("fadeIn")) {
            c.fadeIn = number;
        } else if (key == QLatin1String("fadeOut")) {
            c.fadeOut = number;
        } else if (key == QLatin1String("rotate")) {
            c.rotate = ((value.toInt() / 90 % 4 + 4) % 4) * 90;
        } else if (key == QLatin1String("flipH")) {
            c.flipH = value.toBool();
        } else if (key == QLatin1String("flipV")) {
            c.flipV = value.toBool();
        } else if (key == QLatin1String("crop")) {
            c.crop = value.toString();
        } else if (key == QLatin1String("text")) {
            c.text = value.toString();
        } else if (key == QLatin1String("textSize")) {
            c.textSize = std::clamp(value.toInt(), 8, 400);
        } else if (key == QLatin1String("color")) {
            c.color = value.toString();
        } else if (key == QLatin1String("position")) {
            c.position = value.toString();
        } else if (key == QLatin1String("length") && !c.sourced()) {
            c.out = c.in + std::max(shortest, number);
        }
    });
}

void Timeline::undo()
{
    if (m_undo.isEmpty()) {
        return;
    }
    m_redo.append(m_tracks);
    m_tracks = m_undo.takeLast();
    m_position--;
    emit changed();
}

void Timeline::redo()
{
    if (m_redo.isEmpty()) {
        return;
    }
    m_undo.append(m_tracks);
    m_tracks = m_redo.takeLast();
    m_position++;
    emit changed();
}

void Timeline::clear()
{
    m_tracks = {};
    m_undo.clear();
    m_redo.clear();
    m_position = m_saved = 0;
    setMedia({});
    emit changed();
}

QVariantMap Timeline::clipAt(double at) const
{
    double from = 0;
    const QList<Clip>& video = m_tracks.video;
    for (int i = 0; i < video.size(); i++) {
        const double length = video[i].length();
        if (at >= from && at < from + length) {
            return { { "index", i }, { "offset", at - from }, { "source", video[i].in + (at - from) * video[i].speed } };
        }
        from += length;
    }
    return { { "index", -1 }, { "offset", 0.0 }, { "source", 0.0 } };
}

double Timeline::snap(double at, double tolerance, int track, int index, double playhead) const
{
    double best = at;
    double distance = tolerance;
    auto consider = [&](double edge) {
        if (std::abs(edge - at) <= distance) {
            distance = std::abs(edge - at);
            best = edge;
        }
    };
    consider(0);
    consider(playhead);
    for (int t : { VideoTrack, AudioTrack, TextTrack }) {
        const QList<Clip>& clips = this->track(t);
        for (int i = 0; i < clips.size(); i++) {
            if (t == track && i == index) {
                continue;
            }
            const double from = start(t, i);
            consider(from);
            consider(from + clips[i].length());
        }
    }
    return best;
}

QString Timeline::stillPath(const QString& cache, const QString& source)
{
    if (source.isEmpty()) {
        return cache + QStringLiteral("/black.mp4");
    }
    return cache + u'/' + QString::fromLatin1(QCryptographicHash::hash(source.toUtf8(), QCryptographicHash::Sha1).toHex()) + QStringLiteral(".still.mp4");
}

QString Timeline::edl(const QString& cache) const
{
    QStringList parts;
    for (const Clip& c : m_tracks.video) {
        if (c.sourced()) {
            parts.append(segment(c.source, c.in, c.out - c.in));
        } else {
            still(parts, stillPath(cache, c.kind == Clip::Image ? c.source : QString()), c.length());
        }
    }
    return parts.isEmpty() ? QString() : QStringLiteral("edl://") + parts.join(u';');
}

QString Timeline::musicEdl(const QString& cache) const
{
    // shortcut: the audio track's clips play at their normal speed in the
    // preview, make a speed-aware EDL position mapping when it matters.
    QStringList parts;
    double at = 0;
    for (const Clip& c : m_tracks.audio) {
        if (c.start > at) {
            still(parts, stillPath(cache, QString()), c.start - at); // silence
        }
        const double from = c.in + std::max(0.0, at - c.start) * c.speed; // overlaps: the later one wins
        if (c.out > from) {
            parts.append(segment(c.source, from, c.out - from));
        }
        at = std::max(at, c.start + c.length());
    }
    return parts.isEmpty() ? QString() : QStringLiteral("edl://") + parts.join(u';');
}

double Timeline::toEdl(double at) const
{
    double from = 0;
    double position = 0;
    for (const Clip& c : m_tracks.video) {
        const double length = c.length();
        if (at < from + length) {
            return position + std::max(0.0, at - from) * c.speed;
        }
        from += length;
        position += c.out - c.in;
    }
    return position;
}

double Timeline::fromEdl(double position) const
{
    double from = 0;
    double edl = 0;
    for (const Clip& c : m_tracks.video) {
        const double source = c.out - c.in;
        if (position < edl + source) {
            return from + std::max(0.0, position - edl) / c.speed;
        }
        from += c.length();
        edl += source;
    }
    return from;
}

// --- the project file ---

QJsonObject Timeline::toJson(const QString& folder) const
{
    const QDir base(folder);
    auto path = [&](const QString& file) {
        return QJsonObject { { "source", base.relativeFilePath(file) }, { "absolute", file } };
    };
    auto list = [&](const QList<Clip>& clips) {
        QJsonArray array;
        for (const Clip& c : clips) {
            QJsonObject o {
                { "kind", kinds[c.kind] },
                { "in", c.in },
                { "out", c.out },
                { "start", c.start },
                { "speed", c.speed },
                { "volume", c.volume },
                { "muted", c.muted },
                { "fadeIn", c.fadeIn },
                { "fadeOut", c.fadeOut },
                { "rotate", c.rotate },
                { "flipH", c.flipH },
                { "flipV", c.flipV },
                { "crop", c.crop },
                { "text", c.text },
                { "textSize", c.textSize },
                { "color", c.color },
                { "position", c.position },
                { "duration", c.duration },
                { "width", c.width },
                { "height", c.height },
                { "fps", c.fps },
                { "hasAudio", c.hasAudio },
            };
            if (!c.source.isEmpty()) {
                const QJsonObject p = path(c.source);
                o.insert("source", p["source"]);
                o.insert("absolute", p["absolute"]);
            }
            array.append(o);
        }
        return array;
    };
    QJsonArray media;
    for (const QString& file : m_media) {
        media.append(path(file));
    }
    return {
        { "app", "vela-videoeditor" },
        { "version", 1 },
        { "media", media },
        { "video", list(m_tracks.video) },
        { "audio", list(m_tracks.audio) },
        { "texts", list(m_tracks.texts) },
    };
}

void Timeline::fromJson(const QJsonObject& json, const QString& folder)
{
    const QDir base(folder);
    // The relative path first: the project moved with its media.
    auto path = [&](const QJsonObject& o) {
        const QString relative = o["source"].toString();
        if (relative.isEmpty()) {
            return QString();
        }
        const QString near = QDir::cleanPath(base.absoluteFilePath(relative));
        const QString absolute = o["absolute"].toString();
        return QFileInfo::exists(near) || absolute.isEmpty() ? near : absolute;
    };
    auto list = [&](const QJsonArray& array) {
        QList<Clip> clips;
        for (const QJsonValue& value : array) {
            const QJsonObject o = value.toObject();
            Clip c;
            const QString kind = o["kind"].toString();
            for (int k = 0; k < int(std::size(kinds)); k++) {
                if (kind == QLatin1String(kinds[k])) {
                    c.kind = Clip::Kind(k);
                }
            }
            c.source = path(o);
            c.in = o["in"].toDouble();
            c.out = o["out"].toDouble();
            c.start = o["start"].toDouble();
            c.speed = std::clamp(o["speed"].toDouble(1), 0.25, 4.0);
            c.volume = o["volume"].toDouble(1);
            c.muted = o["muted"].toBool();
            c.fadeIn = o["fadeIn"].toDouble();
            c.fadeOut = o["fadeOut"].toDouble();
            c.rotate = o["rotate"].toInt();
            c.flipH = o["flipH"].toBool();
            c.flipV = o["flipV"].toBool();
            c.crop = o["crop"].toString();
            c.text = o["text"].toString();
            c.textSize = o["textSize"].toInt(c.textSize);
            c.color = o["color"].toString(c.color);
            c.position = o["position"].toString(c.position);
            c.duration = o["duration"].toDouble();
            c.width = o["width"].toInt();
            c.height = o["height"].toInt();
            c.fps = o["fps"].toDouble();
            c.hasAudio = o["hasAudio"].toBool();
            if (c.out > c.in) {
                clips.append(c);
            }
        }
        return clips;
    };
    m_tracks.video = list(json["video"].toArray());
    m_tracks.audio = list(json["audio"].toArray());
    m_tracks.texts = list(json["texts"].toArray());
    tidy();
    QStringList media;
    for (const QJsonValue& value : json["media"].toArray()) {
        media.append(path(value.toObject()));
    }
    setMedia(media);
}

QString Timeline::saveCopy(const QString& file) const
{
    QSaveFile out(file);
    if (!out.open(QIODevice::WriteOnly)) {
        return out.errorString();
    }
    out.write(QJsonDocument(toJson(QFileInfo(file).absolutePath())).toJson());
    return out.commit() ? QString() : out.errorString();
}

QString Timeline::save(const QString& file)
{
    const QString error = saveCopy(file);
    if (error.isEmpty() && m_saved != m_position) {
        m_saved = m_position;
        emit changed();
    }
    return error;
}

QString Timeline::load(const QString& file)
{
    QFile in(file);
    if (!in.open(QIODevice::ReadOnly)) {
        return in.errorString();
    }
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(in.readAll(), &error);
    if (error.error != QJsonParseError::NoError) {
        return error.errorString();
    }
    if (document.object()["app"].toString() != QLatin1String("vela-videoeditor")) {
        return tr("It isn't a Video Editor project.");
    }
    clear();
    fromJson(document.object(), QFileInfo(file).absolutePath());
    emit changed();
    return {};
}

QString Timeline::restore(const QString& autosave)
{
    const QString error = load(autosave);
    if (error.isEmpty()) {
        m_saved = -1;
        emit changed();
    }
    return error;
}

QStringList Timeline::missing() const
{
    QStringList files;
    auto check = [&](const QString& file) {
        if (!file.isEmpty() && !files.contains(file) && !QFileInfo::exists(file)) {
            files.append(file);
        }
    };
    for (const QList<Clip>* clips : { &m_tracks.video, &m_tracks.audio }) {
        for (const Clip& c : *clips) {
            check(c.source);
        }
    }
    for (const QString& file : m_media) {
        check(file);
    }
    return files;
}

void Timeline::relink(const QString& from, const QString& to)
{
    QStringList media = m_media;
    for (QString& file : media) {
        if (file == from) {
            file = to;
        }
    }
    setMedia(media);
    edit([&] {
        for (QList<Clip>* clips : { &m_tracks.video, &m_tracks.audio }) {
            for (Clip& c : *clips) {
                if (c.source == from) {
                    c.source = to;
                }
            }
        }
    });
}
