#include "papyrus/pdf/page_editor.h"

#include "pdfium_runtime.h"

#include <fpdf_edit.h>
#include <fpdf_ppo.h>
#include <fpdf_save.h>
#include <fpdfview.h>

#include <QFile>
#include <QFileInfo>

namespace papyrus::pdf {

using detail::acquirePdfiumLibrary;
using detail::releasePdfiumLibrary;

namespace {

// One page in the working order: which page of the originally loaded
// document it comes from, plus any rotation not yet baked in.
struct PageSlot {
    int sourceIndex;
    int rotationDelta = 0; // additional quarter turns clockwise, 0-3
};

struct FileWriter : FPDF_FILEWRITE {
    explicit FileWriter(const QString& path) : file(path) {
        version = 1;
        WriteBlock = &FileWriter::write;
    }

    bool open() { return file.open(QIODevice::WriteOnly | QIODevice::Truncate); }

    static int write(FPDF_FILEWRITE* self, const void* data, unsigned long size) {
        auto* writer = static_cast<FileWriter*>(self);
        return writer->file.write(static_cast<const char*>(data), static_cast<qint64>(size)) ==
                       static_cast<qint64>(size)
                   ? 1
                   : 0;
    }

    QFile file;
};

EditResult mapLoadError() {
    switch (FPDF_GetLastError()) {
    case FPDF_ERR_FILE:
        return EditResult::FileNotFound;
    case FPDF_ERR_FORMAT:
        return EditResult::InvalidFormat;
    case FPDF_ERR_PASSWORD:
        return EditResult::PasswordProtected;
    default:
        return EditResult::Unknown;
    }
}

// Rebuilds a brand-new document containing `entries` pages taken from `source`,
// in order, with each slot's rotation delta applied. Caller owns the result.
FPDF_DOCUMENT buildDocument(FPDF_DOCUMENT source, const std::vector<PageSlot>& entries) {
    FPDF_DOCUMENT dest = FPDF_CreateNewDocument();
    for (std::size_t i = 0; i < entries.size(); ++i) {
        const int sourceIndex = entries[i].sourceIndex;
        if (!FPDF_ImportPagesByIndex(dest, source, &sourceIndex, 1, static_cast<int>(i))) {
            FPDF_CloseDocument(dest);
            return nullptr;
        }
        if (entries[i].rotationDelta != 0) {
            if (FPDF_PAGE page = FPDF_LoadPage(dest, static_cast<int>(i))) {
                const int base = FPDFPage_GetRotation(page);
                FPDFPage_SetRotation(page, (base + entries[i].rotationDelta) % 4);
                FPDF_ClosePage(page);
            }
        }
    }
    return dest;
}

EditResult writeDocument(FPDF_DOCUMENT document, const QString& outputFilePath) {
    FileWriter writer(outputFilePath);
    if (!writer.open()) {
        return EditResult::SaveFailed;
    }
    const bool ok = FPDF_SaveAsCopy(document, &writer, 0);
    writer.file.close();
    return ok ? EditResult::Ok : EditResult::SaveFailed;
}

} // namespace

struct PageEditor::Impl {
    Impl() { acquirePdfiumLibrary(); }
    ~Impl() {
        if (document) {
            FPDF_CloseDocument(document);
        }
        releasePdfiumLibrary();
    }

