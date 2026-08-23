#include "office_conversion_dialog.h"

#include "papyrus/conversion/office_to_pdf.h"

#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QVBoxLayout>

namespace papyrus {

OfficeConversionDialog::OfficeConversionDialog(QWidget* parent)
    : QDialog(parent),
      m_list(new QListWidget(this)),
      m_outputDirEdit(new QLineEdit(this)),
      m_statusLabel(new QLabel(this)),
      m_progressBar(new QProgressBar(this)) {
    setWindowTitle(tr("Convertir des documents Office en PDF"));
    resize(640, 480);

    if (!conversion::isLibreOfficeAvailable()) {
        m_statusLabel->setText(
            tr("LibreOffice n'est pas installé. Installez-le avec :\n"
               "sudo apt install libreoffice-writer libreoffice-impress"));
        m_statusLabel->setStyleSheet(QStringLiteral("color: #b00;"));
    }

    auto* listButtons = new QHBoxLayout();
    auto* addButton = new QPushButton(tr("Ajouter des documents..."), this);
    connect(addButton, &QPushButton::clicked, this, &OfficeConversionDialog::addFiles);
    auto* removeButton = new QPushButton(tr("Supprimer"), this);
    connect(removeButton, &QPushButton::clicked, this, &OfficeConversionDialog::removeSelected);
    listButtons->addWidget(addButton);
    listButtons->addWidget(removeButton);
    listButtons->addStretch(1);

    m_list->setSelectionMode(QAbstractItemView::ExtendedSelection);

    auto* outputRow = new QHBoxLayout();
    outputRow->addWidget(new QLabel(tr("Dossier de sortie :"), this));
    outputRow->addWidget(m_outputDirEdit, 1);
    auto* browseButton = new QPushButton(tr("Parcourir..."), this);
    connect(browseButton, &QPushButton::clicked, this, &OfficeConversionDialog::pickOutputDir);
    outputRow->addWidget(browseButton);

    m_progressBar->setRange(0, 0); // indeterminate
    m_progressBar->hide();

    m_convertButton = new QPushButton(tr("Convertir"), this);
    m_convertButton->setDefault(true);
    connect(m_convertButton, &QPushButton::clicked, this, &OfficeConversionDialog::startConversion);
    m_closeButton = new QPushButton(tr("Fermer"), this);
    connect(m_closeButton, &QPushButton::clicked, this, &QDialog::accept);

    auto* bottomRow = new QHBoxLayout();
    bottomRow->addWidget(m_statusLabel, 1);
    bottomRow->addWidget(m_convertButton);
    bottomRow->addWidget(m_closeButton);

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(listButtons);
    layout->addWidget(m_list, 1);
    layout->addLayout(outputRow);
    layout->addWidget(m_progressBar);
    layout->addLayout(bottomRow);
}

OfficeConversionDialog::~OfficeConversionDialog() = default;

void OfficeConversionDialog::addFiles() {
    const QStringList paths = QFileDialog::getOpenFileNames(
        this, tr("Choisir des documents Office"), {},
        tr("Documents Office (*.doc *.docx *.odt *.ppt *.pptx *.odp)"));
    for (const QString& path : paths) {
        auto* item = new QListWidgetItem(QFileInfo(path).fileName());
        item->setData(Qt::UserRole, path);
        item->setToolTip(path);
        m_list->addItem(item);
    }
    if (m_outputDirEdit->text().isEmpty() && !paths.isEmpty()) {
        m_outputDirEdit->setText(QFileInfo(paths.first()).absolutePath());
    }
}

void OfficeConversionDialog::removeSelected() {
    for (QListWidgetItem* item : m_list->selectedItems()) {
        delete m_list->takeItem(m_list->row(item));
    }
}

void OfficeConversionDialog::pickOutputDir() {
    const QString dir = QFileDialog::getExistingDirectory(this, tr("Choisir le dossier de sortie"));
    if (!dir.isEmpty()) {
        m_outputDirEdit->setText(dir);
    }
}

void OfficeConversionDialog::setBusy(bool busy) {
    m_convertButton->setEnabled(!busy);
    m_list->setEnabled(!busy);
    m_progressBar->setVisible(busy);
}

void OfficeConversionDialog::startConversion() {
    if (m_list->count() == 0) {
        QMessageBox::warning(this, tr("Aucun document"), tr("Ajoutez au moins un document."));
        return;
    }
    if (m_outputDirEdit->text().isEmpty()) {
        QMessageBox::warning(this, tr("Dossier manquant"), tr("Choisissez un dossier de sortie."));
        return;
    }

    QStringList inputs;
    for (int i = 0; i < m_list->count(); ++i) {
        inputs.append(m_list->item(i)->data(Qt::UserRole).toString());
    }
    m_convertedPaths.clear();

    m_job = std::make_unique<conversion::OfficeToPdfJob>(inputs, m_outputDirEdit->text());
    connect(m_job.get(), &conversion::OfficeToPdfJob::fileConverted, this,
            [this](const QString&, const QString& outputPath) {
                m_convertedPaths.append(outputPath);
                m_statusLabel->setText(tr("%1 / %2 converti(s)...").arg(m_convertedPaths.size()).arg(m_list->count()));
            });
    connect(m_job.get(), &conversion::OfficeToPdfJob::finished, this,
            [this](bool success, const QString& errorMessage) {
                setBusy(false);
                if (success) {
                    m_statusLabel->setText(tr("%1 document(s) converti(s) avec succès.").arg(m_convertedPaths.size()));
                    emit pdfsCreated(m_convertedPaths);
                } else {
                    m_statusLabel->setText(errorMessage);
                    QMessageBox::warning(this, tr("Échec de la conversion"), errorMessage);
                }
            });

    setBusy(true);
    m_statusLabel->setText(tr("Conversion en cours..."));
    m_job->start();
}

} // namespace papyrus
