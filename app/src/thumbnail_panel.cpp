#include "thumbnail_panel.h"

#include "document_tab.h"
#include "papyrus/pdf/thumbnail_cache.h"

#include <QListWidget>
#include <QVBoxLayout>

namespace papyrus {

ThumbnailPanel::ThumbnailPanel(QWidget* parent) : QWidget(parent), m_list(new QListWidget(this)) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_list);

    m_list->setViewMode(QListView::ListMode);
    m_list->setIconSize(QSize(pdf::ThumbnailCache::kWidth, pdf::ThumbnailCache::kWidth * 2));
    m_list->setSpacing(4);

    connect(m_list, &QListWidget::itemClicked, this, &ThumbnailPanel::onItemActivated);
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

        // Note: for very large documents this should be limited to the
        // visible range and extended lazily on scroll; kept simple for the
        // MVP since generation is already serialized off the UI thread.
        const QPixmap cached = tab->thumbnailCache()->thumbnail(page);
        if (!cached.isNull()) {
            item->setIcon(QIcon(cached));
        }
    }

    connect(tab->thumbnailCache(), &pdf::ThumbnailCache::thumbnailReady, this,
            [this, tab](int page, const QPixmap& pixmap) {
                if (m_tab != tab || page < 0 || page >= m_list->count()) {
                    return;
                }
                m_list->item(page)->setIcon(QIcon(pixmap));
            });
}

void ThumbnailPanel::setCurrentPage(int pageIndex) {
    if (pageIndex >= 0 && pageIndex < m_list->count()) {
        m_list->setCurrentRow(pageIndex);
    }
}

void ThumbnailPanel::onItemActivated(QListWidgetItem* item) {
    emit pageActivated(item->data(Qt::UserRole).toInt());
}

} // namespace papyrus