    FPDF_DOCUMENT document = nullptr;
    std::vector<PageSlot> entries;
    int originalPageCount = 0;
};

PageEditor::PageEditor() : m_impl(std::make_unique<Impl>()) {}
PageEditor::~PageEditor() = default;

EditResult PageEditor::load(const QString& filePath) {
    if (!QFileInfo::exists(filePath)) {
        return EditResult::FileNotFound;
    }
    FPDF_DOCUMENT doc = FPDF_LoadDocument(filePath.toUtf8().constData(), nullptr);
    if (!doc) {
        return mapLoadError();
    }

    if (m_impl->document) {
        FPDF_CloseDocument(m_impl->document);
    }
    m_impl->document = doc;

    const int count = FPDF_GetPageCount(doc);
    m_impl->originalPageCount = count;
    m_impl->entries.clear();
    m_impl->entries.reserve(count);
    for (int i = 0; i < count; ++i) {
        m_impl->entries.push_back(PageSlot{i, 0});
    }
    return EditResult::Ok;
}

int PageEditor::pageCount() const {
    return static_cast<int>(m_impl->entries.size());
}

int PageEditor::rotationDegrees(int displayIndex) const {
    if (displayIndex < 0 || displayIndex >= pageCount()) {
        return 0;
    }
    const PageSlot& slot = m_impl->entries[displayIndex];
    int base = 0;
    if (FPDF_PAGE page = FPDF_LoadPage(m_impl->document, slot.sourceIndex)) {
        base = FPDFPage_GetRotation(page);
        FPDF_ClosePage(page);
    }
    return ((base + slot.rotationDelta) % 4) * 90;
}

bool PageEditor::hasPendingChanges() const {
    const auto& entries = m_impl->entries;
    for (std::size_t i = 0; i < entries.size(); ++i) {
        if (entries[i].sourceIndex != static_cast<int>(i) || entries[i].rotationDelta != 0) {
            return true;
        }
    }
    return false;
}

void PageEditor::rotatePage(int displayIndex, int quarterTurnsClockwise) {
    if (displayIndex < 0 || displayIndex >= pageCount()) {
        return;
    }
    PageSlot& slot = m_impl->entries[displayIndex];
    slot.rotationDelta = ((slot.rotationDelta + quarterTurnsClockwise) % 4 + 4) % 4;
}

void PageEditor::deletePage(int displayIndex) {
    if (displayIndex < 0 || displayIndex >= pageCount()) {
        return;
    }
    m_impl->entries.erase(m_impl->entries.begin() + displayIndex);
}

void PageEditor::duplicatePage(int displayIndex) {
    if (displayIndex < 0 || displayIndex >= pageCount()) {
        return;
    }
    m_impl->entries.insert(m_impl->entries.begin() + displayIndex + 1, m_impl->entries[displayIndex]);
}

void PageEditor::movePage(int fromIndex, int toIndex) {
    const int count = pageCount();
    if (fromIndex < 0 || fromIndex >= count || toIndex < 0 || toIndex >= count || fromIndex == toIndex) {
        return;
    }
    const PageSlot slot = m_impl->entries[fromIndex];
    m_impl->entries.erase(m_impl->entries.begin() + fromIndex);
    m_impl->entries.insert(m_impl->entries.begin() + toIndex, slot);
}

void PageEditor::setEntries(const std::vector<int>& sourcePageIndices,
                             const std::vector<int>& rotationQuarterTurns) {
    std::vector<PageSlot> next;
    next.reserve(sourcePageIndices.size());
    for (std::size_t i = 0; i < sourcePageIndices.size(); ++i) {
        const int sourceIndex = sourcePageIndices[i];
        if (sourceIndex < 0 || sourceIndex >= m_impl->originalPageCount) {
            continue;
        }
        const int rotation = i < rotationQuarterTurns.size() ? ((rotationQuarterTurns[i] % 4) + 4) % 4 : 0;
        next.push_back(PageSlot{sourceIndex, rotation});
    }
    m_impl->entries = std::move(next);
}

EditResult PageEditor::save(const QString& outputFilePath) const {
    FPDF_DOCUMENT rebuilt = buildDocument(m_impl->document, m_impl->entries);
    if (!rebuilt) {
        return EditResult::Unknown;
    }
    const EditResult result = writeDocument(rebuilt, outputFilePath);
    FPDF_CloseDocument(rebuilt);
    return result;
}

EditResult PageEditor::extractPages(const std::vector<int>& displayIndices,
                                     const QString& outputFilePath) const {
    std::vector<PageSlot> subset;
    subset.reserve(displayIndices.size());
    for (int index : displayIndices) {
        if (index >= 0 && index < pageCount()) {
            subset.push_back(m_impl->entries[index]);
        }
    }
    if (subset.empty()) {
        return EditResult::Unknown;
    }
    FPDF_DOCUMENT rebuilt = buildDocument(m_impl->document, subset);
    if (!rebuilt) {
        return EditResult::Unknown;
    }
    const EditResult result = writeDocument(rebuilt, outputFilePath);
    FPDF_CloseDocument(rebuilt);
    return result;
}

EditResult PageEditor::mergeFiles(const std::vector<QString>& inputFiles, const QString& outputFilePath) {
    acquirePdfiumLibrary();
    FPDF_DOCUMENT dest = FPDF_CreateNewDocument();
    int destIndex = 0;
    EditResult result = EditResult::Ok;

    for (const QString& path : inputFiles) {
        FPDF_DOCUMENT src = FPDF_LoadDocument(path.toUtf8().constData(), nullptr);
        if (!src) {
            result = mapLoadError();
            break;
        }
        const int count = FPDF_GetPageCount(src);
        for (int i = 0; i < count; ++i) {
            if (!FPDF_ImportPagesByIndex(dest, src, &i, 1, destIndex)) {
                result = EditResult::Unknown;
                break;
            }
            ++destIndex;
        }
        FPDF_CloseDocument(src);
        if (result != EditResult::Ok) {
            break;
        }
    }

    if (result == EditResult::Ok) {
        result = writeDocument(dest, outputFilePath);
    }
    FPDF_CloseDocument(dest);
    releasePdfiumLibrary();
    return result;
}

} // namespace papyrus::pdf
