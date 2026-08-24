#include "papyrus/pdf/annotation_writer.h"

#include "pdfium_runtime.h"

#include <fpdf_annot.h>
#include <fpdf_edit.h>
#include <fpdf_save.h>
#include <fpdfview.h>

#include <QFile>
#include <QFileInfo>
#include <QTransform>

namespace papyrus::pdf {

using detail::acquirePdfiumLibrary;
using detail::releasePdfiumLibrary;

namespace {

FPDF_ANNOTATION_SUBTYPE subtypeFor(AnnotationShape shape) {
    switch (shape) {
    case AnnotationShape::Highlight:
        return FPDF_ANNOT_HIGHLIGHT;
    case AnnotationShape::Underline:
        return FPDF_ANNOT_UNDERLINE;
    case AnnotationShape::StrikeOut:
        return FPDF_ANNOT_STRIKEOUT;
    case AnnotationShape::Rectangle:
        return FPDF_ANNOT_SQUARE;
    case AnnotationShape::Circle:
        return FPDF_ANNOT_CIRCLE;
    }
    return FPDF_ANNOT_UNKNOWN;
}

// Converts a rect in top-down page-point space (origin top-left, y down —
// the convention QPdfLink::rectangles() uses) to a PDF-native quad (origin
// bottom-left, y up).
FS_QUADPOINTSF toQuad(const QRectF& topDownRect, double pageHeightPt) {
    const double pdfTop = pageHeightPt - topDownRect.top();
    const double pdfBottom = pageHeightPt - topDownRect.bottom();
    FS_QUADPOINTSF quad{};
    quad.x1 = static_cast<float>(topDownRect.left());
    quad.y1 = static_cast<float>(pdfTop);
    quad.x2 = static_cast<float>(topDownRect.right());
    quad.y2 = static_cast<float>(pdfTop);
    quad.x3 = static_cast<float>(topDownRect.left());
    quad.y3 = static_cast<float>(pdfBottom);
    quad.x4 = static_cast<float>(topDownRect.right());
    quad.y4 = static_cast<float>(pdfBottom);
    return quad;
}

FS_RECTF toRect(const QRectF& topDownRect, double pageHeightPt) {
    return FS_RECTF{static_cast<float>(topDownRect.left()), static_cast<float>(pageHeightPt - topDownRect.top()),
                     static_cast<float>(topDownRect.right()),
                     static_cast<float>(pageHeightPt - topDownRect.bottom())};
}

} // namespace

struct AnnotationWriter::Impl {
    Impl() { acquirePdfiumLibrary(); }
    ~Impl() {
        if (document) {
            FPDF_CloseDocument(document);
        }
        releasePdfiumLibrary();
    }

