#pragma once

#include "papyrus/pdf/annotation_writer.h"
#include "papyrus/pdf/document.h"

#include <QColor>
#include <QDialog>
#include <QList>

QT_BEGIN_NAMESPACE
class QGraphicsView;
class QGraphicsScene;
class QGraphicsItem;
class QToolButton;
QT_END_NAMESPACE

namespace papyrus {

// "Annotate this page" dialog: draw rectangles/circles by dragging on a
// static render of one page, then Save bakes every shape into the PDF via
// AnnotationWriter in a single pass.
//
// Only Rectangle and Circle shapes are offered here — freehand ink, plain
// lines/arrows and free-text boxes were tried against PDFium's annotation
// API and, despite saving without error, rendered nothing back when
// reopened (no hand-built appearance stream, and PDFium's default-AP
// generation doesn't cover them in this build). Shipping them would look
// broken, so they're left out until that's solved. Highlight is handled
// separately, from search results (see DocumentTab), since it marks
// existing text rather than being drawn freehand.
class AnnotationDialog : public QDialog {
    Q_OBJECT
public:
    explicit AnnotationDialog(const QString& filePath, int pageIndex, QWidget* parent = nullptr);

signals:
    void documentSaved(const QString& filePath);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    enum class Tool { Rectangle, Circle };

    struct PendingShape {
        pdf::AnnotationShape shape;
        QRectF pdfRect; // top-down page-point space
        QColor color;
        QGraphicsItem* graphicsItem;
    };

    bool loadPage();
    void pickColor();
    void undoLastShape();
    void saveAnnotations();
    QRectF sceneRectToPdf(const QRectF& sceneRect) const;

    QString m_filePath;
    int m_pageIndex;
    pdf::Document m_previewDocument;
    double m_pointsPerScenePixel = 1.0;

    QGraphicsView* m_view;
    QGraphicsScene* m_scene;
    Tool m_currentTool = Tool::Rectangle;
    QColor m_currentColor{255, 0, 0};
    QToolButton* m_colorButton;

    QGraphicsItem* m_dragItem = nullptr;
    QPointF m_dragStart;
    QList<PendingShape> m_pendingShapes;
};

} // namespace papyrus
