// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "canvas.h"

#include "imageformats.h"

#include <QClipboard>
#include <QFileInfo>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QImageReader>
#include <QImageWriter>
#include <QPainter>
#include <QPainterPath>
#include <QPolygonF>
#include <QRandomGenerator>
#include <QSaveFile>
#include <QTransform>

#include <cmath>

namespace {

// shortcut: Undo keeps whole pictures (shared until one is drawn on), at most
// this many; keep the changed tiles instead if large pictures need more.
constexpr int undoLimit = 40;

// Shapes drawn in the unit square, stretched to what's dragged.
QPolygonF outline(const QString& shape)
{
    auto regular = [](int sides) {
        QPolygonF polygon;
        for (int i = 0; i < sides; i++) {
            const double angle = -M_PI / 2 + 2 * M_PI * i / sides;
            polygon << QPointF(0.5 + 0.5 * std::cos(angle), 0.5 + 0.5 * std::sin(angle));
        }
        return polygon;
    };
    if (shape == QLatin1String("triangle")) {
        return { { 0.5, 0 }, { 1, 1 }, { 0, 1 } };
    }
    if (shape == QLatin1String("right-triangle")) {
        return { { 0, 0 }, { 1, 1 }, { 0, 1 } };
    }
    if (shape == QLatin1String("diamond")) {
        return { { 0.5, 0 }, { 1, 0.5 }, { 0.5, 1 }, { 0, 0.5 } };
    }
    if (shape == QLatin1String("pentagon")) {
        return regular(5);
    }
    if (shape == QLatin1String("hexagon")) {
        return regular(6);
    }
    auto star = [](int points, double inner) {
        QPolygonF polygon;
        for (int i = 0; i < points * 2; i++) {
            const double angle = -M_PI / 2 + M_PI * i / points;
            const double radius = i % 2 ? inner : 0.5;
            polygon << QPointF(0.5 + radius * std::cos(angle), 0.5 + radius * std::sin(angle));
        }
        return polygon;
    };
    const QPolygonF arrow { { 0, 0.3 }, { 0.6, 0.3 }, { 0.6, 0 }, { 1, 0.5 }, { 0.6, 1 }, { 0.6, 0.7 }, { 0, 0.7 } };
    auto turned = [&arrow](double degrees) {
        return QTransform().translate(0.5, 0.5).rotate(degrees).translate(-0.5, -0.5).map(arrow);
    };
    if (shape == QLatin1String("arrow")) {
        return arrow;
    }
    if (shape == QLatin1String("arrow-left")) {
        return turned(180);
    }
    if (shape == QLatin1String("arrow-up")) {
        return turned(-90);
    }
    if (shape == QLatin1String("arrow-down")) {
        return turned(90);
    }
    if (shape == QLatin1String("star")) {
        return star(5, 0.2);
    }
    if (shape == QLatin1String("star4")) {
        return star(4, 0.18);
    }
    if (shape == QLatin1String("star6")) {
        return star(6, 0.28);
    }
    if (shape == QLatin1String("lightning")) {
        return { { 0.55, 0 }, { 0.1, 0.55 }, { 0.45, 0.55 }, { 0.3, 1 }, { 0.9, 0.4 }, { 0.55, 0.4 }, { 0.8, 0 } };
    }
    if (shape == QLatin1String("callout")) {
        return { { 0, 0 }, { 1, 0 }, { 1, 0.7 }, { 0.5, 0.7 }, { 0.25, 1 }, { 0.3, 0.7 }, { 0, 0.7 } };
    }
    return {};
}

} // namespace

PaintCanvas::PaintCanvas(QQuickItem* parent)
    : QQuickPaintedItem(parent)
{
    newImage(1152, 648);
}

