#include "papyrus/pdf/document_history.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QStandardPaths>

namespace papyrus::pdf {

namespace {

constexpr int kMaxUndoDepth = 20;

// Each open file gets its own undo/ and redo/ subdirectory, holding
// sequentially-named snapshot files (zero-padded decimal counters): the
// highest name is the top of the stack. The next name is always
// (max existing name) + 1 rather than a running count, because cap
// enforcement below trims from the *bottom* of the stack — a plain entry
// count would then collide with a name still in use at the top.
QString historyRoot() {
    const QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return base + QStringLiteral("/history");
}

QString keyFor(const QString& filePath) {
    QString canonical = QFileInfo(filePath).canonicalFilePath();
    if (canonical.isEmpty()) {
        canonical = QFileInfo(filePath).absoluteFilePath();
    }
    return QString::fromLatin1(
        QCryptographicHash::hash(canonical.toUtf8(), QCryptographicHash::Sha1).toHex());
}

QString stackDir(const QString& filePath, const char* stackName) {
    return historyRoot() + QLatin1Char('/') + keyFor(filePath) + QLatin1Char('/') +
           QLatin1String(stackName);
}

QStringList sortedEntries(const QString& dirPath) {
    QDir dir(dirPath);
    QStringList entries = dir.entryList(QDir::Files, QDir::Name);
    return entries;
}

QString nextEntryName(const QStringList& existingEntries) {
    qint64 maxIndex = -1;
    for (const QString& entry : existingEntries) {
        bool ok = false;
        const qint64 index = entry.toLongLong(&ok, 10);
        if (ok) {
            maxIndex = qMax(maxIndex, index);
        }
    }
    return QString::number(maxIndex + 1).rightJustified(12, QLatin1Char('0'));
}

void pushEntry(const QString& dirPath, const QByteArray& bytes) {
    QDir().mkpath(dirPath);

    const QString name = nextEntryName(sortedEntries(dirPath));
    QSaveFile file(dirPath + QLatin1Char('/') + name);
    if (!file.open(QIODevice::WriteOnly)) {
        return;
    }
    file.write(bytes);
    file.commit();

    QStringList entries = sortedEntries(dirPath);
    while (entries.size() > kMaxUndoDepth) {
        QFile::remove(dirPath + QLatin1Char('/') + entries.takeFirst());
    }
}

bool popEntry(const QString& dirPath, QByteArray* outBytes) {
    const QStringList entries = sortedEntries(dirPath);
    if (entries.isEmpty()) {
        return false;
    }
    const QString path = dirPath + QLatin1Char('/') + entries.last();
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }
    *outBytes = file.readAll();
    file.close();
    QFile::remove(path);
    return true;
}

bool writeAtomically(const QString& filePath, const QByteArray& bytes) {
    QSaveFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    file.write(bytes);
    return file.commit();
}

} // namespace

QByteArray DocumentHistory::capture(const QString& filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return file.readAll();
}

void DocumentHistory::commit(const QString& filePath, const QByteArray& before) {
    if (before.isEmpty()) {
        return;
    }
    // A new edit abandons whatever had been undone.
    const QString redo = stackDir(filePath, "redo");
    for (const QString& entry : sortedEntries(redo)) {
        QFile::remove(redo + QLatin1Char('/') + entry);
    }
    pushEntry(stackDir(filePath, "undo"), before);
}

bool DocumentHistory::canUndo(const QString& filePath) {
    return !sortedEntries(stackDir(filePath, "undo")).isEmpty();
}

bool DocumentHistory::canRedo(const QString& filePath) {
    return !sortedEntries(stackDir(filePath, "redo")).isEmpty();
}

bool DocumentHistory::undo(const QString& filePath) {
    QByteArray previous;
    if (!popEntry(stackDir(filePath, "undo"), &previous)) {
        return false;
    }
    const QByteArray current = capture(filePath);
    if (!writeAtomically(filePath, previous)) {
        return false;
    }
    if (!current.isEmpty()) {
        pushEntry(stackDir(filePath, "redo"), current);
    }
    return true;
}

bool DocumentHistory::redo(const QString& filePath) {
    QByteArray next;
    if (!popEntry(stackDir(filePath, "redo"), &next)) {
        return false;
    }
    const QByteArray current = capture(filePath);
    if (!writeAtomically(filePath, next)) {
        return false;
    }
    if (!current.isEmpty()) {
        pushEntry(stackDir(filePath, "undo"), current);
    }
    return true;
}

} // namespace papyrus::pdf
