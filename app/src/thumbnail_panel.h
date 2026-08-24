#pragma once

#include <QWidget>

QT_BEGIN_NAMESPACE
class QListWidget;
class QListWidgetItem;
QT_END_NAMESPACE

namespace papyrus {

class DocumentTab;

// Sidebar listing page thumbnails for the active document. Clicking a
// thumbnail jumps the view to that page; the current page is highlighted
// when navigation happens elsewhere (toolbar, scrolling).
class ThumbnailPanel : public QWidget {
    Q_OBJECT
public:
    explicit ThumbnailPanel(QWidget* parent = nullptr);

    // Rebuilds the list for the given tab (nullptr clears it).
    void setDocumentTab(DocumentTab* tab);

public slots:
    void setCurrentPage(int pageIndex);

signals:
    void pageActivated(int pageIndex);

protected:
    void resizeEvent(QResizeEvent* event) override;

private:
    void onItemActivated(QListWidgetItem* item);

    // Only requests thumbnails for the pages currently in (or near) the
    // viewport, not the whole document — large documents would otherwise
    // queue hundreds of renders on ThumbnailCache's single worker thread
    // for pages the user may never scroll to.
    void requestVisibleThumbnails();

    QListWidget* m_list;
    DocumentTab* m_tab = nullptr;
};

} // namespace papyrus
