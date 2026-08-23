#pragma once

#include <QColor>
#include <QDialog>

QT_BEGIN_NAMESPACE
class QFontComboBox;
class QSpinBox;
class QDoubleSpinBox;
class QComboBox;
class QToolButton;
class QLabel;
QT_END_NAMESPACE

namespace papyrus {

// "Create PDF from text file" dialog: pick a .txt source, choose layout
// options, write the resulting PDF. See papyrus::conversion::TextToPdfOptions
// for what's covered (no header/footer support yet).
class TextToPdfDialog : public QDialog {
    Q_OBJECT
public:
    explicit TextToPdfDialog(QWidget* parent = nullptr);

signals:
    void pdfCreated(const QString& filePath);

private:
    void pickSourceFile();
    void pickColor();
    void createPdf();

    QString m_sourcePath;
    QLabel* m_sourceLabel;
    QFontComboBox* m_fontCombo;
    QSpinBox* m_sizeSpin;
    QToolButton* m_colorButton;
    QColor m_color{0, 0, 0};
    QComboBox* m_alignmentCombo;
    QComboBox* m_pageSizeCombo;
    QComboBox* m_orientationCombo;
    QDoubleSpinBox* m_marginsSpin;
    QDoubleSpinBox* m_lineSpacingSpin;
};

} // namespace papyrus
