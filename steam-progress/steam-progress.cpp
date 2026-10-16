// SPDX-FileCopyrightText: 2026 Brando Giuffrida
// SPDX-License-Identifier: GPL-3.0-or-later

// Steam's download progress on its taskbar button (standalone, optional).
//
// Steam on Linux sends no progress of its own. It writes it to
// steamapps/appmanifest_*.acf and the files it has staged to
// steamapps/downloading/<appid>/. This reads both and sends a Unity
// LauncherEntry update for steam.desktop, which the Vela taskbar already draws.
//
//   steam-progress          polls every 2 s until killed
//   steam-progress --once   prints the progress and exits
//
// STEAM_DIR overrides ~/.local/share/Steam (for tests).

#include "progress.h"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDir>
#include <QTimer>
#include <QVariantMap>

#include <cstdio>

namespace {
void send(const QVariantMap& props)
{
    auto msg = QDBusMessage::createSignal(QStringLiteral("/com/canonical/unity/launcherentry/1"),
        QStringLiteral("com.canonical.Unity.LauncherEntry"), QStringLiteral("Update"));
    msg << QStringLiteral("application://steam.desktop") << props;
    QDBusConnection::sessionBus().send(msg);
}
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    if (app.arguments().contains(QStringLiteral("--once"))) {
        std::printf("progress: %g\n", steamprogress::progress(steamprogress::defaultDir()));
        return 0;
    }
    // One connection for the whole run: the shell drops a badge when its sender leaves the bus.
    bool shown = false;
    QTimer timer;
    QObject::connect(&timer, &QTimer::timeout, [&] {
        const double p = steamprogress::progress(steamprogress::defaultDir());
        if (p >= 0) {
            send({ { QStringLiteral("progress"), p }, { QStringLiteral("progress-visible"), true } });
            shown = true;
        } else if (shown) {
            send({ { QStringLiteral("progress-visible"), false } });
            shown = false;
        }
    });
    timer.start(2000);
    return app.exec();
}
