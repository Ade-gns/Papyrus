#include "papyrus/pdf/thumbnail_cache.h"

#include "papyrus/pdf/document.h"

#include <QMetaObject>
#include <QRunnable>

namespace papyrus::pdf {

ThumbnailCache::ThumbnailCache(Document* document, QObject* parent)
    : QObject(parent), m_document(document) {
    m_pool.setMaxThreadCount(1);
}

ThumbnailCache::~ThumbnailCache() {
    m_pool.clear();
    m_pool.waitForDone();
}

QPixmap ThumbnailCache::thumbnail(int pageIndex) {
    if (const auto it = m_cache.constFind(pageIndex); it != m_cache.constEnd()) {
        return it.value();
    }
    if (m_pending.contains(pageIndex)) {
        return {};
    }
    m_pending.insert(pageIndex);

    Document* document = m_document;
    QRunnable* task = QRunnable::create([this, document, pageIndex] {
        const QImage image = document->renderPage(pageIndex, kWidth);
        QMetaObject::invokeMethod(
            this,
            [this, pageIndex, image] {
                m_pending.remove(pageIndex);
                const QPixmap pixmap = QPixmap::fromImage(image);
                m_cache.insert(pageIndex, pixmap);
                m_cacheOrder.append(pageIndex);
                while (m_cacheOrder.size() > kMaxCached) {
                    m_cache.remove(m_cacheOrder.takeFirst());
                }
                emit thumbnailReady(pageIndex, pixmap);
            },
            Qt::QueuedConnection);
    });
    m_pool.start(task);
    return {};
}

void ThumbnailCache::clear() {
    m_pool.clear();
    m_pool.waitForDone();
    m_pending.clear();
    m_cache.clear();
    m_cacheOrder.clear();
}

} // namespace papyrus::pdf
