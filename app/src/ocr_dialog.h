#pragma once

#include <QDialog>

#include <memory>

QT_BEGIN_NAMESPACE
class QComboBox;
class QLabel;
class QProgressBar;
class QPushButton;
QT_END_NAMESPACE

namespace papyrus::ocr {
class OcrJob;
}

namespace papyrus {

// "Make this document searchable" dialog: pick an OCR language, run
// Tesseract over every page (see papyrus::ocr::OcrJob), offer to open the
// result.
class OcrDialog : public QDialog {
    Q_OBJECT
public:
    explicit OcrDialog(const QString& filePath, QWidget* parent = nullptr);
    ~OcrDialog() override;

signals:
    void pdfCreated(const QString& filePath);

private:
    void startOcr();

    QString m_filePath;
    QComboBox* m_languageCombo;
    QLabel* m_statusLabel;
    QProgressBar* m_progressBar;
    QPushButton* m_startButton;
    QPushButton* m_closeButton;

    std::unique_ptr<ocr::OcrJob> m_job;
};

} // namespace papyrus
