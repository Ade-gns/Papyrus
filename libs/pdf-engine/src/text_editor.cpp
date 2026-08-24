#include "papyrus/pdf/text_editor.h"

#include "pdfium_runtime.h"

#include <fpdf_edit.h>
#include <fpdf_save.h>
#include <fpdf_text.h>
#include <fpdfview.h>

#include <QColor>
#include <QFileInfo>

#include <algorithm>
#include <array>

namespace papyrus::pdf {

using detail::acquirePdfiumLibrary;
using detail::releasePdfiumLibrary;

namespace {

// FPDFText_GetCharBox is bottom-up PDF user space (y increases upward); the
// rest of Papyrus's pdf-engine API (AnnotationWriter, Document) uses
// top-down page-point space (origin top-left, y increases downward) to match
// QPdfLink::rectangles() — same convention, same conversion, as
// annotation_writer.cpp's toRect/toQuad.
QRectF charBoxTopDown(FPDF_TEXTPAGE textPage, int index, double pageHeight) {
    double left = 0, right = 0, bottom = 0, top = 0;
    if (!FPDFText_GetCharBox(textPage, index, &left, &right, &bottom, &top)) {
        return {};
    }
    return QRectF(QPointF(left, pageHeight - top), QPointF(right, pageHeight - bottom));
}

bool isWordBoundary(FPDF_TEXTPAGE textPage, int index) {
    const unsigned int code = FPDFText_GetUnicode(textPage, index);
    return code == ' ' || code == '\t' || code == '\r' || code == '\n' || code == 0;
}

// Distinct FPDF_PAGEOBJECTs spanned by [start, end], in first-seen order.
// Real PDF producers don't agree on how much text one object covers — some
// emit a whole line per object, others (e.g. Qt's own PDF writer, found by
// testing) emit one object per *glyph*. A word clicked on can therefore be
// backed by anywhere from one to many objects.
std::vector<FPDF_PAGEOBJECT> objectsInRange(FPDF_TEXTPAGE textPage, int start, int end) {
    std::vector<FPDF_PAGEOBJECT> objects;
    for (int i = start; i <= end; ++i) {
        FPDF_PAGEOBJECT object = FPDFText_GetTextObject(textPage, i);
        if (object && std::find(objects.begin(), objects.end(), object) == objects.end()) {
            objects.push_back(object);
        }
    }
    return objects;
}

// The char range [start, end] is only the clicked *word*; the object(s)
// backing it may extend further (a whole sentence, in some real-world PDFs —
// see objectsInRange()'s comment). Widening to each object's true full range
// lets replaceText() reconstruct prefix+newText+suffix so the parts of the
// object outside the clicked word survive.
void widenToObjectExtents(FPDF_TEXTPAGE textPage, const std::vector<FPDF_PAGEOBJECT>& objects, int totalChars,
                           int& start, int& end) {
    for (FPDF_PAGEOBJECT object : objects) {
        int s = start;
        while (s > 0 && FPDFText_GetTextObject(textPage, s - 1) == object) {
            --s;
        }
        int e = end;
        while (e + 1 < totalChars && FPDFText_GetTextObject(textPage, e + 1) == object) {
            ++e;
        }
        start = std::min(start, s);
        end = std::max(end, e);
    }
}

// Real-world PDFs (confirmed: French government forms) sometimes wrap all
// page content in a single Form XObject. FPDFFormObj_RemoveObject can delete
// a text object nested one level inside one, but the deletion does not
// reliably survive save+reload: FPDFPage_GenerateContent only regenerates
// the page's own top-level content stream, not the XObject's nested one.
// Rather than depend on removal working, cover the original glyphs with an
// opaque rectangle (real page content, not an annotation — Papyrus's own
// QPdfView doesn't render annotations) and draw the replacement as a new
// page-level text object on top. Both are pure insertions, which have
// proven reliable regardless of how the source PDF nests its content.
FPDF_PAGEOBJECT makeCoverRect(const QRectF& boxBottomUp, const QColor& fillColor) {
    constexpr double margin = 0.5; // swallow antialiasing fringes of the covered glyphs
    FPDF_PAGEOBJECT rect = FPDFPageObj_CreateNewRect(
        static_cast<float>(boxBottomUp.left() - margin), static_cast<float>(boxBottomUp.top() - margin),
        static_cast<float>(boxBottomUp.width() + 2 * margin), static_cast<float>(boxBottomUp.height() + 2 * margin));
    FPDFPageObj_SetFillColor(rect, fillColor.red(), fillColor.green(), fillColor.blue(), 255);
    FPDFPath_SetDrawMode(rect, FPDF_FILLMODE_ALTERNATE, false);
    return rect;
}

// Samples the page background just outside the given box (bottom-up PDF
// point space) by rendering a small crop and averaging its edge pixels.
// Falls back to white when the crop can't be rendered.
QColor sampleBackgroundColor(FPDF_PAGE page, const QRectF& boxBottomUp, double pageWidthPts, double pageHeightPts) {
    constexpr double margin = 3.0; // points outside the box to sample
    constexpr double scale = 3.0;  // px per pt in the crop bitmap

    QRectF sampleRect = boxBottomUp.adjusted(-margin, -margin, margin, margin);
    sampleRect = sampleRect.intersected(QRectF(0, 0, pageWidthPts, pageHeightPts));
    if (sampleRect.width() <= 0 || sampleRect.height() <= 0) {
        return Qt::white;
    }

    const int cropW = std::max(1, static_cast<int>(sampleRect.width() * scale));
    const int cropH = std::max(1, static_cast<int>(sampleRect.height() * scale));
    FPDF_BITMAP bitmap = FPDFBitmap_Create(cropW, cropH, 0);
    if (!bitmap) {
        return Qt::white;
    }
    FPDFBitmap_FillRect(bitmap, 0, 0, cropW, cropH, 0xFFFFFFFF);

    const int fullW = static_cast<int>(pageWidthPts * scale);
    const int fullH = static_cast<int>(pageHeightPts * scale);
    const int offsetX = static_cast<int>(sampleRect.left() * scale);
    const int offsetY = static_cast<int>(sampleRect.top() * scale);
    FPDF_RenderPageBitmap(bitmap, page, -offsetX, -offsetY, fullW, fullH, 0, FPDF_ANNOT);

    const auto* buffer = static_cast<const unsigned char*>(FPDFBitmap_GetBuffer(bitmap));
    const int stride = FPDFBitmap_GetStride(bitmap);

    auto sampleAt = [&](int px, int py) {
        px = std::clamp(px, 0, cropW - 1);
        py = std::clamp(py, 0, cropH - 1);
        const unsigned char* p = buffer + static_cast<std::size_t>(py) * stride + static_cast<std::size_t>(px) * 4;
        return std::array<int, 3>{p[2], p[1], p[0]}; // BGRx -> RGB
    };
    const std::array<std::array<int, 3>, 4> samples = {sampleAt(cropW / 2, 1), sampleAt(cropW / 2, cropH - 2),
                                                         sampleAt(1, cropH / 2), sampleAt(cropW - 2, cropH / 2)};
    int r = 0, g = 0, b = 0;
    for (const auto& s : samples) {
        r += s[0];
        g += s[1];
        b += s[2];
    }
    FPDFBitmap_Destroy(bitmap);
    return QColor(r / 4, g / 4, b / 4);
}

} // namespace

struct TextEditor::Impl {
    Impl() { acquirePdfiumLibrary(); }
    ~Impl() {
        if (document) {
            FPDF_CloseDocument(document);
        }
        releasePdfiumLibrary();
    }

