#include "document_tab.h"

#include "search_bar.h"

#include "papyrus/pdf/annotation_writer.h"

#include <QFile>
#include <QFileInfo>
#include <QMessageBox>
#include <QPdfDocument>
#include <QPdfLink>
#include <QPdfPageNavigator>
#include <QPdfSearchModel>
#include <QPdfView>
#include <QVBoxLayout>

namespace papyrus {

namespace {
constexpr qreal kZoomStep = 1.25;
constexpr qreal kMinZoom = 0.1;
constexpr qreal kMaxZoom = 8.0;
} // namespace

DocumentTab::DocumentTab(QWidget* parent)
    : QWidget(parent),
      m_view(new QPdfView(this)),
      m_searchModel(new QPdfSearchModel(this)),
      m_searchBar(new SearchBar(this)) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    m_searchBar->hide();
    layout->addWidget(m_searchBar);
    layout->addWidget(m_view);

    m_view->setDocument(m_document.qtDocument());
    m_view->setPageMode(QPdfView::PageMode::MultiPage);
    m_view->setZoomMode(QPdfView::ZoomMode::FitToWidth);
    m_view->setSearchModel(m_searchModel);

    connect(m_view->pageNavigator(), &QPdfPageNavigator::currentPageChanged, this,
            &DocumentTab::currentPageChanged);
    connect(m_view, &QPdfView::zoomFactorChanged, this, &DocumentTab::zoomFactorChanged);

    connect(m_searchBar, &SearchBar::searchTextChanged, this, &DocumentTab::onSearchTextChanged);
    connect(m_searchBar, &SearchBar::nextRequested, this, &DocumentTab::nextSearchResult);
    connect(m_searchBar, &SearchBar::previousRequested, this, &DocumentTab::previousSearchResult);
    connect(m_searchBar, &SearchBar::closed, this, &DocumentTab::closeSearchBar);
    connect(m_searchBar, &SearchBar::highlightRequested, this, &DocumentTab::highlightCurrentSearchResult);
    connect(m_searchModel, &QPdfSearchModel::countChanged, this, &DocumentTab::onSearchResultsChanged);
}

DocumentTab::~DocumentTab() {
    // Tearing down m_document below closes the underlying QPdfDocument,
    // which re-emits currentPageChanged (via QPdfPageNavigator) as it resets
    // to an invalid page. If that reaches MainWindow's per-tab lambda while
    // this tab is being destroyed as part of the whole window closing, it
    // calls back into the QTabWidget mid-teardown and crashes (found via a
    // real SIGSEGV in QStackedLayout::currentWidget() on app exit). Blocking
    // signals here keeps every member destructor below from emitting.
    blockSignals(true);
}

pdf::LoadResult DocumentTab::load(const QString& filePath) {
    const auto result = m_document.load(filePath);
    if (result == pdf::LoadResult::Ok) {
        m_thumbnailCache = std::make_unique<pdf::ThumbnailCache>(&m_document, this);
        m_searchModel->setDocument(m_document.qtDocument());
    }
    return result;
}

QString DocumentTab::filePath() const {
    return m_document.filePath();
}

QString DocumentTab::displayName() const {
    return QFileInfo(m_document.filePath()).fileName();
}

int DocumentTab::pageCount() const {
    return m_document.pageCount();
}

int DocumentTab::currentPage() const {
    return m_view->pageNavigator()->currentPage();
}

qreal DocumentTab::zoomFactor() const {
    return m_view->zoomFactor();
}

pdf::Document* DocumentTab::document() {
    return &m_document;
}

pdf::ThumbnailCache* DocumentTab::thumbnailCache() {
    return m_thumbnailCache.get();
}

QPdfView* DocumentTab::view() const {
    return m_view;
}

void DocumentTab::goToPage(int pageIndex) {
    if (pageIndex < 0 || pageIndex >= pageCount()) {
        return;
    }
    m_view->pageNavigator()->jump(pageIndex, QPointF(), m_view->zoomFactor());
}

