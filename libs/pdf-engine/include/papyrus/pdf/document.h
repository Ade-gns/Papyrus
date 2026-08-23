#pragma once

#include <QImage>
#include <QSizeF>
#include <QString>

#include <memory>

QT_BEGIN_NAMESPACE
class QPdfDocument;
QT_END_NAMESPACE

namespace papyrus::pdf {

// Result of Document::load(). Kept as our own enum (rather than leaking
// QPdfDocument::Error everywhere) so the backend can be swapped later
// (e.g. for raw PDFium once we need write support) without touching callers.
enum class LoadResult {
    Ok,
    FileNotFound,
    InvalidFormat,
    PasswordProtected,
    Unknown,
};

// Thin wrapper around QPdfDocument. Stays free of any Widgets dependency so
// it can be used headlessly (thumbnails, conversion, batch jobs) as well as
// bound to a QPdfView in the UI layer.
class Document {
public:
    Document();
    ~Document();

    Document(const Document&) = delete;
    Document& operator=(const Document&) = delete;

    LoadResult load(const QString& filePath);
    void close();

    bool isValid() const;
    QString filePath() const;
    int pageCount() const;
    QSizeF pagePointSize(int pageIndex) const;

    // Renders a page to an image scaled to targetWidthPx, keeping aspect ratio.
    QImage renderPage(int pageIndex, int targetWidthPx) const;

    // Exposes the underlying QPdfDocument for binding to a QPdfView.
    QPdfDocument* qtDocument() const;

private:
    std::unique_ptr<QPdfDocument> m_doc;
    QString m_filePath;
};

} // namespace papyrus::pdf