void PaintCanvas::paint(QPainter* painter)
{
    painter->drawImage(0, 0, m_image);
    if (!m_layer.isNull()) {
        painter->setOpacity(m_brush == QLatin1String("marker") ? 0.45 : 0.35);
        painter->drawImage(0, 0, m_layer);
        painter->setOpacity(1);
    }
    if (!m_floating.isNull()) {
        painter->drawImage(m_selection.topLeft(), m_floating);
    }
    if (m_drag == Drag::Shape) {
        drawShape(*painter, m_from, m_last);
    }
    drawPending(*painter);

    // Dashes seen on any color: the selection, or the one being drawn.
    QPainterPath dashes = m_outline;
    if (m_drag == Drag::Selecting && m_tool == QLatin1String("lasso")) {
        dashes = {};
        dashes.addPolygon(QPolygonF(m_points));
    } else if (dashes.isEmpty() && !m_selection.isEmpty()) {
        dashes.addRect(QRectF(m_selection).adjusted(0, 0, -1, -1));
    }
    if (!dashes.isEmpty()) {
        painter->setRenderHint(QPainter::Antialiasing, false);
        painter->setBrush(Qt::NoBrush);
        painter->setPen(QPen(Qt::white, 0));
        painter->drawPath(dashes);
        painter->setPen(QPen(Qt::black, 0, Qt::DashLine));
        painter->drawPath(dashes);
    }
}

void PaintCanvas::setImage(const QImage& image)
{
    const QSize before = m_image.size();
    m_image = image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    if (m_image.size() != before) {
        setSize(m_image.size());
        emit imageSizeChanged();
    }
    update();
}

void PaintCanvas::remember()
{
    m_undo.append(m_image);
    if (m_undo.size() > undoLimit) {
        m_undo.removeFirst();
    }
    m_redo.clear();
    emit historyChanged();
    setModified(true);
}

void PaintCanvas::setModified(bool modified)
{
    if (m_modified != modified) {
        m_modified = modified;
        emit modifiedChanged();
    }
}

void PaintCanvas::setSelection(const QRect& selection)
{
    if (m_selection != selection) {
        m_selection = selection;
        emit selectionChanged();
        update();
    }
}

void PaintCanvas::setTool(const QString& tool)
{
    if (m_tool == tool) {
        return;
    }
    const bool selects = tool == QLatin1String("select") || tool == QLatin1String("lasso");
    if (selects) {
        land(); // the selection itself stays
    } else {
        deselect();
    }
    m_tool = tool;
    emit toolChanged();
}

bool PaintCanvas::isShape() const
{
    static const QStringList simple { QStringLiteral("line"), QStringLiteral("rectangle"), QStringLiteral("ellipse"),
        QStringLiteral("rounded"), QStringLiteral("heart") };
    return simple.contains(m_tool) || !outline(m_tool).isEmpty();
}

QStringList PaintCanvas::fontFamilies() const
{
    return QFontDatabase::families();
}

// --- files ---

void PaintCanvas::newImage(int width, int height)
{
    QImage image(width, height, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::white);
    m_floating = {};
    m_outline = {};
    m_points.clear();
    m_bends = -1;
    setSelection({});
    m_undo.clear();
    m_redo.clear();
    emit historyChanged();
    setImage(image);
    m_source.clear();
    emit sourceChanged();
    setModified(false);
}

QString PaintCanvas::load(const QUrl& url)
{
    QString error;
    const QImage image = formats::read(url.toLocalFile(), {}, &error); // turned like the camera was
    if (image.isNull()) {
        return error;
    }
    newImage(1, 1);
    setImage(image);
    m_source = url;
    emit sourceChanged();
    return {};
}

QString PaintCanvas::save(const QUrl& url)
{
    land();
    QSaveFile file(url.toLocalFile()); // the old file stays if writing fails
    if (!file.open(QIODevice::WriteOnly)) {
        return file.errorString();
    }
    QByteArray format = QFileInfo(url.toLocalFile()).suffix().toLower().toLatin1();
    if (!QImageWriter::supportedImageFormats().contains(format)) {
        format = "png";
    }
    QImageWriter writer(&file, format);
    writer.setQuality(92);
    // A format without transparency gets the picture on white.
    QImage image = m_image;
    if (format == "jpg" || format == "jpeg" || format == "bmp") {
        image = QImage(m_image.size(), QImage::Format_RGB32);
        image.fill(Qt::white);
        QPainter(&image).drawImage(0, 0, m_image);
    }
    if (!writer.write(image) || !file.commit()) {
        return writer.error() != QImageWriter::UnknownError ? writer.errorString() : file.errorString();
    }
    m_source = url;
    emit sourceChanged();
    setModified(false);
    return {};
}

