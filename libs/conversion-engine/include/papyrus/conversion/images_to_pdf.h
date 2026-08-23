#pragma once

#include "papyrus/conversion/result.h"

#include <QPageLayout>
#include <QPageSize>
#include <QStringList>

namespace papyrus::conversion {

struct ImagesToPdfOptions {
    QPageSize pageSize{QPageSize::A4};
    QPageLayout::Orientation orientation = QPageLayout::Portrait;
    bool keepAspectRatio = true; // centered if true; stretched to fill the page if false
};

// One image per page, in list order. Unreadable images are skipped; if none
// of the given paths could be read, returns SourceUnreadable.
//
// Scope note: multiple images per page isn't implemented in this pass — the
// common case (one scan/photo per page) is what this covers.
ConversionResult convertImagesToPdf(const QStringList& imagePaths, const ImagesToPdfOptions& options,
                                     const QString& outputPdfPath);

} // namespace papyrus::conversion
