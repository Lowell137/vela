// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "document.h"

#include "portalprint.h"

#include <QDir>
#include <QFile>
#include <QFont>
#include <QMarginsF>
#include <QPageLayout>
#include <QPageSize>
#include <QPdfWriter>
#include <QQuickTextDocument>
#include <QSaveFile>
#include <QStringDecoder>
#include <QStringEncoder>
#include <QTemporaryFile>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>

namespace {

// shortcut: the whole file is one string in a TextEdit, so very large files
// are refused instead of freezing the window; upgrade to a paged view if
// people need to open logs bigger than this.
constexpr qint64 MaxFileSize = 64 * 1024 * 1024;

const QByteArray Utf8Bom = QByteArrayLiteral("\xEF\xBB\xBF");
const QByteArray Utf16LeBom = QByteArrayLiteral("\xFF\xFE");
const QByteArray Utf16BeBom = QByteArrayLiteral("\xFE\xFF");

QTextDocument::FindFlags findFlags(bool caseSensitive, bool backward)
{
    QTextDocument::FindFlags flags;
    flags.setFlag(QTextDocument::FindCaseSensitively, caseSensitive);
    flags.setFlag(QTextDocument::FindBackward, backward);
    return flags;
}

} // namespace

void Document::setTextDocument(QQuickTextDocument* document)
{
    if (document == m_quickDocument) {
        return;
    }
    m_quickDocument = document;
    if (QTextDocument* d = this->document()) {
        connect(d, &QTextDocument::modificationChanged, this, &Document::modifiedChanged);
    }
    emit textDocumentChanged();
    emit modifiedChanged();
}

QTextDocument* Document::document() const
{
    return m_quickDocument ? m_quickDocument->textDocument() : nullptr;
}

bool Document::modified() const
{
    const QTextDocument* d = document();
    return d && d->isModified();
}

QStringList Document::encodings()
{
    return {
        QStringLiteral("UTF-8"),
        QStringLiteral("UTF-8 with BOM"),
        QStringLiteral("UTF-16 LE"),
        QStringLiteral("UTF-16 BE"),
        QStringLiteral("ISO-8859-1"),
    };
}

// Choosing another encoding or line ending changes what a save writes: the
// file counts as modified.
void Document::setEncoding(const QString& encoding)
{
    if (encoding == m_encoding || !encodings().contains(encoding)) {
        return;
    }
    m_encoding = encoding;
    emit encodingChanged();
    if (QTextDocument* d = document()) {
        d->setModified(true);
    }
}

void Document::setLineEnding(const QString& lineEnding)
{
    if (lineEnding == m_lineEnding) {
        return;
    }
    m_lineEnding = lineEnding;
    emit lineEndingChanged();
    if (QTextDocument* d = document()) {
        d->setModified(true);
    }
}

QString Document::decode(const QByteArray& data, QString* encoding, QString* lineEnding)
{
    QString text;
    if (data.startsWith(Utf8Bom)) {
        *encoding = QStringLiteral("UTF-8 with BOM");
        text = QString::fromUtf8(data.mid(Utf8Bom.size()));
    } else if (data.startsWith(Utf16LeBom)) {
        *encoding = QStringLiteral("UTF-16 LE");
        text = QStringDecoder(QStringDecoder::Utf16LE)(data.mid(Utf16LeBom.size()));
    } else if (data.startsWith(Utf16BeBom)) {
        *encoding = QStringLiteral("UTF-16 BE");
        text = QStringDecoder(QStringDecoder::Utf16BE)(data.mid(Utf16BeBom.size()));
    } else {
        QStringDecoder utf8(QStringDecoder::Utf8, QStringDecoder::Flag::Stateless);
        text = utf8(data);
        if (utf8.hasError()) {
            // Not UTF-8: Latin-1 keeps every byte as it is, so saving a file
            // nobody touched writes back the same bytes.
            *encoding = QStringLiteral("ISO-8859-1");
            text = QString::fromLatin1(data);
        } else {
            *encoding = QStringLiteral("UTF-8");
        }
    }

    // The first line end decides, like Windows' Notepad; a file that mixes
    // them is saved with that one everywhere.
    const qsizetype cr = text.indexOf(u'\r');
    if (cr < 0) {
        *lineEnding = QStringLiteral("LF");
    } else if (cr + 1 < text.size() && text.at(cr + 1) == u'\n') {
        *lineEnding = QStringLiteral("CRLF");
    } else {
        *lineEnding = QStringLiteral("CR");
    }
    text.replace(QLatin1String("\r\n"), QLatin1String("\n"));
    text.replace(u'\r', u'\n');
    return text;
}

QByteArray Document::encode(QString text, const QString& encoding, const QString& lineEnding, bool* ok)
{
    *ok = true;
    if (lineEnding == QLatin1String("CRLF")) {
        text.replace(u'\n', QLatin1String("\r\n"));
    } else if (lineEnding == QLatin1String("CR")) {
        text.replace(u'\n', u'\r');
    }
    if (encoding == QLatin1String("UTF-8 with BOM")) {
        return Utf8Bom + text.toUtf8();
    }
    if (encoding == QLatin1String("UTF-16 LE")) {
        return Utf16LeBom + QByteArray(QStringEncoder(QStringEncoder::Utf16LE)(text));
    }
    if (encoding == QLatin1String("UTF-16 BE")) {
        return Utf16BeBom + QByteArray(QStringEncoder(QStringEncoder::Utf16BE)(text));
    }
    if (encoding == QLatin1String("ISO-8859-1")) {
        QStringEncoder latin1(QStringEncoder::Latin1);
        QByteArray data = latin1(text);
        *ok = !latin1.hasError();
        return data;
    }
    return text.toUtf8();
}

