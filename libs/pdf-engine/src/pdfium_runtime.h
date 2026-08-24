#pragma once

#include <fpdf_save.h>
#include <fpdfview.h>

#include <QSaveFile>
#include <QString>

// Process-wide FPDF_InitLibrary()/FPDF_DestroyLibrary() lifetime, shared by
// every PDFium-backed class in this library (PageEditor, AnnotationWriter,
// ...). Must be a single counter across translation units — each keeping its
// own would let one class destroy the library while another still has a
// live FPDF_DOCUMENT.
namespace papyrus::pdf::detail {

void acquirePdfiumLibrary();
void releasePdfiumLibrary();

// FPDF_FILEWRITE backed by QSaveFile: writes land in a temp file next to
// `path` and only replace it on commit(). If the process dies mid-save (or
// commit() is never called), the original file is left untouched instead of
// truncated/corrupt — shared by every class here that calls FPDF_SaveAsCopy.
struct AtomicPdfWriter : FPDF_FILEWRITE {
    explicit AtomicPdfWriter(const QString& path);

    bool open();
    bool commit(); // call only after FPDF_SaveAsCopy reports success

    QSaveFile file;

private:
    static int write(FPDF_FILEWRITE* self, const void* data, unsigned long size);
};

} // namespace papyrus::pdf::detail
