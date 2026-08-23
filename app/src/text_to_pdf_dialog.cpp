#include "text_to_pdf_dialog.h"

#include "papyrus/conversion/text_to_pdf.h"

#include <QColorDialog>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QToolButton>
#include <QVBoxLayout>

namespace papyrus {

TextToPdfDialog::TextToPdfDialog(QWidget* parent)
    : QDialog(parent),
      m_sourceLabel(new QLabel(tr("(aucun fichier choisi)"), this)),
      m_fontCombo(new QFontComboBox(this)),
      m_sizeSpin(new QSpinBox(this)),
      m_colorButton(new QToolButton(this)),
      m_alignmentCombo(new QComboBox(this)),
      m_pageSizeCombo(new QComboBox(this)),
      m_orientationCombo(new QComboBox(this)),
      m_marginsSpin(new QDoubleSpinBox(this)),
      m_lineSpacingSpin(new QDoubleSpinBox(this)) {
    setWindowTitle(tr("Créer un PDF depuis un texte"));

    auto* sourceButton = new QPushButton(tr("Choisir le fichier texte..."), this);
    connect(sourceButton, &QPushButton::clicked, this, &TextToPdfDialog::pickSourceFile);

    m_sizeSpin->setRange(6, 96);
    m_sizeSpin->setValue(11);

    m_colorButton->setText(tr("Couleur"));
    m_colorButton->setStyleSheet(QStringLiteral("background-color: %1").arg(m_color.name()));
    connect(m_colorButton, &QToolButton::clicked, this, &TextToPdfDialog::pickColor);

    m_alignmentCombo->addItem(tr("Gauche"), static_cast<int>(Qt::AlignLeft));
    m_alignmentCombo->addItem(tr("Centré"), static_cast<int>(Qt::AlignHCenter));
    m_alignmentCombo->addItem(tr("Droite"), static_cast<int>(Qt::AlignRight));
    m_alignmentCombo->addItem(tr("Justifié"), static_cast<int>(Qt::AlignJustify));

    m_pageSizeCombo->addItem(tr("A4"), static_cast<int>(QPageSize::A4));
    m_pageSizeCombo->addItem(tr("Letter"), static_cast<int>(QPageSize::Letter));

    m_orientationCombo->addItem(tr("Portrait"), static_cast<int>(QPageLayout::Portrait));
    m_orientationCombo->addItem(tr("Paysage"), static_cast<int>(QPageLayout::Landscape));

    m_marginsSpin->setRange(0, 100);
    m_marginsSpin->setValue(20);
    m_marginsSpin->setSuffix(tr(" mm"));

    m_lineSpacingSpin->setRange(50, 300);
    m_lineSpacingSpin->setValue(100);
    m_lineSpacingSpin->setSuffix(tr(" %"));

    auto* form = new QFormLayout();
    form->addRow(sourceButton, m_sourceLabel);
    form->addRow(tr("Police"), m_fontCombo);
    form->addRow(tr("Taille"), m_sizeSpin);
    form->addRow(tr("Couleur du texte"), m_colorButton);
    form->addRow(tr("Alignement"), m_alignmentCombo);
    form->addRow(tr("Format de page"), m_pageSizeCombo);
    form->addRow(tr("Orientation"), m_orientationCombo);
    form->addRow(tr("Marges"), m_marginsSpin);
    form->addRow(tr("Interligne"), m_lineSpacingSpin);

    auto* createButton = new QPushButton(tr("Créer le PDF..."), this);
    createButton->setDefault(true);
    connect(createButton, &QPushButton::clicked, this, &TextToPdfDialog::createPdf);
    auto* cancelButton = new QPushButton(tr("Annuler"), this);
    connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);

    auto* buttonRow = new QHBoxLayout();
    buttonRow->addStretch(1);
    buttonRow->addWidget(cancelButton);
    buttonRow->addWidget(createButton);

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addLayout(buttonRow);
}

void TextToPdfDialog::pickSourceFile() {
    const QString path =
        QFileDialog::getOpenFileName(this, tr("Choisir un fichier texte"), {}, tr("Fichiers texte (*.txt)"));
    if (!path.isEmpty()) {
        m_sourcePath = path;
        m_sourceLabel->setText(QFileInfo(path).fileName());
    }
}

void TextToPdfDialog::pickColor() {
    const QColor chosen = QColorDialog::getColor(m_color, this, tr("Choisir une couleur"));
    if (chosen.isValid()) {
        m_color = chosen;
        m_colorButton->setStyleSheet(QStringLiteral("background-color: %1").arg(m_color.name()));
    }
}

void TextToPdfDialog::createPdf() {
    if (m_sourcePath.isEmpty()) {
        QMessageBox::warning(this, tr("Aucun fichier"), tr("Choisissez d'abord un fichier texte."));
        return;
    }
    const QString outputPath =
        QFileDialog::getSaveFileName(this, tr("Enregistrer le PDF"), {}, tr("PDF (*.pdf)"));
    if (outputPath.isEmpty()) {
        return;
    }

    conversion::TextToPdfOptions options;
    options.fontFamily = m_fontCombo->currentFont().family();
    options.fontPointSize = m_sizeSpin->value();
    options.textColor = m_color;
    options.alignment = static_cast<Qt::Alignment>(m_alignmentCombo->currentData().toInt());
    options.pageSize = QPageSize(static_cast<QPageSize::PageSizeId>(m_pageSizeCombo->currentData().toInt()));
    options.orientation = static_cast<QPageLayout::Orientation>(m_orientationCombo->currentData().toInt());
    options.marginsMm = m_marginsSpin->value();
    options.lineSpacingPercent = m_lineSpacingSpin->value();

    const auto result = conversion::convertTextFileToPdf(m_sourcePath, options, outputPath);
    if (result != conversion::ConversionResult::Ok) {
        QMessageBox::warning(this, tr("Échec"), tr("La conversion a échoué."));
        return;
    }
    emit pdfCreated(outputPath);
    accept();
}

} // namespace papyrus
