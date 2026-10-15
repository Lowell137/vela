// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

// Printing without Qt Widgets: the app writes a PDF (QPdfWriter) and the
// portal shows the system's Print dialog and prints it. Used by Notepad and
// Photos.

#include <QObject>
#include <QString>
#include <QVariantMap>

class PortalPrint : public QObject {
    Q_OBJECT

public:
    // Asks how to print the PDF at `path`, prints it, and removes the file.
    static void print(const QString& path, const QString& title);

private slots:
    void prepared(uint response, const QVariantMap& results);

private:
    QString m_path;
    QString m_title;
};
