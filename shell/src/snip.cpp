// SPDX-FileCopyrightText: 2026 Brando Giuffrida
// SPDX-License-Identifier: GPL-3.0-or-later

#include "snip.h"
#include "../../common/privatefile.h"

#include "clipboard.h"
#include "notifications.h"
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
    , m_capture(capture)
    , m_clipboard(clipboard)
    , m_notifications(notifications)
{
    connect(m_capture, &WindowCapture::screenCaptured, this, [this](const QString&, bool) {
        if (m_waiting > 0 && --m_waiting == 0) {
            showOverlays();
        }
    });
    // A click on the notification: the screenshot opens.
    connect(m_notifications, &NotificationServer::ActionInvoked, this, [this](uint id, const QString&) {
        const QString path = m_saved.take(id);
        if (!path.isEmpty()) {
            QDesktopServices::openUrl(QUrl::fromLocalFile(path));
        }
    });
}

void Snip::setMode(const QString& mode)
{
    if (mode != m_mode) {
        m_mode = mode;
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
            showOverlays();
        }
    });
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
    emit activeChanged();
}

void Snip::finish(const QString& screenName, qreal x, qreal y, qreal width, qreal height)
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
    emit activeChanged();
    if (cut.isNull()) {
        return;
    }

    if (m_mode == QLatin1String("text")) {
        recognizeText(cut);
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

// The Text mode (like PowerToys' Text Extractor): vela-ocr reads the text in
// the chosen rectangle, and it goes to the clipboard.
void Snip::recognizeText(const QImage& image)
{
    const auto report = [this](const QString& title, const QString& body) {
        m_notifications->Notify(QCoreApplication::translate("Snip", "Snipping Tool"), 0,
            QStringLiteral("accessories-screenshot"), title, body, QStringList(), QVariantMap(), 6000);
    };
    auto* file = new QTemporaryFile(QDir::tempPath() + QStringLiteral("/vela-ocr-XXXXXX.png"), this);
    if (!file->open() || !image.save(file, "PNG") || !file->flush()) {
        file->deleteLater();
        report(QCoreApplication::translate("Snip", "Couldn't read the text"), QString());
        return;
    }
    auto* process = new QProcess(this);
    process->setProcessChannelMode(QProcess::SeparateChannels);
    connect(process, &QProcess::errorOccurred, this, [=, this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            report(QCoreApplication::translate("Snip", "Couldn't read the text"),
                QCoreApplication::translate("Snip", "vela-ocr isn't installed."));
            file->deleteLater();
            process->deleteLater();
        }
    });
    connect(process, &QProcess::finished, this, [=, this](int code, QProcess::ExitStatus) {
        const QString text = QString::fromUtf8(process->readAllStandardOutput()).trimmed();
        if (code == 127) {
            report(QCoreApplication::translate("Snip", "Couldn't read the text"),
                QCoreApplication::translate("Snip", "Install tesseract or RapidOCR to read text."));
        } else if (text.isEmpty()) {
            report(QCoreApplication::translate("Snip", "No text found"), QString());
        } else {
            m_clipboard->copyText(text);
            report(QCoreApplication::translate("Snip", "Text copied to clipboard"), text.left(200));
        }
        file->deleteLater();
        process->deleteLater();
    });
    process->start(QStringLiteral("vela-ocr"), { file->fileName() });
}
