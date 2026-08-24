#include "update_checker.h"

#include <QApplication>
#include <QDesktopServices>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QStringList>
#include <QUrl>

namespace papyrus {

namespace {

constexpr char kReleasesApiUrl[] = "https://api.github.com/repos/Ade-gns/Papyrus/releases/latest";
constexpr char kReleasesPageUrl[] = "https://github.com/Ade-gns/Papyrus/releases/latest";

// Compares two "X.Y.Z"-style versions (a leading 'v' is stripped first).
// Missing components compare as 0, so "1.2" == "1.2.0". Returns >0 if `a` is
// newer than `b`, 0 if equal, <0 if older.
int compareVersions(QString a, QString b) {
    if (a.startsWith('v')) a.remove(0, 1);
    if (b.startsWith('v')) b.remove(0, 1);

    const QStringList partsA = a.split('.');
    const QStringList partsB = b.split('.');
    const int count = qMax(partsA.size(), partsB.size());
    for (int i = 0; i < count; ++i) {
        const int va = i < partsA.size() ? partsA[i].toInt() : 0;
        const int vb = i < partsB.size() ? partsB[i].toInt() : 0;
        if (va != vb) {
            return va - vb;
        }
    }
    return 0;
}

} // namespace

UpdateChecker::UpdateChecker(QWidget* parentWidget)
    : QObject(parentWidget), m_manager(new QNetworkAccessManager(this)), m_parentWidget(parentWidget) {}

void UpdateChecker::checkSilently() { fetchLatestRelease(false); }

void UpdateChecker::checkInteractively() { fetchLatestRelease(true); }

void UpdateChecker::fetchLatestRelease(bool interactive) {
    QNetworkRequest request{QUrl(QString::fromLatin1(kReleasesApiUrl))};
    // GitHub's API rejects requests with no User-Agent header.
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Papyrus-UpdateChecker"));
    request.setRawHeader("Accept", "application/vnd.github+json");

    QNetworkReply* reply = m_manager->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, interactive] {
        handleReply(reply, interactive);
        reply->deleteLater();
    });
}

void UpdateChecker::handleReply(QNetworkReply* reply, bool interactive) {
    if (reply->error() != QNetworkReply::NoError) {
        if (interactive) {
            QMessageBox::warning(m_parentWidget, tr("Vérifier les mises à jour"),
                                  tr("Impossible de vérifier les mises à jour : %1").arg(reply->errorString()));
        }
        return;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
    const QString tagName = doc.object().value(QStringLiteral("tag_name")).toString();
    if (tagName.isEmpty()) {
        if (interactive) {
            QMessageBox::information(m_parentWidget, tr("Vérifier les mises à jour"),
                                      tr("Aucune version publiée n'a été trouvée."));
        }
        return;
    }

    const QString currentVersion = QApplication::applicationVersion();
    if (compareVersions(tagName, currentVersion) > 0) {
        const auto choice = QMessageBox::information(
            m_parentWidget, tr("Mise à jour disponible"),
            tr("Une nouvelle version de Papyrus est disponible : %1 (version actuelle : %2).\n\n"
               "Ouvrir la page des téléchargements ?")
                .arg(tagName, currentVersion),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
        if (choice == QMessageBox::Yes) {
            QDesktopServices::openUrl(QUrl(QString::fromLatin1(kReleasesPageUrl)));
        }
    } else if (interactive) {
        QMessageBox::information(m_parentWidget, tr("Vérifier les mises à jour"),
                                  tr("Papyrus est à jour (version %1).").arg(currentVersion));
    }
}

} // namespace papyrus
