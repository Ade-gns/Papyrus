#pragma once

#include "papyrus/pdf/page_editor.h" // reuses EditResult

#include <QRectF>
#include <QString>

#include <memory>
#include <optional>

namespace papyrus::pdf {

// One editable run of existing text on a page: PDFium groups consecutive
// characters written by the same content-stream text-drawing operator into
// one "text object" (typically a line, sometimes a whole styled run,
// depending on how the source PDF was generated) — that's the smallest unit
// FPDFText_SetText can replace, so it's also the smallest unit a click can
// select here.
struct TextRun {
    QString text;
    QRectF boundsPoints;  // top-down page-point space, union of all its chars
    int startCharIndex = -1;
    int endCharIndex = -1; // inclusive
};

// Finds and replaces existing text on a page via PDFium's FPDFText_SetText.
//
// Important, inherent limitation (this is how PDF stores text everywhere,
// not a shortcut specific to Papyrus): a PDF page has no reflow model, so
// replacing a run with text of different width does not move anything else
// on the page. A much longer or shorter replacement can visually overlap
// following text or leave a gap — the same behavior as the "edit text"
// feature in mainstream PDF editors.
class TextEditor {
public:
    TextEditor();
    ~TextEditor();

    TextEditor(const TextEditor&) = delete;
    TextEditor& operator=(const TextEditor&) = delete;

    EditResult load(const QString& filePath);

    // The text run at/near `point` (top-down page-point space), within
    // `tolerancePoints` of it, or std::nullopt if there's no text there.
    std::optional<TextRun> runAt(int pageIndex, const QPointF& point, double tolerancePoints) const;

    // Replaces `run`'s text in place. `run` must have come from the most
    // recent runAt() call on this page — replacing invalidates character
    // indices after it, so runs found before a replacement on the same page
    // must not be reused.
    bool replaceText(int pageIndex, const TextRun& run, const QString& newText);

    EditResult save(const QString& outputFilePath) const;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace papyrus::pdf
