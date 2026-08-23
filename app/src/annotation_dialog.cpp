#include "annotation_dialog.h"

#include <QButtonGroup>
#include <QColorDialog>
#include <QEvent>
#include <QFile>
#include <QFileInfo>
#include <QGraphicsEllipseItem>
#include <QGraphicsRectItem>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPushButton>
#include <QToolButton>
#include <QVBoxLayout>

namespace papyrus {

namespace {
constexpr int kRenderWidthPx = 1000;
constexpr int kMinDragPx = 6;
} // namespace

AnnotationDialog::AnnotationDialog(const QString& filePath, int pageIndex, QWidget* parent)
    : QDialog(parent),
      m_filePath(filePath),
      m_pageIndex(pageIndex),
      m_view(new QGraphicsView(this)),
      m_scene(new QGraphicsScene(this)),
      m_colorButton(new QToolButton(this)) {
    setWindowTitle(tr("Annoter — %1 (page %2)").arg(QFileInfo(filePath).fileName()).arg(pageIndex + 1));
    resize(900, 750);

    m_view->setScene(m_scene);
    m_view->setRenderHint(QPainter::Antialiasing);
    m_view->viewport()->installEventFilter(this);

    auto* toolBar = new QHBoxLayout();
    auto* rectButton = new QToolButton(this);
    rectButton->setText(tr("▭ Rectangle"));
    rectButton->setCheckable(true);
    rectButton->setChecked(true);
    auto* circleButton = new QToolButton(this);
    circleButton->setText(tr("◯ Cercle"));
    circleButton->setCheckable(true);

    auto* toolGroup = new QButtonGroup(this);
    toolGroup->addButton(rectButton);
    toolGroup->addButton(circleButton);
    toolGroup->setExclusive(true);
    connect(rectButton, &QToolButton::toggled, this, [this](bool checked) {
        if (checked) m_currentTool = Tool::Rectangle;
    });
    connect(circleButton, &QToolButton::toggled, this, [this](bool checked) {
        if (checked) m_currentTool = Tool::Circle;
    });

    m_colorButton->setText(tr("Couleur"));
    m_colorButton->setStyleSheet(QStringLiteral("background-color: %1").arg(m_currentColor.name()));
    connect(m_colorButton, &QToolButton::clicked, this, &AnnotationDialog::pickColor);

    auto* undoButton = new QToolButton(this);
    undoButton->setText(tr("Annuler la dernière forme"));
    connect(undoButton, &QToolButton::clicked, this, &AnnotationDialog::undoLastShape);

    toolBar->addWidget(rectButton);
    toolBar->addWidget(circleButton);
    toolBar->addWidget(m_colorButton);
    toolBar->addStretch(1);
    toolBar->addWidget(undoButton);

    auto* bottomBar = new QHBoxLayout();
    bottomBar->addStretch(1);
    auto* saveButton = new QPushButton(tr("Enregistrer"), this);
    saveButton->setDefault(true);
    connect(saveButton, &QPushButton::clicked, this, &AnnotationDialog::saveAnnotations);
    auto* closeButton = new QPushButton(tr("Fermer"), this);
    connect(closeButton, &QPushButton::clicked, this, &QDialog::accept);
    bottomBar->addWidget(saveButton);
    bottomBar->addWidget(closeButton);

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(toolBar);
    layout->addWidget(m_view, 1);
    layout->addLayout(bottomBar);

    loadPage();
}

bool AnnotationDialog::loadPage() {
    if (m_previewDocument.load(m_filePath) != pdf::LoadResult::Ok) {
        QMessageBox::warning(this, tr("Impossible d'ouvrir"), tr("Le document n'a pas pu être chargé."));
        return false;
    }
    const QPixmap pixmap = QPixmap::fromImage(m_previewDocument.renderPage(m_pageIndex, kRenderWidthPx));
    if (pixmap.isNull()) {
        QMessageBox::warning(this, tr("Impossible d'ouvrir"), tr("La page n'a pas pu être rendue."));
        return false;
    }
    m_scene->clear();
    m_pendingShapes.clear();
    m_scene->addPixmap(pixmap);
    m_scene->setSceneRect(pixmap.rect());

    const QSizeF pageSize = m_previewDocument.pagePointSize(m_pageIndex);
    m_pointsPerScenePixel = pageSize.width() / pixmap.width();
    return true;
}

bool AnnotationDialog::eventFilter(QObject* watched, QEvent* event) {
    if (watched != m_view->viewport()) {
        return QDialog::eventFilter(watched, event);
    }

    if (event->type() == QEvent::MouseButtonPress) {
        auto* mouseEvent = static_cast<QMouseEvent*>(event);
        if (mouseEvent->button() != Qt::LeftButton) {
            return false;
        }
        m_dragStart = m_view->mapToScene(mouseEvent->pos());
        const QRectF zero(m_dragStart, QSizeF(0, 0));
        if (m_currentTool == Tool::Rectangle) {
            m_dragItem = m_scene->addRect(zero, QPen(m_currentColor, 2, Qt::DashLine));
        } else {
            m_dragItem = m_scene->addEllipse(zero, QPen(m_currentColor, 2, Qt::DashLine));
        }
        return true;
    }

    if (event->type() == QEvent::MouseMove && m_dragItem) {
        auto* mouseEvent = static_cast<QMouseEvent*>(event);
        const QRectF rect = QRectF(m_dragStart, m_view->mapToScene(mouseEvent->pos())).normalized();
        if (auto* rectItem = qgraphicsitem_cast<QGraphicsRectItem*>(m_dragItem)) {
            rectItem->setRect(rect);
        } else if (auto* ellipseItem = qgraphicsitem_cast<QGraphicsEllipseItem*>(m_dragItem)) {
            ellipseItem->setRect(rect);
        }
        return true;
    }

    if (event->type() == QEvent::MouseButtonRelease && m_dragItem) {
        auto* mouseEvent = static_cast<QMouseEvent*>(event);
        const QRectF rect = QRectF(m_dragStart, m_view->mapToScene(mouseEvent->pos())).normalized();
        if (rect.width() < kMinDragPx || rect.height() < kMinDragPx) {
            m_scene->removeItem(m_dragItem);
            delete m_dragItem;
        } else {
            const pdf::AnnotationShape shape =
                m_currentTool == Tool::Rectangle ? pdf::AnnotationShape::Rectangle : pdf::AnnotationShape::Circle;
            QColor fill = m_currentColor;
            fill.setAlpha(120);
            if (auto* rectItem = qgraphicsitem_cast<QGraphicsRectItem*>(m_dragItem)) {
                rectItem->setPen(QPen(Qt::black, 2));
                rectItem->setBrush(fill);
            } else if (auto* ellipseItem = qgraphicsitem_cast<QGraphicsEllipseItem*>(m_dragItem)) {
                ellipseItem->setPen(QPen(Qt::black, 2));
                ellipseItem->setBrush(fill);
            }
            m_pendingShapes.append(PendingShape{shape, sceneRectToPdf(rect), m_currentColor, m_dragItem});
        }
        m_dragItem = nullptr;
        return true;
    }

    return false;
}

QRectF AnnotationDialog::sceneRectToPdf(const QRectF& sceneRect) const {
    // Scene coordinates are page-render pixel coordinates: origin top-left,
    // y down — the same top-down convention AnnotationWriter expects, so a
    // uniform scale is all that's needed (no vertical flip here).
    return QRectF(sceneRect.left() * m_pointsPerScenePixel, sceneRect.top() * m_pointsPerScenePixel,
                  sceneRect.width() * m_pointsPerScenePixel, sceneRect.height() * m_pointsPerScenePixel);
}

void AnnotationDialog::pickColor() {
    const QColor chosen = QColorDialog::getColor(m_currentColor, this, tr("Choisir une couleur"));
    if (chosen.isValid()) {
        m_currentColor = chosen;
        m_colorButton->setStyleSheet(QStringLiteral("background-color: %1").arg(m_currentColor.name()));
    }
}

void AnnotationDialog::undoLastShape() {
    if (m_pendingShapes.isEmpty()) {
        return;
    }
    const PendingShape last = m_pendingShapes.takeLast();
    m_scene->removeItem(last.graphicsItem);
    delete last.graphicsItem;
}

void AnnotationDialog::saveAnnotations() {
    if (m_pendingShapes.isEmpty()) {
        QMessageBox::information(this, tr("Rien à enregistrer"), tr("Ajoutez au moins une forme d'abord."));
        return;
    }
    if (QMessageBox::question(this, tr("Enregistrer"),
                               tr("Ajouter %1 forme(s) à « %2 » ?")
                                   .arg(m_pendingShapes.size())
                                   .arg(QFileInfo(m_filePath).fileName())) != QMessageBox::Yes) {
        return;
    }

    pdf::AnnotationWriter writer;
    if (writer.load(m_filePath) != pdf::EditResult::Ok) {
        QMessageBox::warning(this, tr("Échec"), tr("Impossible de charger le document."));
        return;
    }
    for (const PendingShape& shape : m_pendingShapes) {
        writer.addShape(m_pageIndex, shape.shape, shape.pdfRect, shape.color, 2.0);
    }

    const QString tempPath = m_filePath + QStringLiteral(".papyrus-tmp");
    if (writer.save(tempPath) != pdf::EditResult::Ok) {
        QMessageBox::warning(this, tr("Échec"), tr("L'enregistrement a échoué."));
        QFile::remove(tempPath);
        return;
    }
    QFile::remove(m_filePath);
    if (!QFile::rename(tempPath, m_filePath)) {
        QMessageBox::warning(this, tr("Échec"), tr("Impossible de remplacer le fichier d'origine."));
        return;
    }

    m_pendingShapes.clear();
    emit documentSaved(m_filePath);
    QMessageBox::information(this, tr("Enregistré"), tr("Annotations ajoutées avec succès."));
}

} // namespace papyrus