// What can be opened: what Qt reads, and what ImageMagick adds (AVIF, HEIC, PSD, RAW...).
QString PaintCanvas::openFilter() const
{
    QStringList list;
    const QList<QByteArray> readable = QImageReader::supportedImageFormats();
    for (const QByteArray& format : readable) {
        list.append(QStringLiteral("*.") + QString::fromLatin1(format));
    }
    const QStringList extra = formats::extra();
    for (const QString& suffix : extra) {
        list.append(QStringLiteral("*.") + suffix);
    }
    return list.join(u' ');
}

QString PaintCanvas::filter() const
{
    QStringList list;
    const QList<QByteArray> formats = QImageWriter::supportedImageFormats();
    for (const QByteArray& format : formats) {
        list.append(QStringLiteral("*.") + QString::fromLatin1(format));
    }
    return list.join(u' ');
}

// --- the pointer ---

void PaintCanvas::press(qreal x, qreal y, bool secondary)
{
    const QPointF at(x, y);
    const QPoint pixel(std::floor(x), std::floor(y));
    m_from = m_last = at;
    m_constrain = false;

    if (m_tool == QLatin1String("select") || m_tool == QLatin1String("lasso")) {
        if (m_selection.contains(pixel) && (m_outline.isEmpty() || m_outline.contains(at))) {
            lift();
            m_grab = pixel - m_selection.topLeft();
            m_drag = Drag::Moving;
        } else {
            deselect();
            m_points = { at };
            m_drag = Drag::Selecting;
        }
        return;
    }
    if (m_tool == QLatin1String("curve")) {
        if (m_bends < 0) {
            m_secondary = secondary;
            m_curve = { at, at, at, at };
            m_bends = 0;
        }
        m_drag = Drag::Curve;
        move(x, y);
        return;
    }
    if (m_tool == QLatin1String("polygon")) {
        if (m_points.isEmpty()) {
            m_secondary = secondary;
            m_points = { at, at };
        } else if (m_points.size() > 2 && QLineF(at, m_points.first()).length() < 6) {
            finishShape(); // back at the first corner: closed
            return;
        } else {
            m_points.append(at);
        }
        m_drag = Drag::Polygon;
        update();
        return;
    }
    m_secondary = secondary;
    if (m_tool == QLatin1String("picker")) {
        if (m_image.rect().contains(pixel)) {
            emit picked(m_image.pixelColor(pixel), secondary);
        }
        return;
    }
    if (m_tool == QLatin1String("fill")) {
        if (m_image.rect().contains(pixel)) {
            remember();
            fill(pixel, secondary ? m_color2 : m_color);
            update();
        }
        return;
    }
    if (isShape()) {
        m_drag = Drag::Shape;
        return;
    }
    if (m_tool != QLatin1String("pencil") && m_tool != QLatin1String("brush") && m_tool != QLatin1String("eraser")) {
        return; // QML's own (text, zoom)
    }
    remember();
    if (m_tool == QLatin1String("brush") && (m_brush == QLatin1String("marker") || m_brush == QLatin1String("watercolor"))) {
        m_layer = QImage(m_image.size(), QImage::Format_ARGB32_Premultiplied);
        m_layer.fill(Qt::transparent);
    }
    m_drag = Drag::Stroke;
    move(x, y); // a click leaves a dot
}

void PaintCanvas::move(qreal x, qreal y, bool constrain)
{
    const QPointF at(x, y);
    m_constrain = constrain;
    switch (m_drag) {
    case Drag::Stroke: {
        QPainter painter(m_layer.isNull() ? &m_image : &m_layer);
        stroke(painter, m_last, at);
        m_last = at;
        break;
    }
    case Drag::Selecting:
        if (m_tool == QLatin1String("lasso")) {
            m_points.append(at);
            update();
        } else {
            setSelection(QRectF(m_from, constrained(m_from, at)).normalized().toAlignedRect().intersected(m_image.rect()));
        }
        break;
    case Drag::Moving:
        setSelection(QRect(QPoint(std::floor(x), std::floor(y)) - m_grab, m_selection.size()));
        break;
    case Drag::Shape:
        m_last = constrained(m_from, at);
        break;
    case Drag::Curve:
        if (m_bends == 0) { // the line itself
            m_curve[1] = constrained(m_curve[0], at);
            m_curve[2] = m_curve[0];
            m_curve[3] = m_curve[1];
        } else if (m_bends == 1) {
            m_curve[2] = m_curve[3] = at;
        } else {
            m_curve[3] = at;
        }
        break;
    case Drag::Polygon:
        m_points.last() = constrained(m_points.at(m_points.size() - 2), at);
        break;
    case Drag::None:
        break;
    }
    if (m_drag != Drag::None) {
        update();
    }
}

