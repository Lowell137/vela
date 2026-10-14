// SPDX-FileCopyrightText: 2026 Brando Giuffrida
// SPDX-License-Identifier: GPL-3.0-or-later

#include "snip.h"
#include "../../common/privatefile.h"

#include "clipboard.h"
#include "notifications.h"
#include "snipextras.h"
#include "windowcapture.h"

#include <QCoreApplication>
#include <LayerShellQt/Window>

#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLocalSocket>
#include <QProcess>
#include <QQmlEngine>
#include <QQuickWindow>
#include <QScreen>
#include <QSaveFile>
#include <QTemporaryFile>
#include <QStandardPaths>
#include <QTimer>
#include <QUrl>

namespace {

// The visible windows, asked of the compositor.
QVariantList queryWindows()
{
    const QString runtimeDir = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    const QString display = qEnvironmentVariable("WAYLAND_DISPLAY", QStringLiteral("wayland-0"));
    QLocalSocket socket;
    socket.connectToServer(runtimeDir + QStringLiteral("/vela-") + display + QStringLiteral(".sock"));
    if (!socket.waitForConnected(200)) {
        return {};
    }
    socket.write("window-rects\n");
    socket.waitForBytesWritten(200);
    QByteArray reply;
    while (!reply.contains('\n') && socket.waitForReadyRead(200)) {
        reply += socket.readAll();
    }
    return QJsonDocument::fromJson(reply.trimmed()).array().toVariantList();
}

} // namespace

Snip::Snip(QQmlEngine* engine, WindowCapture* capture, Clipboard* clipboard, NotificationServer* notifications,
    QObject* parent)
    : QObject(parent)
    , m_component(engine, "Vela.Shell", "SnipOverlay")
    , m_counterComponent(engine, "Vela.Shell", "SnipCountdown")
    , m_capture(capture)
    , m_clipboard(clipboard)
    , m_notifications(notifications)
{
    m_idle.setSingleShot(true);
    connect(&m_idle, &QTimer::timeout, this, [this] {
        if (m_worker && !m_pending.file) {
            m_worker->closeWriteChannel(); // vela-ocr --serve ends at the end of its input
        }
    });
    connect(m_capture, &WindowCapture::screenCaptured, this, [this](const QString&, bool) {
        if (m_waiting > 0 && --m_waiting == 0) {
            captured();
        }
    });
    m_countdown.setInterval(1000);
    connect(&m_countdown, &QTimer::timeout, this, &Snip::tickCountdown);
    // A click on the notification: the screenshot opens.
    connect(m_notifications, &NotificationServer::ActionInvoked, this, [this](uint id, const QString&) {
        const QString path = m_saved.take(id);
        if (!path.isEmpty()) {
            QDesktopServices::openUrl(QUrl::fromLocalFile(path));
        }
    });
}

void Snip::cycleDelay()
{
    m_delay = SnipExtras::nextDelay(m_delay);
    emit delayChanged();
}

// All the photos are in: the overlays, or (after a countdown) the snip itself.
void Snip::captured()
{
    if (!m_delayed.recapturing) {
        showOverlays();
        return;
    }
    m_delayed.recapturing = false;
    finishNow(m_delayed.screen, m_delayed.area.x(), m_delayed.area.y(), m_delayed.area.width(), m_delayed.area.height());
}

// The choice is made: count down (the overlays are gone, so menus can be
// opened), then take a new photo and cut the same area from it.
void Snip::tickCountdown()
{
    if (--m_remaining > 0) {
        if (m_counter) {
            m_counter->setProperty("seconds", m_remaining);
        }
        return;
    }
    m_countdown.stop();
    if (m_counter) { // out of the picture before the photo
        m_counter->hide();
        m_counter->deleteLater();
    }
    QTimer::singleShot(150, this, [this] {
        m_delayed.recapturing = true;
        m_capture->clearFull();
        m_waiting = m_capture->captureScreensFull();
        if (m_waiting == 0) {
            m_delayed.recapturing = false;
            cancel();
            return;
        }
        QTimer::singleShot(1500, this, [this] {
            if (m_waiting > 0) {
                m_waiting = 0;
                captured();
            }
        });
    });
}

