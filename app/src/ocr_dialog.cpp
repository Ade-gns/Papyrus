#include "ocr_dialog.h"

#include "papyrus/ocr/ocr_job.h"

#include <QComboBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QVBoxLayout>

namespace papyrus {

namespace {
QString languageDisplayName(const QString& code) {
    static const QHash<QString, QString> names = {
        {QStringLiteral("fra"), QObject::tr("Français")},
        {QStringLiteral("eng"), QObject::tr("Anglais")},
        {QStringLiteral("deu"), QObject::tr("Allemand")},
        {QStringLiteral("spa"), QObject::tr("Espagnol")},
        {QStringLiteral("ita"), QObject::tr("Italien")},
    };
    return names.value(code, code);
}
} // namespace

OcrDialog::OcrDialog(const QString& filePath, QWidget* parent)
    : QDialog(parent),
      m_filePath(filePath),
      m_languageCombo(new QComboBox(this)),
      m_statusLabel(new QLabel(this)),
      m_progressBar(new QProgressBar(this)) {
    setWindowTitle(tr("Rendre ce document recherchable (OCR)"));
    resize(480, 220);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel(
        tr("La reconnaissance de texte transforme un PDF scanné (image) en document où le "
           "texte peut être sélectionné et recherché."),
        this));

    if (!ocr::isTesseractAvailable()) {
        m_statusLabel->setText(tr("Tesseract OCR n'est pas installé.\nsudo apt install tesseract-ocr"));
        m_statusLabel->setStyleSheet(QStringLiteral("color: #b00;"));
    } else {
        const QStringList languages = ocr::availableLanguages();
        for (const QString& code : languages) {
            m_languageCombo->addItem(languageDisplayName(code), code);
        }
        const int frenchIndex = m_languageCombo->findData(QStringLiteral("fra"));
        if (frenchIndex >= 0) {
            m_languageCombo->setCurrentIndex(frenchIndex);
        }
        if (languages.isEmpty()) {
            m_statusLabel->setText(tr("Aucune langue installée.\nsudo apt install tesseract-ocr-fra"));
            m_statusLabel->setStyleSheet(QStringLiteral("color: #b00;"));
        }
    }

    auto* languageRow = new QHBoxLayout();
    languageRow->addWidget(new QLabel(tr("Langue du document :"), this));
    languageRow->addWidget(m_languageCombo, 1);
    layout->addLayout(languageRow);

    m_progressBar->setRange(0, 0);
    m_progressBar->hide();
    layout->addWidget(m_progressBar);
    layout->addWidget(m_statusLabel);

    m_startButton = new QPushButton(tr("Lancer l'OCR..."), this);
    m_startButton->setDefault(true);
    connect(m_startButton, &QPushButton::clicked, this, &OcrDialog::startOcr);
    m_closeButton = new QPushButton(tr("Fermer"), this);
    connect(m_closeButton, &QPushButton::clicked, this, &QDialog::accept);
    auto* bottomRow = new QHBoxLayout();
    bottomRow->addStretch(1);
    bottomRow->addWidget(m_closeButton);
    bottomRow->addWidget(m_startButton);
    layout->addLayout(bottomRow);

    if (!ocr::isTesseractAvailable() || m_languageCombo->count() == 0) {
        m_startButton->setEnabled(false);
    }
}

OcrDialog::~OcrDialog() = default;

void OcrDialog::startOcr() {
    const QString outputPath = QFileDialog::getSaveFileName(
        this, tr("Enregistrer le PDF recherchable"),
        QFileInfo(m_filePath).absolutePath() + "/" + QFileInfo(m_filePath).completeBaseName() +
            tr("_ocr.pdf"),
        tr("PDF (*.pdf)"));
    if (outputPath.isEmpty()) {
        return;
    }

    m_startButton->setEnabled(false);
    m_languageCombo->setEnabled(false);
    m_progressBar->show();
    m_statusLabel->setText(tr("Traitement en cours..."));

    const QString language = m_languageCombo->currentData().toString();
    m_job = std::make_unique<ocr::OcrJob>(m_filePath, outputPath, language);
    connect(m_job.get(), &ocr::OcrJob::progress, this, [this](int current, int total) {
        m_progressBar->setRange(0, total);
        m_progressBar->setValue(current);
        m_statusLabel->setText(tr("Page %1 / %2...").arg(current).arg(total));
    });
    connect(m_job.get(), &ocr::OcrJob::finished, this, [this, outputPath](bool success, const QString& error) {
        m_startButton->setEnabled(true);
        m_languageCombo->setEnabled(true);
        m_progressBar->hide();
        if (success) {
            m_statusLabel->setText(tr("Terminé."));
            emit pdfCreated(outputPath);
            accept();
        } else {
            m_statusLabel->setText(error);
            QMessageBox::warning(this, tr("Échec de l'OCR"), error);
        }
    });
    m_job->start();
}

} // namespace papyrus