void PaintCanvas::release()
{
    const Drag drag = m_drag;
    m_drag = Drag::None;
    if (drag == Drag::Shape && m_last != m_from) {
        remember();
        QPainter painter(&m_image);
        drawShape(painter, m_from, m_last);
    } else if (drag == Drag::Stroke && !m_layer.isNull()) {
        QPainter painter(&m_image);
        painter.setOpacity(m_brush == QLatin1String("marker") ? 0.45 : 0.35);
        painter.drawImage(0, 0, m_layer);
        m_layer = {};
    } else if (drag == Drag::Selecting && m_tool == QLatin1String("lasso")) {
        QPainterPath path;
        path.addPolygon(QPolygonF(m_points));
        path.closeSubpath();
        m_points.clear();
        const QRect area = path.boundingRect().toAlignedRect().intersected(m_image.rect());
        if (area.width() > 1 && area.height() > 1) {
            m_outline = path;
            setSelection(area);
        }
    } else if (drag == Drag::Curve) {
        if (m_bends == 0 && m_curve[0] == m_curve[1]) {
            m_bends = -1; // a click, not a line
        } else if (++m_bends == 3) {
            finishShape();
        }
    }
    update();
}

void PaintCanvas::finishShape()
{
    const bool curve = m_bends >= 0;
    if (!curve && m_points.size() < 3) {
        m_points.clear(); // a polygon needs three corners
    } else if (curve || m_tool == QLatin1String("polygon")) {
        remember();
        QPainter painter(&m_image);
        drawPending(painter);
    }
    if (m_tool == QLatin1String("polygon") || curve) {
        m_points.clear();
    }
    m_bends = -1;
    m_drag = Drag::None;
    update();
}

// Shift: a line every 45 degrees, anything else as wide as it's high.
QPointF PaintCanvas::constrained(const QPointF& from, const QPointF& to) const
{
    if (!m_constrain) {
        return to;
    }
    const QPointF d = to - from;
    if (m_tool == QLatin1String("line") || m_tool == QLatin1String("curve") || m_tool == QLatin1String("polygon")) {
        const double angle = std::round(std::atan2(d.y(), d.x()) / (M_PI / 4)) * (M_PI / 4);
        const double length = std::hypot(d.x(), d.y());
        return from + QPointF(length * std::cos(angle), length * std::sin(angle));
    }
    const double side = std::max(std::abs(d.x()), std::abs(d.y()));
    return from + QPointF(d.x() < 0 ? -side : side, d.y() < 0 ? -side : side);
}

