#include "papyrus/pdf/document.h"

#include <QPdfDocument>

namespace papyrus::pdf {

Document::Document() : m_doc(std::make_unique<QPdfDocument>()) {}

Document::~Document() = default;

LoadResult Document::load(const QString& filePath) {
    const auto error = m_doc->load(filePath);
    switch (error) {
    case QPdfDocument::Error::None:
        m_filePath = filePath;
        return LoadResult::Ok;
    case QPdfDocument::Error::FileNotFound:
        return LoadResult::FileNotFound;
    case QPdfDocument::Error::InvalidFileFormat:
        return LoadResult::InvalidFormat;
    case QPdfDocument::Error::IncorrectPassword:
        return LoadResult::PasswordProtected;
    default:
        return LoadResult::Unknown;
    }
}

void Document::close() {
    m_doc->close();
    m_filePath.clear();
}

bool Document::isValid() const {
    return m_doc->status() == QPdfDocument::Status::Ready;
}

QString Document::filePath() const {
    return m_filePath;
}

int Document::pageCount() const {
    return m_doc->pageCount();
}

QSizeF Document::pagePointSize(int pageIndex) const {
    return m_doc->pagePointSize(pageIndex);
}

QImage Document::renderPage(int pageIndex, int targetWidthPx) const {
    const QSizeF points = pagePointSize(pageIndex);
    if (points.isEmpty() || targetWidthPx <= 0) {
        return {};
    }
    const qreal scale = targetWidthPx / points.width();
    const QSize imageSize(targetWidthPx, qRound(points.height() * scale));
    return m_doc->render(pageIndex, imageSize);
}

QPdfDocument* Document::qtDocument() const {
    return m_doc.get();
}

} // namespace papyrus::pdf
