#include "pdfium_runtime.h"

#include <fpdfview.h>

#include <atomic>

namespace papyrus::pdf::detail {

namespace {
std::atomic<int> g_refCount{0};
}

void acquirePdfiumLibrary() {
    if (g_refCount.fetch_add(1) == 0) {
        FPDF_InitLibrary();
    }
}

void releasePdfiumLibrary() {
    if (g_refCount.fetch_sub(1) == 1) {
        FPDF_DestroyLibrary();
    }
}

AtomicPdfWriter::AtomicPdfWriter(const QString& path) : file(path) {
    version = 1;
    WriteBlock = &AtomicPdfWriter::write;
}

bool AtomicPdfWriter::open() { return file.open(QIODevice::WriteOnly); }

bool AtomicPdfWriter::commit() { return file.commit(); }

int AtomicPdfWriter::write(FPDF_FILEWRITE* self, const void* data, unsigned long size) {
    auto* writer = static_cast<AtomicPdfWriter*>(self);
    return writer->file.write(static_cast<const char*>(data), static_cast<qint64>(size)) ==
                   static_cast<qint64>(size)
               ? 1
               : 0;
}

} // namespace papyrus::pdf::detail
