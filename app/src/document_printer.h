#pragma once

#include <QtGlobal>

QT_BEGIN_NAMESPACE
class QPrinter;
QT_END_NAMESPACE

namespace papyrus::pdf {
class Document;
}

namespace papyrus {

// Renders a Document onto a QPrinter (real printer or the paint device handed
// to us by QPrintPreviewDialog::paintRequested). Honors the printer's page
// range/current-page selection; each page is scaled to fit the printable
// area while preserving its aspect ratio.
void printDocument(pdf::Document& document, QPrinter& printer, int currentPageIndex);

} // namespace papyrus
