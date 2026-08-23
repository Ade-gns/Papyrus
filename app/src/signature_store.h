#pragma once

#include <QImage>
#include <QList>
#include <QString>

namespace papyrus {

// Persists named signature images (PNG, with transparency) under the app's
// data directory ("Mes signatures" in the UI). Deliberately simple:
// filesystem + no index file, the file's base name (sanitized) is the
// signature's display name and each PNG carries an alpha channel so it
// composites onto a page correctly when placed.
namespace SignatureStore {

struct Entry {
    QString name;
    QString imagePath;
};

QString storageDir();
QList<Entry> list();

// Overwrites any existing signature with the same name.
bool save(const QString& name, const QImage& image);
bool remove(const QString& name);

} // namespace SignatureStore

} // namespace papyrus
