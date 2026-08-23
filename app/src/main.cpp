#include "main_window.h"
#include "signature_fonts.h"

#include <QApplication>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QApplication::setApplicationName("Papyrus");
    QApplication::setOrganizationName("Papyrus");
    QApplication::setApplicationVersion("0.1.0");

    papyrus::bundledSignatureFontFamilies(); // load the handwriting fonts up front

    papyrus::MainWindow window;
    window.resize(1200, 850);
    window.show();

    if (argc > 1) {
        window.openFile(QString::fromLocal8Bit(argv[1]));
    }

    return QApplication::exec();
}
