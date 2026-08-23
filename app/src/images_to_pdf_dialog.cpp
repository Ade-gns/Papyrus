#include "images_to_pdf_dialog.h"

#include "papyrus/conversion/images_to_pdf.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

namespace papyrus {

ImagesToPdfDialog::ImagesToPdfDialog(QWidget* parent)
    : QDialog(parent),
      m_list(new QListWidget(this)),
      m_pageSizeCombo(new QComboBox(this)),
      m_orientationCombo(new QComboBox(this)),
      m_keepAspectCheck(new QCheckBox(tr("Conserver les proportions (centré)"), this)) {
    setWindowTitle(tr("Créer un PDF depuis des images"));
    resize(600, 500);

    m_list->setSelectionMode(QAbstractItemView::ExtendedSelection);

    auto* listButtons = new QHBoxLayout();
    auto* addButton = new QPushButton(tr("Ajouter des images..."), this);
    connect(addButton, &QPushButton::clicked, this, &ImagesToPdfDialog::addImages);
    auto* removeButton = new QPushButton(tr("Supprimer"), this);
    connect(removeButton, &QPushButton::clicked, this, &ImagesToPdfDialog::removeSelected);
    auto* upButton = new QPushButton(tr("▲ Monter"), this);
    connect(upButton, &QPushButton::clicked, this, [this] { moveSelected(-1); });
    auto* downButton = new QPushButton(tr("▼ Descendre"), this);
    connect(downButton, &QPushButton::clicked, this, [this] { moveSelected(1); });
    listButtons->addWidget(addButton);
    listButtons->addWidget(removeButton);
    listButtons->addStretch(1);
    listButtons->addWidget(upButton);
    listButtons->addWidget(downButton);

    m_pageSizeCombo->addItem(tr("A4"), static_cast<int>(QPageSize::A4));
    m_pageSizeCombo->addItem(tr("Letter"), static_cast<int>(QPageSize::Letter));
    m_orientationCombo->addItem(tr("Portrait"), static_cast<int>(QPageLayout::Portrait));
    m_orientationCombo->addItem(tr("Paysage"), static_cast<int>(QPageLayout::Landscape));
    m_keepAspectCheck->setChecked(true);

    auto* optionsRow = new QHBoxLayout();
    optionsRow->addWidget(new QLabel(tr("Format :"), this));
    optionsRow->addWidget(m_pageSizeCombo);
    optionsRow->addWidget(new QLabel(tr("Orientation :"), this));
    optionsRow->addWidget(m_orientationCombo);
    optionsRow->addWidget(m_keepAspectCheck);
    optionsRow->addStretch(1);

    auto* createButton = new QPushButton(tr("Créer le PDF..."), this);
    createButton->setDefault(true);
    connect(createButton, &QPushButton::clicked, this, &ImagesToPdfDialog::createPdf);
    auto* cancelButton = new QPushButton(tr("Annuler"), this);
    connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    auto* bottomRow = new QHBoxLayout();
    bottomRow->addStretch(1);
    bottomRow->addWidget(cancelButton);
    bottomRow->addWidget(createButton);

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(listButtons);
    layout->addWidget(m_list, 1);
    layout->addLayout(optionsRow);
    layout->addLayout(bottomRow);
}

void ImagesToPdfDialog::addImages() {
    const QStringList paths = QFileDialog::getOpenFileNames(
        this, tr("Choisir des images"), {}, tr("Images (*.png *.jpg *.jpeg *.webp *.bmp)"));
    for (const QString& path : paths) {
        auto* item = new QListWidgetItem(QFileInfo(path).fileName());
        item->setData(Qt::UserRole, path);
        item->setToolTip(path);
        m_list->addItem(item);
    }
}

void ImagesToPdfDialog::removeSelected() {
    for (QListWidgetItem* item : m_list->selectedItems()) {
        delete m_list->takeItem(m_list->row(item));
    }
}

void ImagesToPdfDialog::moveSelected(int direction) {
    const QList<QListWidgetItem*> selected = m_list->selectedItems();
    if (selected.size() != 1) {
        return;
    }
    const int row = m_list->row(selected.first());
    const int target = row + direction;
    if (target < 0 || target >= m_list->count()) {
        return;
    }
    QListWidgetItem* item = m_list->takeItem(row);
    m_list->insertItem(target, item);
    m_list->setCurrentItem(item);
}

void ImagesToPdfDialog::createPdf() {
    if (m_list->count() == 0) {
        QMessageBox::warning(this, tr("Aucune image"), tr("Ajoutez au moins une image."));
        return;
    }
    const QString outputPath =
        QFileDialog::getSaveFileName(this, tr("Enregistrer le PDF"), {}, tr("PDF (*.pdf)"));
    if (outputPath.isEmpty()) {
        return;
    }

    QStringList paths;
    for (int i = 0; i < m_list->count(); ++i) {
        paths.append(m_list->item(i)->data(Qt::UserRole).toString());
    }

    conversion::ImagesToPdfOptions options;
    options.pageSize = QPageSize(static_cast<QPageSize::PageSizeId>(m_pageSizeCombo->currentData().toInt()));
    options.orientation = static_cast<QPageLayout::Orientation>(m_orientationCombo->currentData().toInt());
    options.keepAspectRatio = m_keepAspectCheck->isChecked();

    const auto result = conversion::convertImagesToPdf(paths, options, outputPath);
    if (result != conversion::ConversionResult::Ok) {
        QMessageBox::warning(this, tr("Échec"), tr("La conversion a échoué."));
        return;
    }
    emit pdfCreated(outputPath);
    accept();
}

} // namespace papyrus
