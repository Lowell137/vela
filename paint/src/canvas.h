// SPDX-FileCopyrightText: 2026 Vela contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

// Paint's picture and what the tools do to it (`PaintCanvas` in QML). The
// item is as large as the picture, one unit per pixel: QML scales it to zoom
// and passes the pointer in its own coordinates, which are the picture's.

#include <QColor>
#include <QImage>
#include <QList>
#include <QPainterPath>
#include <QPointF>
#include <QQuickPaintedItem>
#include <QRect>
#include <QString>
#include <QStringList>
#include <QUrl>

class PaintCanvas : public QQuickPaintedItem {
    Q_OBJECT
    // pencil, brush, eraser, fill, picker, select, lasso (a free-form
    // selection), curve (a line, then bent by two more drags), polygon (a
    // corner per click), or a shape dragged out: line, rectangle, ellipse,
    // rounded, triangle, right-triangle, diamond, pentagon, hexagon, star,
    // star4, star6, arrow, arrow-left, arrow-up, arrow-down, lightning,
    // callout, heart. Other names (text, zoom) are QML's: press() ignores them.
    Q_PROPERTY(QString tool READ tool WRITE setTool NOTIFY toolChanged)
    Q_PROPERTY(QColor color MEMBER m_color NOTIFY colorChanged) // the left button's
    Q_PROPERTY(QColor color2 MEMBER m_color2 NOTIFY colorChanged) // the right button's, and what's erased to
    Q_PROPERTY(int size MEMBER m_size NOTIFY sizeChanged) // the line's width
    Q_PROPERTY(bool filled MEMBER m_filled NOTIFY filledChanged) // shapes are filled with color2
    Q_PROPERTY(bool outlined MEMBER m_outlined NOTIFY filledChanged) // and have their line
    // round, calligraphy, airbrush, marker, crayon or watercolor.
    Q_PROPERTY(QString brush MEMBER m_brush NOTIFY brushChanged)
    // What's moved or pasted leaves out color2: only the drawing is carried.
    Q_PROPERTY(bool transparentSelection MEMBER m_transparent NOTIFY transparentSelectionChanged)
    Q_PROPERTY(QSize imageSize READ imageSize NOTIFY imageSizeChanged)
    Q_PROPERTY(QRect selection READ selection NOTIFY selectionChanged)
    Q_PROPERTY(bool modified READ modified NOTIFY modifiedChanged)
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY historyChanged)
    Q_PROPERTY(bool canRedo READ canRedo NOTIFY historyChanged)
    Q_PROPERTY(QUrl source READ source NOTIFY sourceChanged) // the file; empty: never saved

public:
    explicit PaintCanvas(QQuickItem* parent = nullptr);

    void paint(QPainter* painter) override;

    QString tool() const { return m_tool; }
    void setTool(const QString& tool);
    QSize imageSize() const { return m_image.size(); }
    QRect selection() const { return m_selection; }
    bool modified() const { return m_modified; }
    bool canUndo() const { return !m_undo.isEmpty(); }
    bool canRedo() const { return !m_redo.isEmpty(); }
    QUrl source() const { return m_source; }
    const QImage& image() const { return m_image; }

    Q_INVOKABLE void newImage(int width, int height);
    // The error, empty if it went.
    Q_INVOKABLE QString load(const QUrl& url);
    Q_INVOKABLE QString save(const QUrl& url);
    Q_INVOKABLE QString filter() const; // "*.png *.jpg ...", what can be written

    // The pointer; `secondary`: the right button, which draws with color2;
    // `constrain` (Shift): squares, circles and lines at 45 degrees.
    Q_INVOKABLE void press(qreal x, qreal y, bool secondary = false);
    Q_INVOKABLE void move(qreal x, qreal y, bool constrain = false);
    Q_INVOKABLE void release();
    // Closes the polygon, or leaves the curve as it is (a double click).
    Q_INVOKABLE void finishShape();

    Q_INVOKABLE void undo();
    Q_INVOKABLE void redo();

    // The selection if there is one, otherwise the picture.
    Q_INVOKABLE void rotate(); // a quarter turn clockwise
    Q_INVOKABLE void flip(bool horizontal);
    // `scale`: the picture is stretched; otherwise cut, or extended with color2.
    Q_INVOKABLE void resize(int width, int height, bool scale);
    Q_INVOKABLE void crop(); // to the selection

    Q_INVOKABLE void selectAll();
    Q_INVOKABLE void deselect(); // what was being moved stays where it is
    Q_INVOKABLE void invertSelection();
    Q_INVOKABLE void stretchSelection(const QRect& to); // its pixels, scaled to another rectangle
    Q_INVOKABLE void deleteSelection();
    Q_INVOKABLE void copy();
    Q_INVOKABLE void cut();
    Q_INVOKABLE void paste();

    Q_INVOKABLE void drawText(qreal x, qreal y, const QString& text, const QString& family, int pixelSize,
        bool bold = false, bool italic = false, bool underline = false);
    Q_INVOKABLE QStringList fontFamilies() const;

signals:
    void toolChanged();
    void colorChanged();
    void sizeChanged();
    void filledChanged();
    void brushChanged();
    void transparentSelectionChanged();
    void imageSizeChanged();
    void selectionChanged();
    void modifiedChanged();
    void historyChanged();
    void sourceChanged();
    void picked(const QColor& color, bool secondary);

private:
    enum class Drag { None, Stroke, Shape, Selecting, Moving, Curve, Polygon };

    void setImage(const QImage& image);
    void remember(); // the picture as it is, for Undo
    void setModified(bool modified);
    void setSelection(const QRect& selection);
    void land(); // what's being moved or still drawn becomes part of the picture
    void lift(); // the selection's pixels leave the picture, to be moved
    QImage selected() const; // the selection's pixels, without what's outside its outline
    void clearSelected(QImage& image) const; // the selection's place, back to color2
    void dropBackground(QImage& image) const; // a transparent selection
    void fill(const QPoint& at, const QColor& color);
    void stroke(QPainter& painter, const QPointF& from, const QPointF& to) const;
    void style(QPainter& painter) const; // the shapes' line and filling
    void drawShape(QPainter& painter, const QPointF& from, const QPointF& to) const;
    void drawPending(QPainter& painter) const; // the curve or polygon not finished yet
    QPointF constrained(const QPointF& from, const QPointF& to) const;
    bool isShape() const;

    QImage m_image;
    QList<QImage> m_undo;
    QList<QImage> m_redo;
    QUrl m_source;
    bool m_modified = false;

    QString m_tool = QStringLiteral("pencil");
    QColor m_color = Qt::black;
    QColor m_color2 = Qt::white;
    int m_size = 3;
    bool m_filled = false;
    bool m_outlined = true;
    QString m_brush = QStringLiteral("round");
    bool m_transparent = false;

    Drag m_drag = Drag::None;
    bool m_secondary = false;
    QPointF m_from; // where the button went down
    QPointF m_last;
    QRect m_selection;
    QPainterPath m_outline; // a selection that isn't its rectangle; empty once it's lifted
    QImage m_floating; // the selection's pixels while they're moved; null: still in the picture
    QImage m_unscaled; // them, before stretching: stretched again from these
    QImage m_layer; // a see-through brush's stroke, laid on the picture when it ends
    bool m_constrain = false;
    QList<QPointF> m_points; // the free-form selection being drawn, or the polygon's corners
    QList<QPointF> m_curve; // its ends and the two points that bend it
    int m_bends = -1; // -1: no curve; else how many of its three drags are done
    QPoint m_grab; // the pointer, from the selection's corner
};