    FPDF_DOCUMENT document = nullptr;
};

TextEditor::TextEditor() : m_impl(std::make_unique<Impl>()) {}
TextEditor::~TextEditor() = default;

EditResult TextEditor::load(const QString& filePath) {
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

std::optional<TextRun> TextEditor::runAt(int pageIndex, const QPointF& point, double tolerancePoints) const {
    FPDF_PAGE page = FPDF_LoadPage(m_impl->document, pageIndex);
    if (!page) {
        return std::nullopt;
    }
    const double pageHeight = FPDF_GetPageHeightF(page);
    FPDF_TEXTPAGE textPage = FPDFText_LoadPage(page);
    if (!textPage) {
        FPDF_ClosePage(page);
        return std::nullopt;
    }

    // point is top-down; FPDFText_GetCharIndexAtPos wants bottom-up.
    const int charIndex = FPDFText_GetCharIndexAtPos(textPage, point.x(), pageHeight - point.y(), tolerancePoints,
                                                       tolerancePoints);
    std::optional<TextRun> result;
    if (charIndex >= 0 && !isWordBoundary(textPage, charIndex)) {
        // Word boundaries are found by content (whitespace), not by object
        // identity — see objectsInRange()'s comment for why object identity
        // alone isn't a usable unit of selection.
        const int totalChars = FPDFText_CountChars(textPage);
        int start = charIndex;
        while (start > 0 && !isWordBoundary(textPage, start - 1)) {
            --start;
        }
        int end = charIndex;
        while (end + 1 < totalChars && !isWordBoundary(textPage, end + 1)) {
            ++end;
        }

        const int count = end - start + 1;
        std::vector<unsigned short> buffer(static_cast<std::size_t>(count) + 1);
        const int written = FPDFText_GetText(textPage, start, count, buffer.data());
        const QString text = written > 0
                                  ? QString::fromUtf16(reinterpret_cast<const char16_t*>(buffer.data()),
                                                        written - 1) // drop the trailing terminator
                                  : QString();

        QRectF bounds;
        for (int i = start; i <= end; ++i) {
            bounds = bounds.united(charBoxTopDown(textPage, i, pageHeight));
        }

        result = TextRun{text, bounds, start, end};
    }

    FPDFText_ClosePage(textPage);
    FPDF_ClosePage(page);
    return result;
}

bool TextEditor::replaceText(int pageIndex, const TextRun& run, const QString& newText) {
    if (run.startCharIndex < 0) {
        return false;
    }
    FPDF_PAGE page = FPDF_LoadPage(m_impl->document, pageIndex);
    if (!page) {
        return false;
    }
    const double pageWidth = FPDF_GetPageWidthF(page);
    const double pageHeight = FPDF_GetPageHeightF(page);
    FPDF_TEXTPAGE textPage = FPDFText_LoadPage(page);
    if (!textPage) {
        FPDF_ClosePage(page);
        return false;
    }

    const int totalChars = FPDFText_CountChars(textPage);
    const std::vector<FPDF_PAGEOBJECT> objects = objectsInRange(textPage, run.startCharIndex, run.endCharIndex);
    if (objects.empty()) {
        FPDFText_ClosePage(textPage);
        FPDF_ClosePage(page);
        return false;
    }

    // Widen to each backing object's full extent (may exceed the clicked
    // word — see widenToObjectExtents()'s comment) and reconstruct
    // prefix+newText+suffix so text outside the clicked word, but sharing an
    // object with it, is preserved.
    int fullStart = run.startCharIndex;
    int fullEnd = run.endCharIndex;
    widenToObjectExtents(textPage, objects, totalChars, fullStart, fullEnd);

    std::vector<unsigned short> buffer(static_cast<std::size_t>(fullEnd - fullStart + 1) + 1);
    const int written = FPDFText_GetText(textPage, fullStart, fullEnd - fullStart + 1, buffer.data());
    const QString originalFull = written > 0 ? QString::fromUtf16(reinterpret_cast<const char16_t*>(buffer.data()),
                                                                    written - 1)
                                              : QString();
    const int relStart = run.startCharIndex - fullStart;
    const int wordLength = run.endCharIndex - run.startCharIndex + 1;
    const QString prefix = originalFull.left(relStart);
    const QString suffix = originalFull.mid(relStart + wordLength);
    const QString replacement = prefix + newText + suffix;

    // Union of the touched extent's char boxes, in bottom-up PDF point space
    // (PDFium's native space, unlike charBoxTopDown()'s conversion for the
    // rest of this file's Papyrus-facing API).
    double left = 1e9, right = -1e9, bottom = 1e9, top = -1e9;
    bool haveBox = false;
    for (int i = fullStart; i <= fullEnd; ++i) {
        double l = 0, r = 0, b = 0, t = 0;
        if (FPDFText_GetCharBox(textPage, i, &l, &r, &b, &t)) {
            left = std::min(left, l);
            right = std::max(right, r);
            bottom = std::min(bottom, b);
            top = std::max(top, t);
            haveBox = true;
        }
    }
    double originX = 0, originY = 0;
    FPDFText_GetCharOrigin(textPage, fullStart, &originX, &originY);
    FPDFText_ClosePage(textPage);

    if (!haveBox) {
        FPDF_ClosePage(page);
        return false;
    }
    const QRectF boxBottomUp(QPointF(left, bottom), QPointF(right, top));

    // The nominal Tf font size can be misleading when the source PDF scales
    // text via the object's transform matrix instead (found by testing on a
    // real government form: FPDFTextObj_GetFontSize reported 1.0pt while the
    // glyphs render several points tall). The char box height is a robust
    // proxy for the actual visual size regardless of which mechanism the
    // source PDF used.
    float fontSize = static_cast<float>(top - bottom);
    if (fontSize <= 0) {
        fontSize = 10.0f;
    }

    const QColor background = sampleBackgroundColor(page, boxBottomUp, pageWidth, pageHeight);
    FPDF_PAGEOBJECT cover = makeCoverRect(boxBottomUp, background);
    FPDFPage_InsertObject(page, cover);

    // A standard base-14 font, rather than reusing the original object's
    // font, sidesteps two problems found by testing on real documents: (1)
    // PDF producers often embed only the glyph subset actually used, so
    // reusing the font can render new characters as missing-glyph boxes; (2)
    // mutating an existing complex-font object's text in place was found not
    // to reliably persist through save+reload on some real documents.
    FPDF_PAGEOBJECT newObject = FPDFPageObj_NewTextObj(m_impl->document, "Helvetica", fontSize);
    const std::u16string wide = replacement.toStdU16String();
    const bool ok = FPDFText_SetText(newObject, reinterpret_cast<FPDF_WIDESTRING>(wide.c_str()));
    if (!ok) {
        FPDFPageObj_Destroy(newObject);
        FPDF_ClosePage(page);
        return false;
    }
    FPDFPageObj_Transform(newObject, 1, 0, 0, 1, originX, originY);
    FPDFPage_InsertObject(page, newObject);

    FPDFPage_GenerateContent(page);
    FPDF_ClosePage(page);
    return true;
}

EditResult TextEditor::save(const QString& outputFilePath) const {
    detail::AtomicPdfWriter writer(outputFilePath);
    if (!writer.open()) {
        return EditResult::SaveFailed;
    }
    const bool ok = FPDF_SaveAsCopy(m_impl->document, &writer, 0) && writer.commit();
    return ok ? EditResult::Ok : EditResult::SaveFailed;
}

} // namespace papyrus::pdf
