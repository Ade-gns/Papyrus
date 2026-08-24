#pragma once

#include <QObject>

QT_BEGIN_NAMESPACE
class QNetworkAccessManager;
class QNetworkReply;
class QWidget;
QT_END_NAMESPACE

namespace papyrus {

// Checks GitHub Releases for a version newer than the one currently running.
// No packaging format has its own repository/update infrastructure (no apt
// PPA, no dnf repo), so this is the one update mechanism available across
// .deb/.rpm/AppImage alike: point the user at the release page to download
// manually. Never installs anything itself.
class UpdateChecker : public QObject {
    Q_OBJECT
public:
    explicit UpdateChecker(QWidget* parentWidget);

    // For an unobtrusive startup check: only shows a dialog if a newer
    // version is actually found. Silent on error, rate limit, or "up to
    // date" — never bothers the user for those.
    void checkSilently();

    // For the "Vérifier les mises à jour..." menu action: always shows a
    // result, including "up to date" and error cases.
    void checkInteractively();

private:
    void fetchLatestRelease(bool interactive);
    void handleReply(QNetworkReply* reply, bool interactive);

    QNetworkAccessManager* m_manager;
    QWidget* m_parentWidget;
};

} // namespace papyrus
