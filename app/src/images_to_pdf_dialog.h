#pragma once

#include <QDialog>

QT_BEGIN_NAMESPACE
class QListWidget;
class QComboBox;
class QCheckBox;
QT_END_NAMESPACE

namespace papyrus {

// "Create PDF from images" dialog: pick images (one per page, in list
// order), reorder with move-up/down, choose page size/orientation/aspect
// handling, write the resulting PDF.
class ImagesToPdfDialog : public QDialog {
    Q_OBJECT
public:
    explicit ImagesToPdfDialog(QWidget* parent = nullptr);

signals:
    void pdfCreated(const QString& filePath);

private:
    void addImages();
    void removeSelected();
    void moveSelected(int direction);
    void createPdf();

    QListWidget* m_list;
    QComboBox* m_pageSizeCombo;
    QComboBox* m_orientationCombo;
    QCheckBox* m_keepAspectCheck;
};

} // namespace papyrus