QString Snip::colorAt(const QString& screenName, qreal x, qreal y) const
{
    const QImage full = m_capture->fullImage(screenName);
    QScreen* screen = nullptr;
    for (QScreen* candidate : QGuiApplication::screens()) {
        if (candidate->name() == screenName) {
            screen = candidate;
        }
    }
    if (full.isNull() || !screen) {
        return {};
    }
    const int px = qRound(x * full.width() / qreal(screen->geometry().width()));
    const int py = qRound(y * full.height() / qreal(screen->geometry().height()));
    if (!full.rect().contains(px, py)) {
        return {};
    }
    return SnipExtras::hexColor(full.pixelColor(px, py));
}

void Snip::pickColor(const QString& screenName, qreal x, qreal y)
{
    const QString hex = colorAt(screenName, x, y);
    if (hex.isEmpty()) {
        return;
    }
    m_clipboard->copyText(hex);
    m_notifications->Notify(QCoreApplication::translate("Snip", "Snipping Tool"), 0, QStringLiteral("color-management"),
        QCoreApplication::translate("Snip", "Color copied to clipboard"), hex, QStringList(), QVariantMap(), 4000);
    cancel();
}

void Snip::setMode(const QString& mode)
{
    if (mode != m_mode) {
        m_mode = mode;
        if (mode == QLatin1String("text")) {
            startWorker(); // warms up while the rectangle is chosen
        }
        emit modeChanged();
    }
}

QImage Snip::image(const QString& screen) const
{
    return m_capture->fullImage(screen);
}

void Snip::start()
{
    if (m_active) {
        return;
    }
    if (m_component.isError()) {
        qWarning() << "vela-shell: snipping tool" << m_component.errors();
        return;
    }
    m_active = true;
    m_mode = QStringLiteral("rect");
    m_windows = queryWindows();
    m_capture->clearFull();
    m_waiting = m_capture->captureScreensFull();
    emit modeChanged();
    emit activeChanged();
    if (m_waiting == 0) {
        cancel();
        return;
    }
    // If a capture doesn't come back, go on with those there are.
    QTimer::singleShot(1500, this, [this] {
        if (m_waiting > 0) {
            m_waiting = 0;
            captured();
        }
    });
}

void Snip::startText()
{
    start();
    setMode(QStringLiteral("text"));
}

void Snip::showOverlays()
{
    for (QScreen* screen : QGuiApplication::screens()) {
        if (m_capture->fullImage(screen->name()).isNull()) {
            continue;
        }
        QObject* object = m_component.create();
        auto* window = qobject_cast<QQuickWindow*>(object);
        if (!window) {
            qWarning() << "vela-shell: snipping tool" << m_component.errors();
            delete object;
            continue;
        }
        window->setProperty("screenName", screen->name());
        window->setScreen(screen);
        using LayerWindow = LayerShellQt::Window;
        LayerWindow* layer = LayerWindow::get(window);
        layer->setScreen(screen);
        layer->setScope(QStringLiteral("vela-snip"));
        layer->setLayer(LayerWindow::LayerOverlay);
        layer->setAnchors(LayerWindow::Anchors(LayerWindow::AnchorTop) | LayerWindow::AnchorBottom
            | LayerWindow::AnchorLeft | LayerWindow::AnchorRight);
        layer->setExclusiveZone(-1);
        layer->setKeyboardInteractivity(LayerWindow::KeyboardInteractivityOnDemand);
        window->show();
        m_overlays.append(window);
    }
    if (m_overlays.isEmpty()) {
        cancel();
    }
}

void Snip::closeOverlays()
{
    for (const QPointer<QQuickWindow>& window : std::as_const(m_overlays)) {
        if (window) {
            window->hide();
            window->deleteLater();
        }
    }
    m_overlays.clear();
}

void Snip::cancel()
{
    if (!m_active) {
        return;
    }
    closeOverlays();
    m_capture->clearFull();
    m_active = false;
    m_idle.start(30000); // the worker stays for a second snip
    emit activeChanged();
}

