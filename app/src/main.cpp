#include "main_window.h"
#include "signature_fonts.h"

#include <QApplication>
#include <QLocale>
#include <QTranslator>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QApplication::setApplicationName("Papyrus");
    QApplication::setOrganizationName("Papyrus");
    QApplication::setApplicationVersion("0.1.0");
    QApplication::setDesktopFileName("papyrus"); // matches packaging/papyrus.desktop

    // Source strings are French; only non-French system locales need a
    // translation file (papyrus_<lang>.qm, embedded via app/resources/i18n.qrc).
    // load() falls back from a full locale name (e.g. "en_US") to just the
    // language code ("en") on its own, and simply fails — leaving the
    // hardcoded French text in place — when the system locale is French or
    // has no matching translation yet.
    QTranslator translator;
    if (translator.load(QLocale::system(), "papyrus", "_", ":/i18n")) {
        QApplication::installTranslator(&translator);
    }

    papyrus::bundledSignatureFontFamilies(); // load the handwriting fonts up front

    papyrus::MainWindow window;
    window.resize(1200, 850);
    window.show();

    if (argc > 1) {
        window.openFile(QString::fromLocal8Bit(argv[1]));
    }

    return QApplication::exec();
}
