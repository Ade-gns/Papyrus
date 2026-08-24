#include "document_printer.h"

#include "papyrus/pdf/document.h"

#include <QPainter>
#include <QPrinter>

namespace papyrus {

void printDocument(pdf::Document& document, QPrinter& printer, int currentPageIndex) {
    const int pageCount = document.pageCount();
    if (pageCount <= 0) {
        return;
    }

    int firstPage = 0;
    int lastPage = pageCount - 1;
    switch (printer.printRange()) {
    case QPrinter::PageRange:
        firstPage = qMax(0, printer.fromPage() - 1);
        lastPage = qMin(pageCount - 1, printer.toPage() - 1);
        break;
    case QPrinter::CurrentPage:
        firstPage = lastPage = qBound(0, currentPageIndex, pageCount - 1);
        break;
    default:
        break;
    }
    if (firstPage > lastPage) {
        return;
    }

    QPainter painter;
    if (!painter.begin(&printer)) {
        return;
    }

    const QRectF pageRectPx = printer.pageRect(QPrinter::DevicePixel);
    const qreal dpi = printer.resolution();

    for (int page = firstPage; page <= lastPage; ++page) {
        if (page != firstPage) {
            printer.newPage();
        }

        const QSizeF pointSize = document.pagePointSize(page);
        const QSizeF pageSizePx(pointSize.width() / 72.0 * dpi, pointSize.height() / 72.0 * dpi);
        if (pageSizePx.isEmpty()) {
            continue;
        }

        const qreal scale = qMin(pageRectPx.width() / pageSizePx.width(),
                                  pageRectPx.height() / pageSizePx.height());
        const int targetWidthPx = qRound(pageSizePx.width() * scale);

        const QImage image = document.renderPage(page, targetWidthPx);
        if (image.isNull()) {
            continue;
        }

        const qreal x = pageRectPx.x() + (pageRectPx.width() - image.width()) / 2.0;
        const qreal y = pageRectPx.y() + (pageRectPx.height() - image.height()) / 2.0;
        painter.drawImage(QPointF(x, y), image);
    }

    painter.end();
}

} // namespace papyrus
