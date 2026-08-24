#pragma once

#include <QByteArray>
#include <QString>

namespace papyrus::pdf {

// Whole-file undo/redo history, one independent stack pair per source file
// (keyed by its canonical path), persisted to disk under
// QStandardPaths::AppDataLocation rather than kept in memory. Two effects
// follow from that: the stacks survive an app restart, so Ctrl+Z still works
// after a crash or force-quit; and every edit's pre-image is durably on disk
// the moment the edit is committed, not just after a clean exit.
//
// Usage: capture() the file's current bytes right before an operation is
// about to overwrite it in place. If that operation then actually saves
// successfully, commit() the captured snapshot as the new undo point (this
// also clears the redo stack — a fresh edit abandons whatever had been
// undone). If the operation is cancelled instead, just drop the QByteArray.
class DocumentHistory {
public:
    static QByteArray capture(const QString& filePath);
    static void commit(const QString& filePath, const QByteArray& before);

    static bool canUndo(const QString& filePath);
    static bool canRedo(const QString& filePath);

    // Restore the previous/next version onto filePath (atomically). Returns
    // false and leaves the file untouched if there's nothing to undo/redo.
    static bool undo(const QString& filePath);
    static bool redo(const QString& filePath);
};

} // namespace papyrus::pdf
