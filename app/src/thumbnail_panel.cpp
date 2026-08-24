#include "thumbnail_panel.h"

#include "document_tab.h"
#include "papyrus/pdf/thumbnail_cache.h"

#include <QListWidget>
#include <QResizeEvent>
#include <QScrollBar>
#include <QTimer>
#include <QVBoxLayout>

namespace papyrus {

namespace {
// How many rows beyond the visible viewport to request eagerly, so a small
// scroll doesn't immediately show blank icons while the worker thread catches
// up.
constexpr int kPrefetchRows = 8;
} // namespace

ThumbnailPanel::ThumbnailPanel(QWidget* parent) : QWidget(parent), m_list(new QListWidget(this)) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_list);

    m_list->setViewMode(QListView::ListMode);
    m_list->setIconSize(QSize(pdf::ThumbnailCache::kWidth, pdf::ThumbnailCache::kWidth * 2));
    m_list->setSpacing(4);

    connect(m_list, &QListWidget::itemClicked, this, &ThumbnailPanel::onItemActivated);
    connect(m_list->verticalScrollBar(), &QScrollBar::valueChanged, this,
            &ThumbnailPanel::requestVisibleThumbnails);
}

void ThumbnailPanel::setDocumentTab(DocumentTab* tab) {
    m_list->clear();
    m_tab = tab;
    if (!tab) {
        return;
    }

    for (int page = 0; page < tab->pageCount(); ++page) {
        auto* item = new QListWidgetItem(QStringLiteral("Page %1").arg(page + 1));
        item->setData(Qt::UserRole, page);
        m_list->addItem(item);
    }

    connect(tab->thumbnailCache(), &pdf::ThumbnailCache::thumbnailReady, this,
            [this, tab](int page, const QPixmap& pixmap) {
                if (m_tab != tab || page < 0 || page >= m_list->count()) {
                    return;
                }
                m_list->item(page)->setIcon(QIcon(pixmap));
            });

    // Deferred: the viewport isn't laid out to its final size yet at this
    // point (e.g. right after the dock widget is created).
    QTimer::singleShot(0, this, &ThumbnailPanel::requestVisibleThumbnails);
}

void ThumbnailPanel::setCurrentPage(int pageIndex) {
    if (pageIndex >= 0 && pageIndex < m_list->count()) {
        m_list->setCurrentRow(pageIndex);
    }
}

void ThumbnailPanel::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    requestVisibleThumbnails();
}

void ThumbnailPanel::requestVisibleThumbnails() {
    if (!m_tab || m_list->count() == 0) {
        return;
    }

    const QRect viewportRect = m_list->viewport()->rect();
    int firstRow = m_list->indexAt(viewportRect.topLeft()).row();
    if (firstRow < 0) {
        firstRow = 0;
    }

    int lastRow = m_list->indexAt(viewportRect.bottomLeft()).row();
    if (lastRow < 0) {
        const int rowHeight = qMax(1, m_list->sizeHintForRow(0));
        lastRow = firstRow + viewportRect.height() / rowHeight + 1;
    }

    firstRow = qMax(0, firstRow - kPrefetchRows);
    lastRow = qMin(m_list->count() - 1, lastRow + kPrefetchRows);

    for (int page = firstRow; page <= lastRow; ++page) {
        const QPixmap cached = m_tab->thumbnailCache()->thumbnail(page);
        if (!cached.isNull()) {
            m_list->item(page)->setIcon(QIcon(cached));
        }
    }
}

void ThumbnailPanel::onItemActivated(QListWidgetItem* item) {
    emit pageActivated(item->data(Qt::UserRole).toInt());
}

} // namespace papyrus