void Snip::finish(const QString& screenName, qreal x, qreal y, qreal width, qreal height)
{
    if (m_delay > 0 && width >= 1 && height >= 1) {
        m_delayed = { false, screenName, QRectF(x, y, width, height) };
        closeOverlays();
        m_capture->clearFull();
        m_remaining = m_delay;
        if (m_counterComponent.isReady()) {
            auto* window = qobject_cast<QQuickWindow*>(m_counterComponent.create());
            QScreen* screen = nullptr;
            for (QScreen* candidate : QGuiApplication::screens()) {
                if (candidate->name() == screenName) {
                    screen = candidate;
                }
            }
            if (window && screen) {
                window->setProperty("seconds", m_remaining);
                window->setScreen(screen);
                using LayerWindow = LayerShellQt::Window;
                LayerWindow* layer = LayerWindow::get(window);
                layer->setScreen(screen);
                layer->setScope(QStringLiteral("vela-snip-countdown"));
                layer->setLayer(LayerWindow::LayerOverlay);
                layer->setKeyboardInteractivity(LayerWindow::KeyboardInteractivityNone);
                window->show();
                m_counter = window;
            } else {
                delete window;
            }
        }
        m_countdown.start();
        return;
    }
    finishNow(screenName, x, y, width, height);
}

void Snip::finishNow(const QString& screenName, qreal x, qreal y, qreal width, qreal height)
{
    const QImage full = m_capture->fullImage(screenName);
    QScreen* screen = nullptr;
    for (QScreen* candidate : QGuiApplication::screens()) {
        if (candidate->name() == screenName) {
            screen = candidate;
        }
    }
    if (full.isNull() || !screen || width < 1 || height < 1) {
        cancel();
        return;
    }
    // From logical coordinates to image pixels (the output's scale).
    const qreal sx = full.width() / qreal(screen->geometry().width());
    const qreal sy = full.height() / qreal(screen->geometry().height());
    const QRect area = QRect(qRound(x * sx), qRound(y * sy), qRound(width * sx), qRound(height * sy))
                           .intersected(full.rect());
    const QImage cut = full.copy(area);
    closeOverlays();
    m_capture->clearFull();
    m_active = false;
    m_idle.start(30000);
    emit activeChanged();
    if (cut.isNull()) {
        return;
    }

    m_clipboard->copyImage(cut);
    // In Pictures/Screenshots, like Windows.
    const QString folder = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation) + QStringLiteral("/Screenshot");
    QDir().mkpath(folder);
    const QString path = folder + QStringLiteral("/Screenshot ")
        + QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HHmmss")) + QStringLiteral(".png");
    // Screenshots may contain private information. Save atomically and 0600.
    QSaveFile file(path);
    const bool saved = vela::openPrivateFile(file) && cut.save(&file, "PNG") && file.commit();
    const uint id = m_notifications->Notify(QCoreApplication::translate("Snip", "Snipping Tool"), 0,
        QStringLiteral("accessories-screenshot"), QCoreApplication::translate("Snip", "Screenshot copied to clipboard"),
        saved ? QCoreApplication::translate("Snip", "Saved to Pictures > Screenshot. Click to open it.")
              : QCoreApplication::translate("Snip", "Couldn't save it to Pictures > Screenshot."),
        saved ? QStringList { QStringLiteral("default"), QCoreApplication::translate("Snip", "Open") } : QStringList(),
        QVariantMap { { QStringLiteral("image-path"), path } }, 6000);
    if (saved) {
        m_saved.insert(id, path);
    }
}

// The area of output `screenName` (logical coordinates) in image pixels, and
// the pixels per logical unit.
static bool imageArea(const QImage& full, const QString& screenName, const QRectF& logical, QRect* area, QPointF* scale)
{
    QScreen* screen = nullptr;
    for (QScreen* candidate : QGuiApplication::screens()) {
        if (candidate->name() == screenName) {
            screen = candidate;
        }
    }
    if (full.isNull() || !screen || logical.width() < 1 || logical.height() < 1) {
        return false;
    }
    *scale = QPointF(full.width() / qreal(screen->geometry().width()), full.height() / qreal(screen->geometry().height()));
    *area = QRect(qRound(logical.x() * scale->x()), qRound(logical.y() * scale->y()),
        qRound(logical.width() * scale->x()), qRound(logical.height() * scale->y()))
                .intersected(full.rect());
    return !area->isEmpty();
}