void DocumentTab::nextPage() {
    goToPage(currentPage() + 1);
}

void DocumentTab::previousPage() {
    goToPage(currentPage() - 1);
}

void DocumentTab::zoomIn() {
    setZoomFactor(zoomFactor() * kZoomStep);
}

void DocumentTab::zoomOut() {
    setZoomFactor(zoomFactor() / kZoomStep);
}

void DocumentTab::setZoomFactor(qreal factor) {
    m_view->setZoomMode(QPdfView::ZoomMode::Custom);
    m_view->setZoomFactor(qBound(kMinZoom, factor, kMaxZoom));
}

void DocumentTab::fitToWidth() {
    m_view->setZoomMode(QPdfView::ZoomMode::FitToWidth);
}

void DocumentTab::toggleSearchBar() {
    if (m_searchBar->isVisible()) {
        closeSearchBar();
    } else {
        m_searchBar->show();
        m_searchBar->focusInput();
    }
}

void DocumentTab::closeSearchBar() {
    m_searchBar->hide();
    m_searchBar->setResultText(QString());
    m_searchModel->setSearchString(QString());
    m_currentResultIndex = -1;
    m_view->setFocus();
}

void DocumentTab::onSearchTextChanged(const QString& text) {
    m_currentResultIndex = -1;
    m_searchModel->setSearchString(text);
    onSearchResultsChanged();
}

void DocumentTab::onSearchResultsChanged() {
    const int count = m_searchModel->count();
    if (m_searchModel->searchString().isEmpty()) {
        m_searchBar->setResultText(QString());
    } else if (count == 0) {
        m_searchBar->setResultText(tr("0 résultat"));
    } else {
        if (m_currentResultIndex < 0) {
            goToSearchResult(0);
            return;
        }
        m_searchBar->setResultText(tr("%1 / %2").arg(m_currentResultIndex + 1).arg(count));
    }
}

void DocumentTab::goToSearchResult(int index) {
    const int count = m_searchModel->count();
    if (count == 0) {
        return;
    }
    m_currentResultIndex = (index % count + count) % count;
    m_view->setCurrentSearchResultIndex(m_currentResultIndex);

    const QPdfLink link = m_searchModel->resultAtIndex(m_currentResultIndex);
    m_view->pageNavigator()->jump(link.page(), link.location(), m_view->zoomFactor());

    m_searchBar->setResultText(tr("%1 / %2").arg(m_currentResultIndex + 1).arg(count));
}

void DocumentTab::nextSearchResult() {
    goToSearchResult(m_currentResultIndex < 0 ? 0 : m_currentResultIndex + 1);
}

void DocumentTab::previousSearchResult() {
    goToSearchResult(m_currentResultIndex < 0 ? 0 : m_currentResultIndex - 1);
}

void DocumentTab::highlightCurrentSearchResult() {
    if (m_currentResultIndex < 0 || m_currentResultIndex >= m_searchModel->count()) {
        return;
    }
    const QPdfLink link = m_searchModel->resultAtIndex(m_currentResultIndex);
    const QString filePath = m_document.filePath();

    pdf::AnnotationWriter writer;
    if (writer.load(filePath) != pdf::EditResult::Ok ||
        !writer.addTextMarkup(link.page(), pdf::AnnotationShape::Highlight, link.rectangles(),
                               QColor(255, 255, 0))) {
        QMessageBox::warning(this, tr("Échec"), tr("Impossible de surligner ce résultat."));
        return;
    }

    const QString tempPath = filePath + QStringLiteral(".papyrus-tmp");
    if (writer.save(tempPath) != pdf::EditResult::Ok) {
        QFile::remove(tempPath);
        QMessageBox::warning(this, tr("Échec"), tr("L'enregistrement a échoué."));
        return;
    }
    QFile::remove(filePath);
    if (!QFile::rename(tempPath, filePath)) {
        QMessageBox::warning(this, tr("Échec"), tr("Impossible de remplacer le fichier d'origine."));
        return;
    }
    emit annotationsSaved(filePath);
}

} // namespace papyrus
