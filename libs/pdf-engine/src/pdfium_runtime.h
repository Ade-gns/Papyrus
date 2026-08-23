#pragma once

// Process-wide FPDF_InitLibrary()/FPDF_DestroyLibrary() lifetime, shared by
// every PDFium-backed class in this library (PageEditor, AnnotationWriter,
// ...). Must be a single counter across translation units — each keeping its
// own would let one class destroy the library while another still has a
// live FPDF_DOCUMENT.
namespace papyrus::pdf::detail {

void acquirePdfiumLibrary();
void releasePdfiumLibrary();

} // namespace papyrus::pdf::detail
