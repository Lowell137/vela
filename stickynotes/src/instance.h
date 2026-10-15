// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QLocalServer>
#include <QObject>

// One process per user: a second start tells the first to show its list.
class Instance : public QObject
{
    Q_OBJECT

public:
    explicit Instance(QObject* parent = nullptr) : QObject(parent) {}

    // False when another instance was found and told to come forward.
    bool acquire();

signals:
    void activated();

private:
    QLocalServer m_server;
};
