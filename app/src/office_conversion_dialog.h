#pragma once

#include <QDialog>
#include <QStringList>

#include <memory>

QT_BEGIN_NAMESPACE
class QListWidget;
class QLabel;
class QProgressBar;
class QPushButton;
class QLineEdit;
QT_END_NAMESPACE

namespace papyrus::conversion {
class OfficeToPdfJob;
}

namespace papyrus {

// "Convert Office documents to PDF" dialog: pick DOC/DOCX/ODT/PPT/PPTX/ODP
// files and an output folder, converts them all in one LibreOffice headless
// subprocess (see papyrus::conversion::OfficeToPdfJob).
class OfficeConversionDialog : public QDialog {
    Q_OBJECT
public:
    explicit OfficeConversionDialog(QWidget* parent = nullptr);
    ~OfficeConversionDialog() override;

signals:
    void pdfsCreated(const QStringList& filePaths);

private:
    void addFiles();
    void removeSelected();
    void pickOutputDir();
    void startConversion();
    void setBusy(bool busy);

    QListWidget* m_list;
    QLineEdit* m_outputDirEdit;
    QLabel* m_statusLabel;
    QProgressBar* m_progressBar;
    QPushButton* m_convertButton;
    QPushButton* m_closeButton;

    std::unique_ptr<conversion::OfficeToPdfJob> m_job;
    QStringList m_convertedPaths;
};

} // namespace papyrus