QString Document::load(const QUrl& url)
{
    QTextDocument* d = document();
    if (!d) {
        return tr("The editor isn't ready.");
    }
    if (!url.isLocalFile()) {
        return tr("Only files on this computer can be opened.");
    }
    QFile file(url.toLocalFile());
    QString text;
    QString encoding = QStringLiteral("UTF-8");
    QString lineEnding = QStringLiteral("LF");
    if (file.exists()) {
        if (!file.open(QIODevice::ReadOnly)) {
            return tr("Can't open \"%1\": %2").arg(url.toLocalFile(), file.errorString());
        }
        if (file.size() > MaxFileSize) {
            return tr("\"%1\" is too large for Notepad.").arg(url.toLocalFile());
        }
        text = decode(file.readAll(), &encoding, &lineEnding);
    }
    d->setPlainText(text); // also empties the undo history
    d->setModified(false); // the TextEdit takes the new text for an edit
    m_url = url;
    m_encoding = encoding;
    m_lineEnding = lineEnding;
    emit urlChanged();
    emit encodingChanged();
    emit lineEndingChanged();
    return {};
}

QString Document::save(const QUrl& url)
{
    QTextDocument* d = document();
    if (!d) {
        return tr("The editor isn't ready.");
    }
    if (!url.isLocalFile()) {
        return tr("Files can only be saved on this computer.");
    }
    bool ok = false;
    const QByteArray data = encode(d->toPlainText(), m_encoding, m_lineEnding, &ok);
    if (!ok) {
        return tr("This text has characters that %1 can't hold. Choose another encoding, such as UTF-8, and save again.").arg(m_encoding);
    }
    // Written beside the file and renamed over it: a full disk or a crash
    // leaves the old file whole. Where the folder can't be written (a file
    // of ours in someone else's folder) it's written in place.
    QSaveFile file(url.toLocalFile());
    file.setDirectWriteFallback(true);
    if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size() || !file.commit()) {
        return tr("Can't save \"%1\": %2").arg(url.toLocalFile(), file.errorString());
    }
    d->setModified(false);
    if (url != m_url) {
        m_url = url;
        emit urlChanged();
    }
    return {};
}

QPoint Document::find(const QString& needle, int from, bool caseSensitive, bool backward) const
{
    QTextDocument* d = document();
    if (!d || needle.isEmpty()) {
        return { -1, -1 };
    }
    const QTextDocument::FindFlags flags = findFlags(caseSensitive, backward);
    QTextCursor found = d->find(needle, from, flags);
    if (found.isNull()) {
        found = d->find(needle, backward ? d->characterCount() - 1 : 0, flags);
    }
    return found.isNull() ? QPoint(-1, -1) : QPoint(found.selectionStart(), found.selectionEnd());
}

int Document::replaceAll(const QString& needle, const QString& replacement, bool caseSensitive)
{
    QTextDocument* d = document();
    if (!d || needle.isEmpty()) {
        return 0;
    }
    const QTextDocument::FindFlags flags = findFlags(caseSensitive, false);
    int count = 0;
    QTextCursor block(d);
    block.beginEditBlock();
    // The search goes on after what was just written, so a replacement that
    // contains the searched text isn't replaced again.
    for (QTextCursor found = d->find(needle, 0, flags); !found.isNull(); found = d->find(needle, found, flags)) {
        found.insertText(replacement);
        ++count;
    }
    block.endEditBlock();
    return count;
}

QPoint Document::lineColumn(int position) const
{
    const QTextDocument* d = document();
    if (!d) {
        return { 1, 1 };
    }
    const QTextBlock block = d->findBlock(position);
    return block.isValid() ? QPoint(block.blockNumber() + 1, position - block.position() + 1) : QPoint(1, 1);
}

int Document::positionOfLine(int line) const
{
    const QTextDocument* d = document();
    if (!d) {
        return 0;
    }
    return d->findBlockByNumber(qBound(1, line, d->blockCount()) - 1).position();
}

int Document::lineCount() const
{
    const QTextDocument* d = document();
    return d ? d->blockCount() : 1;
}

void Document::print(const QString& fontFamily) const
{
    QTextDocument* d = document();
    QTemporaryFile file(QDir::temp().filePath(QStringLiteral("vela-notepad-XXXXXX.pdf")));
    file.setAutoRemove(false); // the print job removes it
    if (!d || !file.open()) {
        return;
    }
    {
        QPdfWriter pdf(&file);
        pdf.setPageSize(QPageSize(QPageSize::A4));
        pdf.setPageMargins(QMarginsF(20, 20, 20, 20), QPageLayout::Millimeter);
        QTextDocument page; // black on white whatever the theme, wrapped to the paper
        page.setDefaultFont(QFont(fontFamily, 11));
        page.setPlainText(d->toPlainText());
        page.print(&pdf);
    }
    file.close();
    PortalPrint::print(file.fileName(), name().isEmpty() ? tr("Untitled") : name());
}
