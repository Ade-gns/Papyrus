#include "papyrus/conversion/office_to_pdf.h"

#include <QDir>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTimer>

namespace papyrus::conversion {

bool isLibreOfficeAvailable() {
    return !QStandardPaths::findExecutable(QStringLiteral("soffice")).isEmpty() ||
           !QStandardPaths::findExecutable(QStringLiteral("libreoffice")).isEmpty();
}

namespace {
constexpr int kTimeoutMs = 5 * 60 * 1000; // 5 minutes for the whole batch
}

struct OfficeToPdfJob::Impl {
    QStringList inputPaths;
    QString outputDir;
    QProcess process;
    QTemporaryDir profileDir;
    QByteArray pendingOutput;
};

OfficeToPdfJob::OfficeToPdfJob(QStringList inputPaths, QString outputDir, QObject* parent)
    : QObject(parent), m_impl(std::make_unique<Impl>()) {
    m_impl->inputPaths = std::move(inputPaths);
    m_impl->outputDir = std::move(outputDir);
}

OfficeToPdfJob::~OfficeToPdfJob() {
    if (m_impl->process.state() != QProcess::NotRunning) {
        m_impl->process.kill();
        m_impl->process.waitForFinished(3000);
    }
}

void OfficeToPdfJob::start() {
    if (!isLibreOfficeAvailable()) {
        QTimer::singleShot(0, this, [this] {
            emit finished(false, tr("LibreOffice n'est pas installé (commande « soffice » introuvable)."));
        });
        return;
    }
    if (m_impl->inputPaths.isEmpty()) {
        QTimer::singleShot(0, this, [this] { emit finished(false, tr("Aucun fichier à convertir.")); });
        return;
    }
    if (!m_impl->profileDir.isValid()) {
        QTimer::singleShot(0, this, [this] {
            emit finished(false, tr("Impossible de créer un profil temporaire pour LibreOffice."));
        });
        return;
    }
    QDir().mkpath(m_impl->outputDir);

    QString binary = QStandardPaths::findExecutable(QStringLiteral("soffice"));
    if (binary.isEmpty()) {
        binary = QStandardPaths::findExecutable(QStringLiteral("libreoffice"));
    }

    QStringList args;
    args << QStringLiteral("--headless") << QStringLiteral("--norestore") << QStringLiteral("--convert-to")
         << QStringLiteral("pdf") << QStringLiteral("--outdir") << m_impl->outputDir
         << QStringLiteral("-env:UserInstallation=file://%1").arg(m_impl->profileDir.path());
    args += m_impl->inputPaths;

    connect(&m_impl->process, &QProcess::readyReadStandardOutput, this, [this] {
        m_impl->pendingOutput += m_impl->process.readAllStandardOutput();
        int newlineIndex = 0;
        while ((newlineIndex = m_impl->pendingOutput.indexOf('\n')) >= 0) {
            const QByteArray line = m_impl->pendingOutput.left(newlineIndex);
            m_impl->pendingOutput.remove(0, newlineIndex + 1);
            // LibreOffice sometimes prints "convert X as a <Type> document -> Y
            // using filter : ..." and sometimes just "convert X -> Y using
            // filter : ...", depending on version — strip the optional middle
            // clause so group 1 is always just the input path.
            static const QRegularExpression pattern(
                QStringLiteral(R"(^convert (.+?)(?: as a .+ document)? -> (.+) using filter)"));
            const QRegularExpressionMatch match = pattern.match(QString::fromUtf8(line));
            if (match.hasMatch()) {
                emit fileConverted(match.captured(1), match.captured(2));
            }
        }
    });

    connect(&m_impl->process, &QProcess::finished, this,
            [this](int exitCode, QProcess::ExitStatus exitStatus) {
                const bool success = exitStatus == QProcess::NormalExit && exitCode == 0;
                emit finished(success, success ? QString() : tr("LibreOffice s'est terminé avec une erreur."));
            });
    connect(&m_impl->process, &QProcess::errorOccurred, this,
            [this](QProcess::ProcessError) { emit finished(false, tr("Impossible de lancer LibreOffice.")); });

    m_impl->process.start(binary, args);

    QTimer::singleShot(kTimeoutMs, this, [this] {
        if (m_impl->process.state() != QProcess::NotRunning) {
            m_impl->process.kill();
            emit finished(false, tr("La conversion a dépassé le délai imparti."));
        }
    });
}

void OfficeToPdfJob::cancel() {
    if (m_impl->process.state() != QProcess::NotRunning) {
        m_impl->process.kill();
    }
}

} // namespace papyrus::conversion
