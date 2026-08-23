#pragma once

#include "papyrus/conversion/result.h"

#include <QColor>
#include <QPageLayout>
#include <QPageSize>
#include <QString>

namespace papyrus::conversion {

struct TextToPdfOptions {
    QString fontFamily = QStringLiteral("Liberation Sans");
    int fontPointSize = 11;
    QColor textColor = QColor(Qt::black);
    double lineSpacingPercent = 100.0; // 100 = single spacing
    Qt::Alignment alignment = Qt::AlignLeft;
    double marginsMm = 20.0; // uniform margin on all four sides
    QPageSize pageSize{QPageSize::A4};
    QPageLayout::Orientation orientation = QPageLayout::Portrait;
};

// Renders a plain-text file into a paginated PDF using QTextDocument's own
// text layout (word wrap, pagination) — far more robust than hand-laying-out
// text via low-level PDF page objects.
//
// Scope note: no header/footer support yet — QTextDocument::print() paginates
// content but doesn't have a built-in per-page header/footer hook without
// manually painting each page, which was cut from this pass.
ConversionResult convertTextFileToPdf(const QString& textFilePath, const TextToPdfOptions& options,
                                       const QString& outputPdfPath);

} // namespace papyrus::conversion