    FPDF_DOCUMENT document = nullptr;
};

AnnotationWriter::AnnotationWriter() : m_impl(std::make_unique<Impl>()) {}
AnnotationWriter::~AnnotationWriter() = default;

EditResult AnnotationWriter::load(const QString& filePath) {
    if (!QFileInfo::exists(filePath)) {
        return EditResult::FileNotFound;
    }
    FPDF_DOCUMENT doc = FPDF_LoadDocument(filePath.toUtf8().constData(), nullptr);
    if (!doc) {
        switch (FPDF_GetLastError()) {
        case FPDF_ERR_FORMAT:
            return EditResult::InvalidFormat;
        case FPDF_ERR_PASSWORD:
            return EditResult::PasswordProtected;
        default:
            return EditResult::Unknown;
        }
    }
    if (m_impl->document) {
        FPDF_CloseDocument(m_impl->document);
    }
    m_impl->document = doc;
    return EditResult::Ok;
}

bool AnnotationWriter::addTextMarkup(int pageIndex, AnnotationShape shape, const QList<QRectF>& quads,
                                      const QColor& color) {
    if (quads.empty()) {
        return false;
    }
    FPDF_PAGE page = FPDF_LoadPage(m_impl->document, pageIndex);
    if (!page) {
        return false;
    }
    const double pageHeight = FPDF_GetPageHeightF(page);

    FPDF_ANNOTATION annot = FPDFPage_CreateAnnot(page, subtypeFor(shape));
    bool ok = annot != nullptr;
    if (ok) {
        QRectF bounds = quads.first();
        for (const QRectF& quad : quads) {
            FS_QUADPOINTSF q = toQuad(quad, pageHeight);
            ok = FPDFAnnot_AppendAttachmentPoints(annot, &q) && ok;
            bounds = bounds.united(quad);
        }
        // The quadpoints alone aren't enough — PDFium's default appearance
        // generation for markup annotations also needs /Rect set, or it
        // silently renders nothing (found by direct testing).
        FS_RECTF r = toRect(bounds, pageHeight);
        FPDFAnnot_SetRect(annot, &r);
        FPDFAnnot_SetColor(annot, FPDFANNOT_COLORTYPE_Color, color.red(), color.green(), color.blue(), 255);
        FPDFPage_CloseAnnot(annot);
    }
    FPDFPage_GenerateContent(page);
    FPDF_ClosePage(page);
    return ok;
}

bool AnnotationWriter::addShape(int pageIndex, AnnotationShape shape, const QRectF& rect, const QColor& color,
                                 qreal borderWidth) {
    FPDF_PAGE page = FPDF_LoadPage(m_impl->document, pageIndex);
    if (!page) {
        return false;
    }
    const double pageHeight = FPDF_GetPageHeightF(page);

    FPDF_ANNOTATION annot = FPDFPage_CreateAnnot(page, subtypeFor(shape));
    bool ok = annot != nullptr;
    if (ok) {
        FS_RECTF r = toRect(rect, pageHeight);
        FPDFAnnot_SetRect(annot, &r);
        // Square/Circle need an interior (fill) color set, not just a border
        // stroke color, or PDFium's default appearance generation produces
        // nothing visible at all (found by direct testing) — border-only
        // shapes silently failed to render even though the annotation and
        // its stored rect/color were otherwise saved correctly.
        FPDFAnnot_SetColor(annot, FPDFANNOT_COLORTYPE_InteriorColor, color.red(), color.green(), color.blue(),
                            255);
        FPDFAnnot_SetColor(annot, FPDFANNOT_COLORTYPE_Color, 0, 0, 0, 255);
        FPDFAnnot_SetBorder(annot, static_cast<float>(borderWidth), 0, 0);
        FPDFPage_CloseAnnot(annot);
    }
    FPDFPage_GenerateContent(page);
    FPDF_ClosePage(page);
    return ok;
}

bool AnnotationWriter::addImage(int pageIndex, const QImage& sourceImage, const QRectF& rect,
                                 double rotationDegrees) {
    FPDF_PAGE page = FPDF_LoadPage(m_impl->document, pageIndex);
    if (!page) {
        return false;
    }
    const double pageHeight = FPDF_GetPageHeightF(page);

    const QImage image = sourceImage.convertToFormat(QImage::Format_ARGB32);
    FPDF_BITMAP bitmap = FPDFBitmap_CreateEx(image.width(), image.height(), FPDFBitmap_BGRA,
                                              const_cast<uchar*>(image.constBits()), image.bytesPerLine());
    if (!bitmap) {
        FPDF_ClosePage(page);
        return false;
    }

    FPDF_PAGEOBJECT imageObject = FPDFPageObj_NewImageObj(m_impl->document);
    FPDF_PAGE pages[] = {page};
    const bool bitmapSet = FPDFImageObj_SetBitmap(pages, 1, imageObject, bitmap);
    FPDFBitmap_Destroy(bitmap);
    if (!bitmapSet) {
        FPDFPageObj_Destroy(imageObject);
        FPDF_ClosePage(page);
        return false;
    }

    // An image object's bitmap is always mapped from the unit square
    // [0,1]x[0,1], so: scale to the target size, rotate around the center,
    // then translate the center into place — all in PDF's native bottom-up
    // point space.
    const QRectF pdfRect(rect.left(), pageHeight - rect.bottom(), rect.width(), rect.height());
    const QPointF center = pdfRect.center();

    QTransform transform;
    transform.translate(center.x(), center.y());
    transform.rotate(-rotationDegrees); // sign convention verified empirically (see smoke test)
    transform.scale(pdfRect.width(), pdfRect.height());
    transform.translate(-0.5, -0.5);

    const FS_MATRIX matrix{static_cast<float>(transform.m11()), static_cast<float>(transform.m12()),
                            static_cast<float>(transform.m21()), static_cast<float>(transform.m22()),
                            static_cast<float>(transform.m31()), static_cast<float>(transform.m32())};
    FPDFPageObj_SetMatrix(imageObject, &matrix);
    FPDFPage_InsertObject(page, imageObject); // takes ownership on success

    FPDFPage_GenerateContent(page);
    FPDF_ClosePage(page);
    return true;
}

EditResult AnnotationWriter::save(const QString& outputFilePath) const {
    detail::AtomicPdfWriter writer(outputFilePath);
    if (!writer.open()) {
        return EditResult::SaveFailed;
    }
    const bool ok = FPDF_SaveAsCopy(m_impl->document, &writer, 0) && writer.commit();
    return ok ? EditResult::Ok : EditResult::SaveFailed;
}

} // namespace papyrus::pdf
