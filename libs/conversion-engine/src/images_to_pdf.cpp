#include "papyrus/conversion/images_to_pdf.h"

#include <QFileInfo>
#include <QImage>
#include <QPainter>
#include <QPdfWriter>

namespace papyrus::conversion {

namespace {
constexpr int kResolutionDpi = 300;
}

ConversionResult convertImagesToPdf(const QStringList& imagePaths, const ImagesToPdfOptions& options,
                                     const QString& outputPdfPath) {
    if (imagePaths.isEmpty()) {
        return ConversionResult::SourceNotFound;
    }

    QPdfWriter writer(outputPdfPath);
    writer.setPageSize(options.pageSize);
    writer.setPageOrientation(options.orientation);
    writer.setResolution(kResolutionDpi);

    QPainter painter;
    bool started = false;
    int placedCount = 0;

    for (const QString& path : imagePaths) {
        const QImage image(path);
        if (image.isNull()) {
            continue;
        }
        if (!started) {
            if (!painter.begin(&writer)) {
                return ConversionResult::WriteFailed;
            }
            started = true;
        } else {
            writer.newPage();
        }

        const QRectF pageRect(0, 0, writer.width(), writer.height());
        QRectF targetRect = pageRect;
        if (options.keepAspectRatio) {
            const QSizeF scaled = QSizeF(image.size()).scaled(pageRect.size(), Qt::KeepAspectRatio);
            targetRect = QRectF(QPointF((pageRect.width() - scaled.width()) / 2,
                                         (pageRect.height() - scaled.height()) / 2),
                                 scaled);
        }
        painter.drawImage(targetRect, image);
        ++placedCount;
    }

    if (started) {
        painter.end();
    }
    if (placedCount == 0) {
        return ConversionResult::SourceUnreadable;
    }
    return QFileInfo::exists(outputPdfPath) ? ConversionResult::Ok : ConversionResult::WriteFailed;
}

} // namespace papyrus::conversion
