#pragma once

#include "papyrus/pdf/page_editor.h" // reuses EditResult

#include <QColor>
#include <QImage>
#include <QList>
#include <QRectF>
#include <QString>

#include <memory>

namespace papyrus::pdf {

enum class AnnotationShape {
    Highlight, // quadpoint-based, marks existing text
    Underline, // quadpoint-based, marks existing text
    StrikeOut, // quadpoint-based, marks existing text
    Rectangle, // rect-based shape drawn on the page
    Circle,    // rect-based shape (ellipse inscribed in the rect) drawn on the page
};

// Adds annotations to a PDF via PDFium and saves the result.
//
// Scope note: only annotation types PDFium reliably auto-generates a visible
// appearance for are supported here, verified empirically against this
// PDFium build — Highlight/Underline/StrikeOut (quadpoint-based markup on
// existing text) and Square/Circle (rect-based shapes). Freehand ink, plain
// lines/arrows and free-text boxes were tried first and PDFium saved them
// but rendered nothing back without a hand-built appearance stream, so
// they're deliberately left out rather than shipped invisible.
class AnnotationWriter {
public:
    AnnotationWriter();
    ~AnnotationWriter();

    AnnotationWriter(const AnnotationWriter&) = delete;
    AnnotationWriter& operator=(const AnnotationWriter&) = delete;

    EditResult load(const QString& filePath);

    // quads: one rect per line of marked text, in *top-down* page-point space
    // (origin top-left, y increases downward) — the same convention
    // QPdfLink::rectangles() already uses, so search results can be passed
    // straight through. Converted to PDF's native bottom-up space internally.
    bool addTextMarkup(int pageIndex, AnnotationShape shape, const QList<QRectF>& quads,
                        const QColor& color);

    // rect: bounding box in the same top-down page-point space as above.
    // color: the shape's fill (interior) color; the border is always black.
    bool addShape(int pageIndex, AnnotationShape shape, const QRectF& rect, const QColor& color,
                  qreal borderWidth);

    // Inserts `image` as real page content (a page-level image object), not
    // an annotation. Used for placing signatures/stamps: unlike the markup
    // and shape annotations above, this always renders in every viewer
    // including our own QPdfView-based one, which doesn't render
    // annotations at all (see phase-3 notes) — going through real page
    // content sidesteps that limitation entirely for this feature.
    //
    // rect: bounding box in top-down page-point space. rotationDegrees:
    // clockwise, applied around the rect's center.
    bool addImage(int pageIndex, const QImage& image, const QRectF& rect, double rotationDegrees);

    EditResult save(const QString& outputFilePath) const;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace papyrus::pdf
