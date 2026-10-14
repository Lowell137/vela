// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

// One Notepad tab's file: reads it into the tab's TextEdit and writes it back
// with the encoding and the line endings it had. The text itself lives in the
// TextEdit's document (`textDocument`), which is also where searching, the
// modified flag and line numbers come from, without copying the text to QML.

#include <QByteArray>
#include <QObject>
#include <QPoint>
#include <QPointer>
#include <QQuickTextDocument>
#include <QString>
#include <QStringList>
#include <QUrl>

class QTextDocument;

class Document : public QObject {
    Q_OBJECT
    Q_PROPERTY(QQuickTextDocument* textDocument READ textDocument WRITE setTextDocument NOTIFY textDocumentChanged)
    Q_PROPERTY(QUrl url READ url NOTIFY urlChanged)
    Q_PROPERTY(QString name READ name NOTIFY urlChanged) // the file's name; empty: never saved
    Q_PROPERTY(bool modified READ modified NOTIFY modifiedChanged)
    Q_PROPERTY(QString encoding READ encoding WRITE setEncoding NOTIFY encodingChanged)
    Q_PROPERTY(QString lineEnding READ lineEnding WRITE setLineEnding NOTIFY lineEndingChanged)
    Q_PROPERTY(QStringList encodings READ encodings CONSTANT)

public:
    using QObject::QObject;

    QQuickTextDocument* textDocument() const { return m_quickDocument; }
    void setTextDocument(QQuickTextDocument* document);
    QUrl url() const { return m_url; }
    QString name() const { return m_url.fileName(); }
    bool modified() const;
    QString encoding() const { return m_encoding; }
    void setEncoding(const QString& encoding);
    QString lineEnding() const { return m_lineEnding; }
    void setLineEnding(const QString& lineEnding);
    static QStringList encodings();

    // Both return the error to show, empty if it went well. A file that
    // doesn't exist yet opens empty: the first save creates it.
    Q_INVOKABLE QString load(const QUrl& url);
    Q_INVOKABLE QString save(const QUrl& url);

    // The next (or previous) occurrence from `from`, starting over at the
    // other end: (start, end), or (-1, -1).
    Q_INVOKABLE QPoint find(const QString& needle, int from, bool caseSensitive, bool backward) const;
    // One undo step. Returns how many were replaced.
    Q_INVOKABLE int replaceAll(const QString& needle, const QString& replacement, bool caseSensitive);
    // (line, column) of a position, from 1, and back.
    Q_INVOKABLE QPoint lineColumn(int position) const;
    Q_INVOKABLE int positionOfLine(int line) const;
    Q_INVOKABLE int lineCount() const;
    // The text on paper, in that font: through the system's Print dialog.
    Q_INVOKABLE void print(const QString& fontFamily) const;

    // The bytes of a file as text with "\n" line ends, and what it was
    // ("UTF-8", "UTF-8 with BOM", "UTF-16 LE", "UTF-16 BE", "ISO-8859-1";
    // "LF", "CRLF", "CR").
    static QString decode(const QByteArray& data, QString* encoding, QString* lineEnding);
    // The opposite. `ok` false: the encoding can't hold some characters.
    static QByteArray encode(QString text, const QString& encoding, const QString& lineEnding, bool* ok);

signals:
    void textDocumentChanged();
    void urlChanged();
    void modifiedChanged();
    void encodingChanged();
    void lineEndingChanged();

private:
    QTextDocument* document() const;

    QPointer<QQuickTextDocument> m_quickDocument;
    QUrl m_url;
    QString m_encoding = QStringLiteral("UTF-8");
    QString m_lineEnding = QStringLiteral("LF");
};
