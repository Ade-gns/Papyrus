#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

#include <memory>

namespace papyrus::conversion {

// True if a `soffice` (or `libreoffice`) binary is on PATH.
bool isLibreOfficeAvailable();

// Converts a batch of Office documents (doc/docx/odt/ppt/pptx/odp) to PDF in
// a single LibreOffice headless subprocess (`soffice --convert-to pdf` can
// take multiple inputs at once, which avoids paying LibreOffice's ~1-2s
// startup cost once per file). Runs with an isolated, temporary user profile
// (`-env:UserInstallation=...`) so a conversion never touches the user's own
// LibreOffice profile or collides with an already-running LibreOffice
// instance, and macros are not executed by a plain --convert-to.
//
// Asynchronous: construct, connect to the signals, then call start(). The
// job deletes its temporary profile directory on destruction.
class OfficeToPdfJob : public QObject {
    Q_OBJECT
public:
    OfficeToPdfJob(QStringList inputPaths, QString outputDir, QObject* parent = nullptr);
    ~OfficeToPdfJob() override;

    void start();
    void cancel();

signals:
    // Emitted once per file as LibreOffice reports finishing it.
    void fileConverted(const QString& inputPath, const QString& outputPdfPath);
    void finished(bool success, const QString& errorMessage);

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace papyrus::conversion
