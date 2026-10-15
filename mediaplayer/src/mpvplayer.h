// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

// The picture and the sound: libmpv, drawn into a Qt Quick item
// (`MpvPlayer` in QML). mpv's own properties are followed in `info`
// (position, duration, paused, volume, muted, speed, title, tracks, metadata,
// idle) and
// changed by name with set() and command(), as on mpv's command line.

#include <QQmlPropertyMap>
#include <QQuickFramebufferObject>
#include <QStringList>
#include <QUrl>

struct mpv_handle;
struct mpv_render_context;

class MpvPlayer : public QQuickFramebufferObject {
    Q_OBJECT
    Q_PROPERTY(QQmlPropertyMap* info READ info CONSTANT)
    // mpv can draw: a video loaded before that would play without its picture.
    Q_PROPERTY(bool ready READ ready NOTIFY readyChanged)

public:
    explicit MpvPlayer(QQuickItem* parent = nullptr);
    ~MpvPlayer() override;

    Renderer* createRenderer() const override;
    QQmlPropertyMap* info() { return m_info; }
    bool ready() const { return m_ready; }

    Q_INVOKABLE void load(const QUrl& url);
    Q_INVOKABLE void set(const QString& name, const QString& value);
    Q_INVOKABLE void command(const QStringList& arguments);
    // Keeps where a long file was left, for the next time it's opened.
    Q_INVOKABLE void remember();

signals:
    void readyChanged();
    void ended(); // the file played to its end
    void failed(const QString& message);

private:
    friend class MpvRenderer;
    void drain();

    mpv_handle* m_mpv = nullptr;
    mpv_render_context* m_gl = nullptr;
    QQmlPropertyMap* m_info = QQmlPropertyMap::create(this);
    bool m_ready = false;
};
