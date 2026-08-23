#pragma once

#include "papyrus/pdf/document.h"
#include "papyrus/pdf/thumbnail_cache.h"

#include <QWidget>

#include <memory>

QT_BEGIN_NAMESPACE
class QPdfView;
class QPdfSearchModel;
QT_END_NAMESPACE

namespace papyrus {

class SearchBar;

// One open document: owns the pdf-engine Document, a QPdfView to display it,
// and its thumbnail cache. Reusable inside a QTabWidget.
class DocumentTab : public QWidget {
    Q_OBJECT
public:
    explicit DocumentTab(QWidget* parent = nullptr);

    pdf::LoadResult load(const QString& filePath);

    QString filePath() const;
    QString displayName() const;
    int pageCount() const;
    int currentPage() const;
    qreal zoomFactor() const;

    pdf::Document* document();
    pdf::ThumbnailCache* thumbnailCache();
    QPdfView* view() const;

public slots:
    void goToPage(int pageIndex);
    void nextPage();
    void previousPage();
    void zoomIn();
    void zoomOut();
    void setZoomFactor(qreal factor);
    void fitToWidth();

    void toggleSearchBar();
    void closeSearchBar();
    void highlightCurrentSearchResult();

signals:
    void currentPageChanged(int pageIndex);
    void zoomFactorChanged(qreal factor);
    void annotationsSaved(const QString& filePath);

private:
    void onSearchTextChanged(const QString& text);
    void onSearchResultsChanged();
    void goToSearchResult(int index);
    void nextSearchResult();
    void previousSearchResult();

    pdf::Document m_document;
    std::unique_ptr<pdf::ThumbnailCache> m_thumbnailCache;
    QPdfView* m_view;
    QPdfSearchModel* m_searchModel;
    SearchBar* m_searchBar;
    int m_currentResultIndex = -1;
};

} // namespace papyrus