// The Text mode, like Windows' Text Actions: the chosen rectangle stays on the
// screen and vela-ocr (a worker kept warm while the tool is open) finds its
// words; the overlay lets you select them and copy.
void Snip::startWorker()
{
    m_idle.stop();
    if (m_worker && m_worker->state() != QProcess::NotRunning) {
        return;
    }
    if (!m_worker) {
        m_worker = new QProcess(this);
        m_worker->setProcessChannelMode(QProcess::SeparateChannels);
        connect(m_worker, &QProcess::readyReadStandardOutput, this, [this] {
            while (m_worker->canReadLine()) {
                const QJsonObject reply = QJsonDocument::fromJson(m_worker->readLine()).object();
                if (m_pending.file) {
                    finishRecognize(reply);
                }
            }
        });
        connect(m_worker, &QProcess::finished, this, [this] {
            if (m_pending.file) {
                finishRecognize(QJsonObject { { QStringLiteral("error"), QStringLiteral("exit") } });
            }
        });
        connect(m_worker, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
            if (error == QProcess::FailedToStart && m_pending.file) {
                finishRecognize(QJsonObject { { QStringLiteral("error"), QStringLiteral("missing") } });
            }
        });
    }
    m_worker->start(QStringLiteral("vela-ocr"), { QStringLiteral("--serve") });
}

void Snip::recognize(const QString& screenName, qreal x, qreal y, qreal width, qreal height)
{
    const QImage full = m_capture->fullImage(screenName);
    QRect area;
    QPointF scale;
    if (m_pending.file || !imageArea(full, screenName, QRectF(x, y, width, height), &area, &scale)) {
        return;
    }
    auto* file = new QTemporaryFile(QDir::tempPath() + QStringLiteral("/vela-ocr-XXXXXX.png"), this);
    if (!file->open() || !full.copy(area).save(file, "PNG") || !file->flush()) {
        file->deleteLater();
        emit recognizeFailed(screenName, QCoreApplication::translate("Snip", "Couldn't read the text"));
        return;
    }
    m_pending = { file, screenName, QPointF(area.x() / scale.x(), area.y() / scale.y()), scale };
    startWorker();
    m_worker->write(file->fileName().toUtf8() + '\n');
}

void Snip::finishRecognize(const QJsonObject& reply)
{
    const Pending job = std::exchange(m_pending, Pending());
    job.file->deleteLater();
    if (reply.contains(QStringLiteral("error"))) {
        const bool missing = reply.value(QStringLiteral("error")).toString() == QLatin1String("missing");
        emit recognizeFailed(job.screen, missing ? QCoreApplication::translate("Snip", "Text recognition isn't installed (vela-ocr).")
                                                 : QCoreApplication::translate("Snip", "Couldn't read the text"));
        return;
    }
    // Pixels of the crop to this output's logical coordinates.
    QVariantList words;
    for (const QJsonValue& value : reply.value(QStringLiteral("words")).toArray()) {
        const QJsonObject word = value.toObject();
        words.append(QVariantMap {
            { QStringLiteral("text"), word.value(QStringLiteral("t")).toString() },
            { QStringLiteral("x"), job.origin.x() + word.value(QStringLiteral("x")).toDouble() / job.scale.x() },
            { QStringLiteral("y"), job.origin.y() + word.value(QStringLiteral("y")).toDouble() / job.scale.y() },
            { QStringLiteral("w"), word.value(QStringLiteral("w")).toDouble() / job.scale.x() },
            { QStringLiteral("h"), word.value(QStringLiteral("h")).toDouble() / job.scale.y() },
            { QStringLiteral("line"), word.value(QStringLiteral("l")).toInt() },
        });
    }
    emit wordsReady(job.screen, words);
}

void Snip::copyText(const QString& text)
{
    if (text.isEmpty()) {
        return;
    }
    m_clipboard->copyText(text);
    m_notifications->Notify(QCoreApplication::translate("Snip", "Snipping Tool"), 0, QStringLiteral("accessories-screenshot"),
        QCoreApplication::translate("Snip", "Text copied to clipboard"), text.left(200), QStringList(), QVariantMap(), 4000);
    cancel();
}
