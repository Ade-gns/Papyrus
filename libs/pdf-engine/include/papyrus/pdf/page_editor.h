#pragma once

#include <QString>

#include <memory>
#include <vector>

namespace papyrus::pdf {

enum class EditResult {
    Ok,
    FileNotFound,
    InvalidFormat,
    PasswordProtected,
    SaveFailed,
    Unknown,
};

// Structural page editing (rotate/delete/duplicate/reorder/extract/merge),
// backed by PDFium's C API directly since QPdfDocument (used by Document) is
// read-only.
//
// rotatePage/deletePage/duplicatePage/movePage only touch an in-memory
// ordering of the source document's pages; nothing is written until save()
// or extractPages() rebuilds a fresh PDFium document in that order. PDFium
// has no API to reorder or duplicate pages in place, so every edit would
// otherwise need its own document-rebuild — batching them into one rebuild
// at save time keeps each edit an O(1) list operation.
class PageEditor {
public:
    PageEditor();
    ~PageEditor();

    PageEditor(const PageEditor&) = delete;
    PageEditor& operator=(const PageEditor&) = delete;

    EditResult load(const QString& filePath);

    int pageCount() const;
    int rotationDegrees(int displayIndex) const; // 0, 90, 180 or 270, clockwise
    bool hasPendingChanges() const;

    void rotatePage(int displayIndex, int quarterTurnsClockwise);
    void deletePage(int displayIndex);
    void duplicatePage(int displayIndex);
    void movePage(int fromIndex, int toIndex);

    // Replaces the working order wholesale. Meant for UI that manages its own
    // edit buffer (e.g. a page-organizer dialog with move-up/move-down
    // buttons) rather than issuing incremental calls above; sourcePageIndices
    // entries out of range for the originally loaded document are dropped.
    void setEntries(const std::vector<int>& sourcePageIndices,
                     const std::vector<int>& rotationQuarterTurns);

    EditResult save(const QString& outputFilePath) const;
    EditResult extractPages(const std::vector<int>& displayIndices,
                             const QString& outputFilePath) const;

    static EditResult mergeFiles(const std::vector<QString>& inputFiles,
                                  const QString& outputFilePath);

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace papyrus::pdf
