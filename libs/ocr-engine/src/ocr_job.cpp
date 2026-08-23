#include "papyrus/ocr/ocr_job.h"

#include "papyrus/pdf/document.h"
#include "papyrus/pdf/page_editor.h"

#include <QDir>
#include <QFileInfo>
#include <QMetaObject>
#include <QProcess>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtConcurrent/QtConcurrentRun>
#include <QFuture>
#include <QFutureWatcher>

#include <atomic>

namespace papyrus::ocr {

bool isTesseractAvailable() {
    return !QStandardPaths::findExecutable(QStringLiteral("tesseract")).isEmpty();
}

QStringList availableLanguages() {
    QProcess process;
    process.start(QStringLiteral("tesseract"), {QStringLiteral("--list-langs")});
    if (!process.waitForFinished(5000)) {
        return {};
    }
    const QStringList lines =
        QString::fromUtf8(process.readAllStandardOutput()).split('\n', Qt::SkipEmptyParts);
    QStringList languages;
    for (const QString& line : lines) {
        const QString trimmed = line.trimmed();
        if (trimmed.isEmpty() || trimmed.startsWith(QStringLiteral("List of")) ||
            trimmed == QStringLiteral("osd")) {
            continue;
        }
        languages.append(trimmed);
    }
    return languages;
}

struct OcrJob::Impl {
    QString inputPath;
    QString outputPath;
    QString language;
    QFutureWatcher<bool> watcher;
    QString errorMessage;
    std::atomic<bool> cancelRequested{false};
};

OcrJob::OcrJob(QString inputPdfPath, QString outputPdfPath, QString language, QObject* parent)
    : QObject(parent), m_impl(std::make_unique<Impl>()) {
    m_impl->inputPath = std::move(inputPdfPath);
    m_impl->outputPath = std::move(outputPdfPath);
    m_impl->language = std::move(language);
}

OcrJob::~OcrJob() {
    cancel();
    m_impl->watcher.waitForFinished();
}

void OcrJob::start() {
    if (!isTesseractAvailable()) {
        QMetaObject::invokeMethod(
            this, [this] { emit finished(false, tr("Tesseract OCR n'est pas installé.")); },
            Qt::QueuedConnection);
        return;
    }

    Impl* impl = m_impl.get();
    connect(&impl->watcher, &QFutureWatcher<bool>::finished, this,
            [this, impl] { emit finished(impl->watcher.result(), impl->errorMessage); });

    auto task = [this, impl]() -> bool {
        pdf::Document document;
        if (document.load(impl->inputPath) != pdf::LoadResult::Ok) {
            impl->errorMessage = tr("Impossible d'ouvrir le document source.");
            return false;
        }

        QTemporaryDir tempDir;
        if (!tempDir.isValid()) {
            impl->errorMessage = tr("Impossible de créer un dossier temporaire.");
            return false;
        }

        constexpr int kTargetDpi = 300;
        const int pageCount = document.pageCount();
        std::vector<QString> pagePdfPaths;

        for (int i = 0; i < pageCount; ++i) {
            if (impl->cancelRequested) {
                impl->errorMessage = tr("Annulé.");
                return false;
            }

            const QSizeF pointSize = document.pagePointSize(i);
            const int widthPx = qRound(pointSize.width() / 72.0 * kTargetDpi);
            const QImage pageImage = document.renderPage(i, widthPx);

            const QString imagePath = tempDir.filePath(QStringLiteral("page_%1.png").arg(i));
            const QString outputBase = tempDir.filePath(QStringLiteral("page_%1").arg(i));
            if (!pageImage.save(imagePath, "PNG")) {
                impl->errorMessage = tr("Échec du rendu de la page %1.").arg(i + 1);
                return false;
            }

            QProcess process;
            process.start(QStringLiteral("tesseract"),
                           {imagePath, outputBase, QStringLiteral("-l"), impl->language,
                            QStringLiteral("pdf")});
            if (!process.waitForFinished(120000) || process.exitStatus() != QProcess::NormalExit ||
                process.exitCode() != 0) {
                impl->errorMessage = tr("Échec de l'OCR sur la page %1.").arg(i + 1);
                return false;
            }

            const QString pagePdfPath = outputBase + QStringLiteral(".pdf");
            if (!QFileInfo::exists(pagePdfPath)) {
                impl->errorMessage = tr("Tesseract n'a produit aucun résultat pour la page %1.").arg(i + 1);
                return false;
            }
            pagePdfPaths.push_back(pagePdfPath);

            QMetaObject::invokeMethod(
                this, [this, i, pageCount] { emit progress(i + 1, pageCount); }, Qt::QueuedConnection);
        }

        const pdf::EditResult mergeResult = pdf::PageEditor::mergeFiles(pagePdfPaths, impl->outputPath);
        if (mergeResult != pdf::EditResult::Ok) {
            impl->errorMessage = tr("Échec de la fusion des pages OCRisées.");
            return false;
        }
        return true;
    };

    impl->watcher.setFuture(QtConcurrent::run(task));
}

void OcrJob::cancel() {
    m_impl->cancelRequested = true;
}

} // namespace papyrus::ocr
