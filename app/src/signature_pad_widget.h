#pragma once

#include <QColor>
#include <QList>
#include <QPointF>
#include <QVector>
#include <QWidget>

namespace papyrus {

// Freehand drawing pad for signing with the mouse/trackpad/stylus: mouse
// press-drag-release accumulates one stroke, strokes are kept as a list so
// undo/redo just pop/push between two stacks. Background stays transparent
// so exportImage() can be composited straight onto a page.
class SignaturePadWidget : public QWidget {
    Q_OBJECT
public:
    explicit SignaturePadWidget(QWidget* parent = nullptr);

    bool isEmpty() const;
    // Cropped to the ink's bounding box (with a small margin), transparent
    // background, at the pad's on-screen resolution.
    QImage exportImage() const;

public slots:
    void clear();
    void undo();
    void redo();
    void setPenWidth(int width);
    void setPenColor(const QColor& color);

signals:
    void contentChanged();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    using Stroke = QVector<QPointF>;

    QList<Stroke> m_strokes;
    QList<Stroke> m_redoStack;
    Stroke m_currentStroke;
    bool m_drawing = false;
    int m_penWidth = 3;
    QColor m_penColor{Qt::black};
};

} // namespace papyrus
