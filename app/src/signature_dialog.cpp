#include "signature_dialog.h"

#include "signature_pad_widget.h"
#include "signature_placement_dialog.h"
#include "signature_store.h"
#include "typed_signature_widget.h"

#include <QCheckBox>
#include <QColorDialog>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QTabWidget>
#include <QToolButton>
#include <QVBoxLayout>

namespace papyrus {

namespace {

// Simple threshold-based background removal: near-white pixels become
// transparent. Good enough for a typical signature photographed/scanned on
// plain paper; not a general-purpose background remover.
QImage removeWhiteBackground(const QImage& input, int threshold = 235) {
    QImage result = input.convertToFormat(QImage::Format_ARGB32);
    for (int y = 0; y < result.height(); ++y) {
        auto* line = reinterpret_cast<QRgb*>(result.scanLine(y));
        for (int x = 0; x < result.width(); ++x) {
            const QRgb pixel = line[x];
            if (qRed(pixel) >= threshold && qGreen(pixel) >= threshold && qBlue(pixel) >= threshold) {
                line[x] = qRgba(qRed(pixel), qGreen(pixel), qBlue(pixel), 0);
            }
        }
    }
    return result;
}

QLabel* scaledPreviewLabel(QWidget* parent) {
    auto* label = new QLabel(parent);
    label->setMinimumHeight(150);
    label->setAlignment(Qt::AlignCenter);
    label->setStyleSheet(QStringLiteral("background-color: white; border: 1px solid #ccc;"));
    return label;
}

} // namespace

SignatureDialog::SignatureDialog(const QString& documentFilePath, int currentPageIndex, QWidget* parent)
    : QDialog(parent),
      m_documentFilePath(documentFilePath),
      m_pageIndex(currentPageIndex),
      m_signatureList(new QListWidget(this)),
      m_padWidget(new SignaturePadWidget(this)),
      m_typedWidget(new TypedSignatureWidget(this)),
      m_importPreviewLabel(scaledPreviewLabel(this)),
      m_removeBackgroundCheck(new QCheckBox(tr("Supprimer le fond blanc"), this)) {
    setWindowTitle(tr("Signer le document"));
    resize(950, 650);

    // --- Left column: saved signatures ---
    auto* leftColumn = new QVBoxLayout();
    leftColumn->addWidget(new QLabel(tr("Mes signatures"), this));
    m_signatureList->setIconSize(QSize(120, 60));
    leftColumn->addWidget(m_signatureList, 1);
    auto* useButton = new QPushButton(tr("Placer sur le document..."), this);
    connect(useButton, &QPushButton::clicked, this, &SignatureDialog::placeSelected);
    auto* deleteButton = new QPushButton(tr("Supprimer"), this);
    connect(deleteButton, &QPushButton::clicked, this, &SignatureDialog::removeSelected);
    leftColumn->addWidget(useButton);
    leftColumn->addWidget(deleteButton);

    // --- Right column: creation tabs ---
    auto* tabs = new QTabWidget(this);

    // Tab: draw
    auto* drawTab = new QWidget(this);
    auto* drawLayout = new QVBoxLayout(drawTab);
    auto* drawToolbar = new QHBoxLayout();
    auto* widthSpin = new QSpinBox(drawTab);
    widthSpin->setRange(1, 15);
    widthSpin->setValue(3);
    connect(widthSpin, &QSpinBox::valueChanged, m_padWidget, &SignaturePadWidget::setPenWidth);
    auto* colorButton = new QToolButton(drawTab);
    colorButton->setText(tr("Couleur"));
    colorButton->setStyleSheet("background-color: black");
    connect(colorButton, &QToolButton::clicked, this, [this, colorButton] {
        const QColor chosen = QColorDialog::getColor(Qt::black, this, tr("Couleur du trait"));
        if (chosen.isValid()) {
            m_padWidget->setPenColor(chosen);
            colorButton->setStyleSheet(QStringLiteral("background-color: %1").arg(chosen.name()));
        }
    });
    auto* undoButton = new QToolButton(drawTab);
    undoButton->setText(tr("Annuler"));
    connect(undoButton, &QToolButton::clicked, m_padWidget, &SignaturePadWidget::undo);
    auto* redoButton = new QToolButton(drawTab);
    redoButton->setText(tr("Rétablir"));
    connect(redoButton, &QToolButton::clicked, m_padWidget, &SignaturePadWidget::redo);
    auto* clearButton = new QToolButton(drawTab);
    clearButton->setText(tr("Effacer"));
    connect(clearButton, &QToolButton::clicked, m_padWidget, &SignaturePadWidget::clear);
    drawToolbar->addWidget(new QLabel(tr("Épaisseur"), drawTab));
    drawToolbar->addWidget(widthSpin);
    drawToolbar->addWidget(colorButton);
    drawToolbar->addStretch(1);
    drawToolbar->addWidget(undoButton);
    drawToolbar->addWidget(redoButton);
    drawToolbar->addWidget(clearButton);
    auto* saveDrawnButton = new QPushButton(tr("Enregistrer cette signature"), drawTab);
    connect(saveDrawnButton, &QPushButton::clicked, this, &SignatureDialog::saveDrawn);
    drawLayout->addLayout(drawToolbar);
    drawLayout->addWidget(m_padWidget, 1);
    drawLayout->addWidget(saveDrawnButton);
    tabs->addTab(drawTab, tr("Dessiner"));

    // Tab: import
    auto* importTab = new QWidget(this);
    auto* importLayout = new QVBoxLayout(importTab);
    auto* pickButton = new QPushButton(tr("Choisir une image..."), importTab);
    connect(pickButton, &QPushButton::clicked, this, &SignatureDialog::pickImportFile);
    m_removeBackgroundCheck->setChecked(true);
    connect(m_removeBackgroundCheck, &QCheckBox::toggled, this, &SignatureDialog::updateImportPreview);
    auto* saveImportedButton = new QPushButton(tr("Enregistrer cette signature"), importTab);
    connect(saveImportedButton, &QPushButton::clicked, this, &SignatureDialog::saveImported);
    importLayout->addWidget(pickButton);
    importLayout->addWidget(m_removeBackgroundCheck);
    importLayout->addWidget(m_importPreviewLabel, 1);
    importLayout->addWidget(saveImportedButton);
    tabs->addTab(importTab, tr("Importer"));

    // Tab: type
    auto* typeTab = new QWidget(this);
    auto* typeLayout = new QVBoxLayout(typeTab);
    auto* saveTypedButton = new QPushButton(tr("Enregistrer cette signature"), typeTab);
    connect(saveTypedButton, &QPushButton::clicked, this, &SignatureDialog::saveTyped);
    typeLayout->addWidget(m_typedWidget, 1);
    typeLayout->addWidget(saveTypedButton);
    tabs->addTab(typeTab, tr("Écrire"));

    auto* closeButton = new QPushButton(tr("Fermer"), this);
    connect(closeButton, &QPushButton::clicked, this, &QDialog::accept);
    auto* bottomRow = new QHBoxLayout();
    bottomRow->addStretch(1);
    bottomRow->addWidget(closeButton);

    auto* rightColumn = new QVBoxLayout();
    rightColumn->addWidget(tabs, 1);
    rightColumn->addLayout(bottomRow);

    auto* mainRow = new QHBoxLayout(this);
    auto* leftContainer = new QWidget(this);
    leftContainer->setLayout(leftColumn);
    leftContainer->setMaximumWidth(260);
    mainRow->addWidget(leftContainer);
    mainRow->addLayout(rightColumn, 1);

    refreshSignatureList();
}

void SignatureDialog::refreshSignatureList() {
    m_signatureList->clear();
    for (const SignatureStore::Entry& entry : SignatureStore::list()) {
        const QImage image(entry.imagePath);
        auto* item = new QListWidgetItem(entry.name);
        item->setIcon(QIcon(QPixmap::fromImage(image)));
        item->setData(Qt::UserRole, entry.imagePath);
        m_signatureList->addItem(item);
    }
}

void SignatureDialog::promptAndSave(const QImage& image, const QString& suggestedName) {
    if (image.isNull()) {
        QMessageBox::warning(this, tr("Rien à enregistrer"), tr("Créez d'abord une signature."));
        return;
    }
    bool ok = false;
    const QString name =
        QInputDialog::getText(this, tr("Nom de la signature"), tr("Nom :"), QLineEdit::Normal, suggestedName, &ok);
    if (!ok || name.trimmed().isEmpty()) {
        return;
    }
    if (SignatureStore::save(name.trimmed(), image)) {
        refreshSignatureList();
    } else {
        QMessageBox::warning(this, tr("Échec"), tr("Impossible d'enregistrer la signature."));
    }
}

void SignatureDialog::saveDrawn() {
    if (m_padWidget->isEmpty()) {
        QMessageBox::warning(this, tr("Rien à enregistrer"), tr("Dessinez d'abord une signature."));
        return;
    }
    promptAndSave(m_padWidget->exportImage(), tr("Signature manuscrite"));
}

void SignatureDialog::pickImportFile() {
    const QString path = QFileDialog::getOpenFileName(this, tr("Choisir une image de signature"), {},
                                                        tr("Images (*.png *.jpg *.jpeg *.bmp)"));
    if (path.isEmpty()) {
        return;
    }
    m_importedRawImage = QImage(path);
    m_importedBaseName = QFileInfo(path).completeBaseName();
    updateImportPreview();
}

void SignatureDialog::updateImportPreview() {
    if (m_importedRawImage.isNull()) {
        return;
    }
    const QImage processed = m_removeBackgroundCheck->isChecked() ? removeWhiteBackground(m_importedRawImage)
                                                                    : m_importedRawImage;
    m_importPreviewLabel->setPixmap(QPixmap::fromImage(processed).scaled(
        m_importPreviewLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

void SignatureDialog::saveImported() {
    if (m_importedRawImage.isNull()) {
        QMessageBox::warning(this, tr("Aucune image"), tr("Choisissez d'abord une image."));
        return;
    }
    const QImage processed =
        m_removeBackgroundCheck->isChecked() ? removeWhiteBackground(m_importedRawImage) : m_importedRawImage;
    promptAndSave(processed, m_importedBaseName);
}

void SignatureDialog::saveTyped() {
    if (m_typedWidget->isEmpty()) {
        QMessageBox::warning(this, tr("Rien à enregistrer"), tr("Écrivez d'abord votre nom."));
        return;
    }
    promptAndSave(m_typedWidget->renderImage(), m_typedWidget->suggestedName());
}

void SignatureDialog::removeSelected() {
    QListWidgetItem* item = m_signatureList->currentItem();
    if (!item) {
        return;
    }
    if (SignatureStore::remove(item->text())) {
        refreshSignatureList();
    }
}

void SignatureDialog::placeSelected() {
    QListWidgetItem* item = m_signatureList->currentItem();
    if (!item) {
        QMessageBox::information(this, tr("Aucune sélection"),
                                  tr("Choisissez une signature dans la liste, ou créez-en une."));
        return;
    }
    const QImage image(item->data(Qt::UserRole).toString());
    if (image.isNull()) {
        return;
    }
    auto* dialog = new SignaturePlacementDialog(m_documentFilePath, m_pageIndex, image, this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    connect(dialog, &SignaturePlacementDialog::documentSaved, this, &SignatureDialog::documentSaved);
    dialog->exec();
}

} // namespace papyrus
