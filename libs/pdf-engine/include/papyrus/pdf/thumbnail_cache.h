#pragma once

#include <QHash>
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

signals:
    void thumbnailReady(int pageIndex, const QPixmap& pixmap);

private:
    Document* m_document;
    QThreadPool m_pool;
    QHash<int, QPixmap> m_cache;
    QSet<int> m_pending;
};

} // namespace papyrus::pdf
