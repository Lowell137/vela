// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "mpvplayer.h"

#include <QGuiApplication>
#include <QOpenGLContext>
#include <QOpenGLFramebufferObject>
#include <QQuickOpenGLUtils>
#include <QQuickWindow>
#include <QStandardPaths>

#include <mpv/client.h>
#include <mpv/render_gl.h>

namespace {

// mpv's property, its name in `info` and its value before a file is loaded.
struct Followed {
    const char* property;
    const char* name;
    QVariant initial;
};
const Followed followed[] = {
    { "time-pos", "position", 0.0 },
    { "duration", "duration", 0.0 },
    { "pause", "paused", false },
    { "volume", "volume", 100.0 },
    { "mute", "muted", false },
    { "speed", "speed", 1.0 },
    { "media-title", "title", QString() },
    { "track-list", "tracks", QVariantList() },
    { "metadata", "metadata", QVariantMap() },
    { "idle-active", "idle", true },
};

QVariant fromNode(const mpv_node& node)
{
    switch (node.format) {
    case MPV_FORMAT_STRING:
        return QString::fromUtf8(node.u.string);
    case MPV_FORMAT_FLAG:
        return bool(node.u.flag);
    case MPV_FORMAT_INT64:
        return qlonglong(node.u.int64);
    case MPV_FORMAT_DOUBLE:
        return node.u.double_;
    case MPV_FORMAT_NODE_ARRAY: {
        QVariantList list;
        for (int i = 0; i < node.u.list->num; i++) {
            list.append(fromNode(node.u.list->values[i]));
        }
        return list;
    }
    case MPV_FORMAT_NODE_MAP: {
        QVariantMap map;
        for (int i = 0; i < node.u.list->num; i++) {
            map.insert(QString::fromUtf8(node.u.list->keys[i]), fromNode(node.u.list->values[i]));
        }
        return map;
    }
    default:
        return {};
    }
}

void* procAddress(void*, const char* name)
{
    QOpenGLContext* context = QOpenGLContext::currentContext();
    return context ? reinterpret_cast<void*>(context->getProcAddress(name)) : nullptr;
}

} // namespace

class MpvRenderer : public QQuickFramebufferObject::Renderer {
public:
    explicit MpvRenderer(MpvPlayer* player)
        : m_player(player)
    {
    }

    // Called with the OpenGL context current: where mpv's renderer is made.
    QOpenGLFramebufferObject* createFramebufferObject(const QSize& size) override
    {
        if (!m_player->m_gl) {
            mpv_opengl_init_params gl { procAddress, nullptr };
            QList<mpv_render_param> params {
                { MPV_RENDER_PARAM_API_TYPE, const_cast<char*>(MPV_RENDER_API_TYPE_OPENGL) },
                { MPV_RENDER_PARAM_OPENGL_INIT_PARAMS, &gl },
            };
            // The display, for decoding on the graphics card.
            if (auto* wayland = qGuiApp->nativeInterface<QNativeInterface::QWaylandApplication>()) {
                params.append({ MPV_RENDER_PARAM_WL_DISPLAY, wayland->display() });
            }
            params.append({ MPV_RENDER_PARAM_INVALID, nullptr });
            if (mpv_render_context_create(&m_player->m_gl, m_player->m_mpv, params.data()) < 0) {
                qFatal("vela-mediaplayer: mpv can't draw with OpenGL");
            }
            mpv_render_context_set_update_callback(m_player->m_gl, [](void* player) {
                QMetaObject::invokeMethod(static_cast<MpvPlayer*>(player), &MpvPlayer::update, Qt::QueuedConnection);
            }, m_player);
            QMetaObject::invokeMethod(m_player, [player = m_player] {
                player->m_ready = true;
                emit player->readyChanged();
            }, Qt::QueuedConnection);
        }
        return QQuickFramebufferObject::Renderer::createFramebufferObject(size);
    }

    void render() override
    {
        QOpenGLFramebufferObject* target = framebufferObject();
        mpv_opengl_fbo fbo { int(target->handle()), target->width(), target->height(), 0 };
        int flip = 0;
        mpv_render_param params[] = {
            { MPV_RENDER_PARAM_OPENGL_FBO, &fbo },
            { MPV_RENDER_PARAM_FLIP_Y, &flip },
            { MPV_RENDER_PARAM_INVALID, nullptr },
        };
        mpv_render_context_render(m_player->m_gl, params);
        QQuickOpenGLUtils::resetOpenGLState();
    }

private:
    MpvPlayer* m_player;
};

