#include "signature_store.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QStandardPaths>

namespace papyrus::SignatureStore {

namespace {
QString sanitize(const QString& name) {
    QString result = name;
    result.replace(QRegularExpression(R"([^\w\-. À-ÿ]+)"), QStringLiteral("_"));
    return result.isEmpty() ? QStringLiteral("signature") : result;
}
} // namespace

QString storageDir() {
    const QString dir =
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/signatures");
    QDir().mkpath(dir);
    return dir;
}

QList<Entry> list() {
    QList<Entry> entries;
    const QDir dir(storageDir());
    for (const QFileInfo& info : dir.entryInfoList({QStringLiteral("*.png")}, QDir::Files, QDir::Name)) {
        entries.append(Entry{info.completeBaseName(), info.absoluteFilePath()});
    }
    return entries;
}

bool save(const QString& name, const QImage& image) {
    const QString path = storageDir() + QStringLiteral("/%1.png").arg(sanitize(name));
    return image.save(path, "PNG");
}

bool remove(const QString& name) {
    return QFile::remove(storageDir() + QStringLiteral("/%1.png").arg(sanitize(name)));
}

} // namespace papyrus::SignatureStore
