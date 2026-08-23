#pragma once

#include "papyrus/pdf/document.h"

#include <QDialog>
#include <QImage>

QT_BEGIN_NAMESPACE
class QGraphicsView;
class QGraphicsScene;
class QGraphicsPixmapItem;
class QSpinBox;
class QDoubleSpinBox;
class QComboBox;
QT_END_NAMESPACE

namespace papyrus {

// Places one signature image onto a chosen page: drag to move (native
// QGraphicsItem dragging), spinboxes for size/rotation (simpler and more
// reliable than hand-built resize/rotate handles — see AnnotationDialog's
// notes on why interactive drag geometry was kept minimal this pass).
// Embeds via AnnotationWriter::addImage as real page content on Save, so it
// always renders in Papyrus's own viewer too.
class SignaturePlacementDialog : public QDialog {
    Q_OBJECT
public:
    SignaturePlacementDialog(const QString& filePath, int initialPageIndex, const QImage& signatureImage,
                              QWidget* parent = nullptr);

signals:
    void documentSaved(const QString& filePath);

private:
    void loadPage(int pageIndex);
    void updateItemTransform();
    void place();

    QString m_filePath;
    int m_pageIndex;
    QImage m_signatureImage;
    pdf::Document m_previewDocument;
    double m_pointsPerScenePixel = 1.0;

    QGraphicsView* m_view;
    QGraphicsScene* m_scene;
    QGraphicsPixmapItem* m_signatureItem = nullptr;
    QComboBox* m_pageCombo;
    QDoubleSpinBox* m_sizeSpin;
    QSpinBox* m_rotationSpin;
};

} // namespace papyrus
