#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

#include <memory>

namespace papyrus::ocr {

bool isTesseractAvailable();

// Installed Tesseract language codes (e.g. "fra", "eng"), excluding the
// "osd" (orientation/script detection) pseudo-language.
QStringList availableLanguages();

// Turns a scanned/image-based PDF into a searchable one: each page is
// rendered to a ~300dpi image, OCR'd with Tesseract's own `pdf` output mode
// (which produces a page PDF with an invisible text layer already
// correctly positioned over the image — far simpler and more reliable than
// hand-building that text layer ourselves), then all page PDFs are merged
// with PageEditor::mergeFiles.
//
// Runs on a background thread (QtConcurrent) so the UI stays responsive;
// connect to the signals before calling start().
class OcrJob : public QObject {
    Q_OBJECT
public:
    OcrJob(QString inputPdfPath, QString outputPdfPath, QString language, QObject* parent = nullptr);
    ~OcrJob() override;

    void start();
    void cancel(); // best-effort: takes effect between pages

signals:
    void progress(int currentPage, int totalPages);
    void finished(bool success, const QString& errorMessage);

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace papyrus::ocr
