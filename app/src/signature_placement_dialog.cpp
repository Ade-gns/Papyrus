#include "signature_placement_dialog.h"

#include "papyrus/pdf/annotation_writer.h"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileInfo>
#include <QGraphicsPixmapItem>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPainter>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QVBoxLayout>

#include <algorithm>

namespace papyrus {

namespace {
constexpr int kRenderWidthPx = 900;
}

SignaturePlacementDialog::SignaturePlacementDialog(const QString& filePath, int initialPageIndex,
                                                     const QImage& signatureImage, QWidget* parent)
    : QDialog(parent),
      m_filePath(filePath),
      m_pageIndex(initialPageIndex),
      m_signatureImage(signatureImage),
      m_view(new QGraphicsView(this)),
      m_scene(new QGraphicsScene(this)),
      m_pageCombo(new QComboBox(this)),
      m_sizeSpin(new QDoubleSpinBox(this)),
      m_rotationSpin(new QSpinBox(this)) {
    setWindowTitle(tr("Placer la signature — %1").arg(QFileInfo(filePath).fileName()));
    resize(800, 700);

    m_view->setScene(m_scene);
    m_view->setRenderHint(QPainter::Antialiasing);

    if (m_previewDocument.load(filePath) == pdf::LoadResult::Ok) {
        for (int i = 0; i < m_previewDocument.pageCount(); ++i) {
            m_pageCombo->addItem(tr("Page %1").arg(i + 1), i);
        }
        m_pageCombo->setCurrentIndex(initialPageIndex);
    }
    connect(m_pageCombo, &QComboBox::currentIndexChanged, this,
            [this](int) { loadPage(m_pageCombo->currentData().toInt()); });

    m_sizeSpin->setRange(5, 400);
    m_sizeSpin->setValue(100);
    m_sizeSpin->setSuffix(tr(" %"));
    connect(m_sizeSpin, &QDoubleSpinBox::valueChanged, this, &SignaturePlacementDialog::updateItemTransform);

    m_rotationSpin->setRange(-180, 180);
    m_rotationSpin->setValue(0);
    m_rotationSpin->setSuffix(tr(" °"));
    connect(m_rotationSpin, &QSpinBox::valueChanged, this, &SignaturePlacementDialog::updateItemTransform);

    auto* infoLabel = new QLabel(
        tr("Signature visuelle : une image est insérée dans le document. Pour une signature "
           "électronique certifiée (certificat), une fonctionnalité dédiée arrivera plus tard."),
        this);
    infoLabel->setWordWrap(true);
    infoLabel->setStyleSheet(QStringLiteral("color: #666; font-style: italic;"));

    auto* controlsRow = new QHBoxLayout();
    controlsRow->addWidget(new QLabel(tr("Page :"), this));
    controlsRow->addWidget(m_pageCombo);
    controlsRow->addWidget(new QLabel(tr("Taille :"), this));
    controlsRow->addWidget(m_sizeSpin);
    controlsRow->addWidget(new QLabel(tr("Rotation :"), this));
    controlsRow->addWidget(m_rotationSpin);
    controlsRow->addStretch(1);

    auto* placeButton = new QPushButton(tr("Placer sur le document"), this);
    placeButton->setDefault(true);
    connect(placeButton, &QPushButton::clicked, this, &SignaturePlacementDialog::place);
    auto* cancelButton = new QPushButton(tr("Annuler"), this);
    connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    auto* bottomRow = new QHBoxLayout();
    bottomRow->addStretch(1);
    bottomRow->addWidget(cancelButton);
    bottomRow->addWidget(placeButton);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(infoLabel);
    layout->addLayout(controlsRow);
    layout->addWidget(m_view, 1);
    layout->addLayout(bottomRow);

    loadPage(initialPageIndex);
}

void SignaturePlacementDialog::loadPage(int pageIndex) {
    m_pageIndex = pageIndex;
    const QPixmap background = QPixmap::fromImage(m_previewDocument.renderPage(pageIndex, kRenderWidthPx));
    if (background.isNull()) {
        return;
    }

    m_scene->clear();
    m_signatureItem = nullptr;
    m_scene->addPixmap(background);
    m_scene->setSceneRect(background.rect());

    const QSizeF pageSize = m_previewDocument.pagePointSize(pageIndex);
    m_pointsPerScenePixel = pageSize.width() / background.width();

    const QPixmap signaturePixmap = QPixmap::fromImage(m_signatureImage);
    m_signatureItem = m_scene->addPixmap(signaturePixmap);
    m_signatureItem->setFlags(QGraphicsItem::ItemIsMovable | QGraphicsItem::ItemIsSelectable);
    m_signatureItem->setTransformOriginPoint(signaturePixmap.width() / 2.0, signaturePixmap.height() / 2.0);

    // Default to roughly a third of the page width, capped at the
    // signature's natural size so small drawings aren't blown up.
    const double naturalScalePercent = 100.0;
    const double fitScalePercent = (background.width() / 3.0) / signaturePixmap.width() * 100.0;
    const QSignalBlocker blocker(m_sizeSpin);
    m_sizeSpin->setValue(std::min(naturalScalePercent, fitScalePercent));

    const QPointF center = background.rect().center();
    m_signatureItem->setPos(center.x() - signaturePixmap.width() / 2.0,
                             center.y() - signaturePixmap.height() / 2.0);
    updateItemTransform();
}

void SignaturePlacementDialog::updateItemTransform() {
    if (!m_signatureItem) {
        return;
    }
    m_signatureItem->setScale(m_sizeSpin->value() / 100.0);
    m_signatureItem->setRotation(m_rotationSpin->value());
}

void SignaturePlacementDialog::place() {
    if (!m_signatureItem) {
        return;
    }
    const QPointF center = m_signatureItem->sceneBoundingRect().center();
    const QPixmap pixmap = m_signatureItem->pixmap();
    const double scale = m_signatureItem->scale();
    const double width = pixmap.width() * scale;
    const double height = pixmap.height() * scale;
    const QRectF sceneRect(center.x() - width / 2, center.y() - height / 2, width, height);
    const QRectF pdfRect(sceneRect.left() * m_pointsPerScenePixel, sceneRect.top() * m_pointsPerScenePixel,
                          sceneRect.width() * m_pointsPerScenePixel, sceneRect.height() * m_pointsPerScenePixel);

    pdf::AnnotationWriter writer;
    if (writer.load(m_filePath) != pdf::EditResult::Ok ||
        !writer.addImage(m_pageIndex, m_signatureImage, pdfRect, m_signatureItem->rotation())) {
        QMessageBox::warning(this, tr("Échec"), tr("Impossible de placer la signature."));
        return;
    }

    const QString tempPath = m_filePath + QStringLiteral(".papyrus-tmp");
    if (writer.save(tempPath) != pdf::EditResult::Ok) {
        QFile::remove(tempPath);
        QMessageBox::warning(this, tr("Échec"), tr("L'enregistrement a échoué."));
        return;
    }
    QFile::remove(m_filePath);
    if (!QFile::rename(tempPath, m_filePath)) {
        QMessageBox::warning(this, tr("Échec"), tr("Impossible de remplacer le fichier d'origine."));
        return;
    }

    emit documentSaved(m_filePath);
    accept();
}

} // namespace papyrus
