// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "portalprint.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusUnixFileDescriptor>
#include <QFile>
#include <QRandomGenerator>

namespace {

QDBusMessage call(const char* method)
{
    return QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.portal.Desktop"),
        QStringLiteral("/org/freedesktop/portal/desktop"), QStringLiteral("org.freedesktop.portal.Print"),
        QLatin1String(method));
}

} // namespace

// shortcut: the dialog isn't tied to the app's window (the portal wants a
// Wayland handle exported for that); export one if it opens behind.
void PortalPrint::print(const QString& path, const QString& title)
{
    auto* job = new PortalPrint;
    job->m_path = path;
    job->m_title = title;

    QDBusConnection bus = QDBusConnection::sessionBus();
    // The answer comes as a signal from an object the portal names after
    // us and this token: listening first, it can't be missed.
    const QString token = QStringLiteral("vela%1").arg(QRandomGenerator::global()->generate());
    const QString sender = bus.baseService().mid(1).replace(QLatin1Char('.'), QLatin1Char('_'));
    bus.connect(QStringLiteral("org.freedesktop.portal.Desktop"),
        QStringLiteral("/org/freedesktop/portal/desktop/request/%1/%2").arg(sender, token),
        QStringLiteral("org.freedesktop.portal.Request"), QStringLiteral("Response"),
        job, SLOT(prepared(uint, QVariantMap)));

    QDBusMessage prepare = call("PreparePrint");
    prepare << QString() << title << QVariantMap() << QVariantMap()
            << QVariantMap { { QStringLiteral("handle_token"), token } };
    auto* watcher = new QDBusPendingCallWatcher(bus.asyncCall(prepare), job);
    connect(watcher, &QDBusPendingCallWatcher::finished, job, [job](QDBusPendingCallWatcher* reply) {
        if (reply->isError()) { // no portal, or it can't print: nothing will answer
            qWarning("print: %s", qPrintable(reply->error().message()));
            QFile::remove(job->m_path);
            job->deleteLater();
        }
    });
}

void PortalPrint::prepared(uint response, const QVariantMap& results)
{
    QFile file(m_path);
    if (response == 0 && file.open(QIODevice::ReadOnly)) { // 0: Print was pressed
        QDBusMessage print = call("Print");
        print << QString() << m_title << QVariant::fromValue(QDBusUnixFileDescriptor(file.handle()))
              << QVariantMap { { QStringLiteral("token"), results.value(QStringLiteral("token")) } };
        const QDBusMessage reply = QDBusConnection::sessionBus().call(print);
        if (reply.type() == QDBusMessage::ErrorMessage) {
            qWarning("print: %s", qPrintable(reply.errorMessage()));
        }
    }
    file.remove();
    deleteLater();
}
