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

} // namespace papyrus::pdf::detail
