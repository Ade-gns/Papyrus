#include "signature_pad_widget.h"

#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>

namespace papyrus {

namespace {
void drawStroke(QPainter& painter, const QVector<QPointF>& stroke) {
    if (stroke.size() < 2) {
        if (stroke.size() == 1) {
            painter.drawPoint(stroke.first());
        }
        return;
    }
    QPainterPath path(stroke.first());
    for (int i = 1; i < stroke.size(); ++i) {
        path.lineTo(stroke[i]);
    }
    painter.drawPath(path);
}
} // namespace

SignaturePadWidget::SignaturePadWidget(QWidget* parent) : QWidget(parent) {
    setMinimumSize(400, 200);
}

bool SignaturePadWidget::isEmpty() const {
    return m_strokes.isEmpty();
}

void SignaturePadWidget::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.fillRect(rect(), Qt::white);
    painter.setPen(QPen(QColor(200, 200, 200), 1));
    painter.drawRect(rect().adjusted(0, 0, -1, -1));

    painter.setPen(QPen(m_penColor, m_penWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    for (const Stroke& stroke : m_strokes) {
        drawStroke(painter, stroke);
    }
    drawStroke(painter, m_currentStroke);
}

void SignaturePadWidget::mousePressEvent(QMouseEvent* event) {
    if (event->button() != Qt::LeftButton) {
        return;
    }
    m_drawing = true;
    m_currentStroke.clear();
    m_currentStroke.append(event->position());
    m_redoStack.clear();
    update();
}

void SignaturePadWidget::mouseMoveEvent(QMouseEvent* event) {
    if (!m_drawing) {
        return;
    }
    m_currentStroke.append(event->position());
    update();
}

void SignaturePadWidget::mouseReleaseEvent(QMouseEvent* event) {
    if (!m_drawing || event->button() != Qt::LeftButton) {
        return;
    }
    m_drawing = false;
    if (!m_currentStroke.isEmpty()) {
        m_strokes.append(m_currentStroke);
        m_currentStroke.clear();
        emit contentChanged();
    }
    update();
}

void SignaturePadWidget::clear() {
    m_strokes.clear();
    m_redoStack.clear();
    m_currentStroke.clear();
    update();
    emit contentChanged();
}

void SignaturePadWidget::undo() {
    if (m_strokes.isEmpty()) {
        return;
    }
    m_redoStack.append(m_strokes.takeLast());
    update();
    emit contentChanged();
}

void SignaturePadWidget::redo() {
    if (m_redoStack.isEmpty()) {
        return;
    }
    m_strokes.append(m_redoStack.takeLast());
    update();
    emit contentChanged();
}

void SignaturePadWidget::setPenWidth(int width) {
    m_penWidth = width;
}

void SignaturePadWidget::setPenColor(const QColor& color) {
    m_penColor = color;
}

QImage SignaturePadWidget::exportImage() const {
    if (m_strokes.isEmpty()) {
        return {};
    }
    QRectF bounds;
    for (const Stroke& stroke : m_strokes) {
        for (const QPointF& point : stroke) {
            const QRectF pointRect(point, QSizeF(0, 0));
            bounds = bounds.isNull() ? pointRect : bounds.united(pointRect);
        }
    }
    const qreal margin = m_penWidth;
    bounds = bounds.adjusted(-margin, -margin, margin, margin).intersected(rect());

    QImage image(size(), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(m_penColor, m_penWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    for (const Stroke& stroke : m_strokes) {
        drawStroke(painter, stroke);
    }
    painter.end();
    return image.copy(bounds.toRect());
}

} // namespace papyrus
