// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "instance.h"

#include <QLocalSocket>

#include <unistd.h>

bool Instance::acquire()
{
    const QString name = QStringLiteral("vela-stickynotes-%1").arg(getuid());
    QLocalSocket socket;
    socket.connectToServer(name);
    if (socket.waitForConnected(300)) {
        socket.write("show");
        socket.waitForBytesWritten(300);
        return false;
    }
    QLocalServer::removeServer(name); // a stale socket of a crashed run
    m_server.listen(name);
    connect(&m_server, &QLocalServer::newConnection, this, [this] {
        while (QLocalSocket* client = m_server.nextPendingConnection()) {
            client->deleteLater();
        }
        emit activated();
    });
    return true;
}