// A piece of the line the pointer draws, with the tool in hand.
void PaintCanvas::stroke(QPainter& painter, const QPointF& from, const QPointF& to) const
{
    const bool eraser = m_tool == QLatin1String("eraser");
    const QColor color = eraser != m_secondary ? m_color2 : m_color;
    auto line = [&](const QPen& pen) {
        painter.setPen(pen);
        if (from == to) { // a line of no length draws nothing
            painter.drawPoint(to);
        } else {
            painter.drawLine(from, to);
        }
    };
    if (eraser) {
        painter.setCompositionMode(QPainter::CompositionMode_Source);
        line(QPen(color, m_size * 2, Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin));
        return;
    }
    if (m_tool == QLatin1String("pencil")) {
        line(QPen(color, m_size, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        return;
    }
    const double width = m_size * 2;
    auto* random = QRandomGenerator::global();
    // Dots thrown around a point: the airbrush's cloud, the crayon's grain.
    auto scatter = [&](const QPointF& center, double radius, int count) {
        for (int i = 0; i < count; i++) {
            const double angle = random->generateDouble() * 2 * M_PI;
            const double distance = radius * std::sqrt(random->generateDouble());
            painter.fillRect(QRectF(std::floor(center.x() + distance * std::cos(angle)),
                std::floor(center.y() + distance * std::sin(angle)), 1, 1), color);
        }
    };
    if (m_brush == QLatin1String("calligraphy")) { // a flat nib held at 45 degrees
        const QPointF nib(width / 2 * M_SQRT1_2, -width / 2 * M_SQRT1_2);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(QPen(color, 1));
        painter.setBrush(color);
        painter.drawPolygon(QPolygonF { from + nib, to + nib, to - nib, from - nib });
    } else if (m_brush == QLatin1String("airbrush")) {
        scatter(to, width * 1.5, m_size * 6);
    } else if (m_brush == QLatin1String("crayon")) {
        const int steps = std::max(1, int(QLineF(from, to).length()));
        for (int i = 1; i <= steps; i++) {
            scatter(from + (to - from) * i / steps, width / 2, std::max(2, m_size));
        }
    } else if (m_brush == QLatin1String("marker")) {
        line(QPen(color, width, Qt::SolidLine, Qt::SquareCap, Qt::BevelJoin));
    } else if (m_brush == QLatin1String("watercolor")) {
        painter.setRenderHint(QPainter::Antialiasing);
        line(QPen(color, width * 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    } else {
        painter.setRenderHint(QPainter::Antialiasing);
        line(QPen(color, width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    }
}

void PaintCanvas::style(QPainter& painter) const
{
    painter.setRenderHint(QPainter::Antialiasing);
    // Without a line and without a filling there'd be nothing: the line stays.
    painter.setPen(m_outlined || !m_filled
        ? QPen(m_secondary ? m_color2 : m_color, m_size, Qt::SolidLine, Qt::RoundCap, Qt::MiterJoin) : QPen(Qt::NoPen));
    painter.setBrush(m_filled ? QBrush(m_secondary ? m_color : m_color2) : Qt::NoBrush);
}

void PaintCanvas::drawPending(QPainter& painter) const
{
    if (m_bends >= 0) {
        style(painter);
        painter.setBrush(Qt::NoBrush);
        QPainterPath path(m_curve[0]);
        path.cubicTo(m_curve[2], m_curve[3], m_curve[1]);
        painter.drawPath(path);
    } else if (m_tool == QLatin1String("polygon") && m_points.size() > 1) {
        style(painter);
        if (m_drag == Drag::None && painter.device() == &m_image) {
            painter.drawPolygon(QPolygonF(m_points));
        } else { // still open while its corners are placed
            painter.setBrush(Qt::NoBrush);
            painter.drawPolyline(QPolygonF(m_points));
        }
    }
}

void PaintCanvas::drawShape(QPainter& painter, const QPointF& from, const QPointF& to) const
{
    style(painter);
    const QRectF box = QRectF(from, to).normalized();
    if (m_tool == QLatin1String("line")) {
        painter.drawLine(from, to);
    } else if (m_tool == QLatin1String("rectangle")) {
        painter.drawRect(box);
    } else if (m_tool == QLatin1String("ellipse")) {
        painter.drawEllipse(box);
    } else if (m_tool == QLatin1String("rounded")) {
        const double radius = std::min(box.width(), box.height()) / 5;
        painter.drawRoundedRect(box, radius, radius);
    } else if (m_tool == QLatin1String("heart")) {
        QPainterPath heart;
        heart.moveTo(0.5, 1);
        heart.cubicTo(-0.25, 0.55, 0.05, -0.1, 0.5, 0.25);
        heart.cubicTo(0.95, -0.1, 1.25, 0.55, 0.5, 1);
        painter.drawPath(QTransform::fromTranslate(box.x(), box.y()).scale(box.width(), box.height()).map(heart));
    } else {
        painter.drawPolygon(QTransform::fromTranslate(box.x(), box.y()).scale(box.width(), box.height()).map(outline(m_tool)));
    }
}

// Every pixel of at's color that touches it, line by line.
void PaintCanvas::fill(const QPoint& at, const QColor& color)
{
    const QRgb wanted = qPremultiply(color.rgba());
    auto* bits = reinterpret_cast<QRgb*>(m_image.bits());
    const int width = m_image.width();
    const int height = m_image.height();
    const QRgb old = bits[at.y() * width + at.x()];
    if (old == wanted) {
        return;
    }
    QList<QPoint> pending { at };
    while (!pending.isEmpty()) {
        const QPoint point = pending.takeLast();
        QRgb* line = bits + point.y() * width;
        if (line[point.x()] != old) {
            continue;
        }
        int left = point.x();
        int right = point.x();
        while (left > 0 && line[left - 1] == old) {
            left--;
        }
        while (right < width - 1 && line[right + 1] == old) {
            right++;
        }
        for (int x = left; x <= right; x++) {
            line[x] = wanted;
        }
        for (const int y : { point.y() - 1, point.y() + 1 }) {
            if (y < 0 || y >= height) {
                continue;
            }
            const QRgb* other = bits + y * width;
            for (int x = left; x <= right; x++) {
                if (other[x] == old && (x == left || other[x - 1] != old)) {
                    pending.append({ x, y });
                }
            }
        }
    }
}

// --- undo ---

void PaintCanvas::undo()
{
    if (m_undo.isEmpty()) {
        return;
    }
    m_floating = {};
    m_outline = {};
    m_points.clear();
    m_bends = -1;
    setSelection({});
    m_redo.append(m_image);
    setImage(m_undo.takeLast());
    emit historyChanged();
    setModified(true);
}

void PaintCanvas::redo()
{
    if (m_redo.isEmpty()) {
        return;
    }
    m_floating = {};
    m_outline = {};
    setSelection({});
    m_undo.append(m_image);
    setImage(m_redo.takeLast());
    emit historyChanged();
    setModified(true);
}

// --- the whole picture ---

void PaintCanvas::rotate()
{
    if (!m_selection.isEmpty()) {
        lift();
        m_floating = m_floating.transformed(QTransform().rotate(90));
        m_unscaled = {};
        QRect turned(QPoint(), m_floating.size());
        turned.moveCenter(m_selection.center());
        setSelection(turned);
        update();
        return;
    }
    land();
    remember();
    setImage(m_image.transformed(QTransform().rotate(90)));
}

void PaintCanvas::flip(bool horizontal)
{
    const Qt::Orientation way = horizontal ? Qt::Horizontal : Qt::Vertical;
    if (!m_selection.isEmpty()) {
        lift();
        m_floating = m_floating.flipped(way);
        m_unscaled = {};
        update();
        return;
    }
    land();
    remember();
    setImage(m_image.flipped(way));
}

void PaintCanvas::resize(int width, int height, bool scale)
{
    if (width < 1 || height < 1 || QSize(width, height) == m_image.size()) {
        return;
    }
    deselect();
    remember();
    if (scale) {
        setImage(m_image.scaled(width, height, Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
        return;
    }
    QImage image(width, height, QImage::Format_ARGB32_Premultiplied);
    image.fill(m_color2);
    QPainter(&image).drawImage(0, 0, m_image);
    setImage(image);
}

void PaintCanvas::crop()
{
    land();
    const QRect area = m_selection.intersected(m_image.rect());
    if (area.isEmpty()) {
        return;
    }
    remember();
    m_outline = {};
    setSelection({});
    setImage(m_image.copy(area));
}

// --- the selection ---

void PaintCanvas::land()
{
    if (m_bends >= 0 || (m_tool == QLatin1String("polygon") && !m_points.isEmpty())) {
        finishShape();
    }
    if (!m_floating.isNull()) {
        QPainter(&m_image).drawImage(m_selection.topLeft(), m_floating);
        m_floating = {};
        m_unscaled = {};
        update();
    }
}

QImage PaintCanvas::selected() const
{
    QImage image = m_image.copy(m_selection);
    if (!m_outline.isEmpty()) { // only what's inside it
        QImage inside(image.size(), QImage::Format_ARGB32_Premultiplied);
        inside.fill(Qt::transparent);
        QPainter painter(&inside);
        painter.setClipPath(m_outline.translated(-m_selection.topLeft()));
        painter.drawImage(0, 0, image);
        painter.end();
        image = inside;
    }
    dropBackground(image);
    return image;
}

void PaintCanvas::clearSelected(QImage& image) const
{
    QPainter painter(&image);
    painter.setCompositionMode(QPainter::CompositionMode_Source);
    if (m_outline.isEmpty()) {
        painter.fillRect(m_selection, m_color2);
    } else {
        painter.fillPath(m_outline, m_color2);
    }
}

void PaintCanvas::dropBackground(QImage& image) const
{
    if (!m_transparent) {
        return;
    }
    image = image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    const QRgb background = qPremultiply(m_color2.rgba());
    auto* bits = reinterpret_cast<QRgb*>(image.bits());
    for (qsizetype i = 0; i < qsizetype(image.width()) * image.height(); i++) {
        if (bits[i] == background) {
            bits[i] = 0;
        }
    }
}

void PaintCanvas::lift()
{
    if (!m_floating.isNull() || m_selection.isEmpty()) {
        return;
    }
    remember();
    m_floating = selected();
    clearSelected(m_image);
    m_outline = {};
    update();
}

void PaintCanvas::selectAll()
{
    land();
    if (m_tool != QLatin1String("select")) {
        setTool(QStringLiteral("select"));
    }
    m_outline = {};
    setSelection(m_image.rect());
}

void PaintCanvas::deselect()
{
    land();
    m_outline = {};
    setSelection({});
}

void PaintCanvas::invertSelection()
{
    land();
    if (m_selection.isEmpty()) {
        selectAll();
        return;
    }
    QPainterPath now = m_outline;
    if (now.isEmpty()) {
        now.addRect(m_selection);
    }
    QPainterPath all;
    all.addRect(m_image.rect());
    m_outline = all.subtracted(now);
    m_selection = {}; // so that the same rectangle with another outline is a change
    setSelection(m_image.rect());
}

void PaintCanvas::stretchSelection(const QRect& to)
{
    if (m_selection.isEmpty() || to.width() < 1 || to.height() < 1) {
        return;
    }
    lift();
    if (m_unscaled.isNull()) {
        m_unscaled = m_floating;
    }
    m_floating = m_unscaled.scaled(to.size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    setSelection(to);
    update();
}

void PaintCanvas::deleteSelection()
{
    if (m_selection.isEmpty()) {
        return;
    }
    if (m_floating.isNull()) { // otherwise it's already off the picture
        remember();
        clearSelected(m_image);
    }
    m_floating = {};
    m_unscaled = {};
    m_outline = {};
    setSelection({});
    update();
}

void PaintCanvas::copy()
{
    QGuiApplication::clipboard()->setImage(!m_floating.isNull() ? m_floating
        : m_selection.isEmpty() ? m_image : selected());
}

void PaintCanvas::cut()
{
    if (!m_selection.isEmpty()) {
        copy();
        deleteSelection();
    }
}

void PaintCanvas::paste()
{
    const QImage image = QGuiApplication::clipboard()->image();
    if (image.isNull()) {
        return;
    }
    deselect();
    remember();
    if (image.width() > m_image.width() || image.height() > m_image.height()) { // room for it, as in Windows
        QImage larger(m_image.size().expandedTo(image.size()), QImage::Format_ARGB32_Premultiplied);
        larger.fill(m_color2);
        QPainter(&larger).drawImage(0, 0, m_image);
        setImage(larger);
    }
    if (m_tool != QLatin1String("select")) {
        setTool(QStringLiteral("select"));
    }
    m_floating = image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    dropBackground(m_floating);
    setSelection(m_floating.rect());
    update();
}

void PaintCanvas::drawText(qreal x, qreal y, const QString& text, const QString& family, int pixelSize,
    bool bold, bool italic, bool underline)
{
    if (text.trimmed().isEmpty()) {
        return;
    }
    remember();
    QPainter painter(&m_image);
    painter.setRenderHint(QPainter::TextAntialiasing);
    QFont font(family);
    font.setPixelSize(pixelSize);
    font.setBold(bold);
    font.setItalic(italic);
    font.setUnderline(underline);
    painter.setFont(font);
    painter.setPen(m_color);
    painter.drawText(QRectF(x, y, m_image.width() * 4, m_image.height() * 4), Qt::AlignLeft | Qt::AlignTop, text);
    update();
}