MpvPlayer::MpvPlayer(QQuickItem* parent)
    : QQuickFramebufferObject(parent)
{
    m_mpv = mpv_create();
    if (!m_mpv) {
        qFatal("vela-mediaplayer: mpv can't be started");
    }
    const QByteArray later = (QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
        + QStringLiteral("/watch-later")).toUtf8();
    const std::pair<const char*, const char*> options[] = {
        { "config", "no" }, // the Media Player's own settings, not ~/.config/mpv
        { "terminal", "no" },
        { "vo", "libmpv" },
        { "hwdec", "auto-safe" },
        { "idle", "yes" },
        { "input-default-bindings", "no" },
        { "osc", "no" },
        { "sub-auto", "fuzzy" }, // the .srt next to the video
        { "audio-client-name", "Media Player" },
        { "watch-later-dir", later.constData() },
        { "watch-later-options", "start,sid,aid" },
    };
    for (const auto& [name, value] : options) {
        mpv_set_option_string(m_mpv, name, value);
    }
    if (mpv_initialize(m_mpv) < 0) {
        qFatal("vela-mediaplayer: mpv can't be started");
    }

    for (size_t i = 0; i < std::size(followed); i++) {
        m_info->insert(QString::fromLatin1(followed[i].name), followed[i].initial);
        mpv_observe_property(m_mpv, i, followed[i].property, MPV_FORMAT_NODE);
    }
    mpv_set_wakeup_callback(m_mpv, [](void* player) {
        QMetaObject::invokeMethod(static_cast<MpvPlayer*>(player), &MpvPlayer::drain, Qt::QueuedConnection);
    }, this);
}

MpvPlayer::~MpvPlayer()
{
    if (m_gl) {
        mpv_render_context_free(m_gl);
    }
    mpv_terminate_destroy(m_mpv);
}

QQuickFramebufferObject::Renderer* MpvPlayer::createRenderer() const
{
    window()->setPersistentGraphics(true);
    window()->setPersistentSceneGraph(true);
    return new MpvRenderer(const_cast<MpvPlayer*>(this));
}

void MpvPlayer::load(const QUrl& url)
{
    command({ QStringLiteral("loadfile"), url.isLocalFile() ? url.toLocalFile() : url.toString() });
}

void MpvPlayer::set(const QString& name, const QString& value)
{
    mpv_set_property_string(m_mpv, name.toUtf8().constData(), value.toUtf8().constData());
}

void MpvPlayer::command(const QStringList& arguments)
{
    QList<QByteArray> bytes;
    QList<const char*> pointers;
    for (const QString& argument : arguments) {
        bytes.append(argument.toUtf8());
        pointers.append(bytes.last().constData());
    }
    pointers.append(nullptr);
    mpv_command(m_mpv, pointers.data());
}

void MpvPlayer::remember()
{
    const double at = m_info->value(QStringLiteral("position")).toDouble();
    const double length = m_info->value(QStringLiteral("duration")).toDouble();
    // A film, not a song, and not its first or last moments.
    if (length > 300 && at > 10 && at < length - 10) {
        command({ QStringLiteral("write-watch-later-config") });
    }
}

void MpvPlayer::drain()
{
    while (true) {
        const mpv_event* event = mpv_wait_event(m_mpv, 0);
        if (event->event_id == MPV_EVENT_NONE) {
            break;
        }
        if (event->event_id == MPV_EVENT_PROPERTY_CHANGE) {
            const auto* property = static_cast<mpv_event_property*>(event->data);
            const Followed& entry = followed[event->reply_userdata];
            m_info->insert(QString::fromLatin1(entry.name), property->format == MPV_FORMAT_NODE
                ? fromNode(*static_cast<mpv_node*>(property->data)) : entry.initial);
        } else if (event->event_id == MPV_EVENT_END_FILE) {
            const auto* end = static_cast<mpv_event_end_file*>(event->data);
            if (end->reason == MPV_END_FILE_REASON_EOF) {
                emit ended();
            } else if (end->reason == MPV_END_FILE_REASON_ERROR) {
                emit failed(QString::fromUtf8(mpv_error_string(end->error)));
            }
        }
    }
}
