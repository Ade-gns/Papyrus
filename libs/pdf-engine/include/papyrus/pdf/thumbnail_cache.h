#pragma once

#include <QHash>
#include <QList>
#include <QObject>
#include <QPixmap>
#include <QSet>
#include <QThreadPool>

namespace papyrus::pdf {

class Document;

// Generates page thumbnails on a dedicated background thread so scrolling the
// thumbnail panel never blocks the UI thread. Rendering is serialized (a
// single worker thread) because QPdfDocument is not documented as safe for
// concurrent renders from multiple threads on the same instance.
//
// Callers are expected to request only the pages actually visible (plus a
// small prefetch margin) rather than the whole document up front — this
// class only guards against unbounded memory growth (kMaxCached, evicted
// oldest-first) if a caller doesn't.
class ThumbnailCache : public QObject {
    Q_OBJECT
public:
    explicit ThumbnailCache(Document* document, QObject* parent = nullptr);
    ~ThumbnailCache() override;

    // Returns the cached thumbnail if already generated. Otherwise schedules
    // background generation and returns a null pixmap; thumbnailReady() will
    // fire once it's available.
    QPixmap thumbnail(int pageIndex);

    void clear();

    static constexpr int kWidth = 140;
    static constexpr int kMaxCached = 300;

signals:
    void thumbnailReady(int pageIndex, const QPixmap& pixmap);

private:
    Document* m_document;
    QThreadPool m_pool;
    QHash<int, QPixmap> m_cache;
    QList<int> m_cacheOrder; // oldest-inserted first, for eviction
    QSet<int> m_pending;
};

} // namespace papyrus::pdf
