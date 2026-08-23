#include "signature_fonts.h"

#include <QFontDatabase>

namespace papyrus {

QStringList bundledSignatureFontFamilies() {
    static const QStringList families = [] {
        const QStringList files = {
            QStringLiteral(":/fonts/Pacifico-Regular.ttf"),
            QStringLiteral(":/fonts/GreatVibes-Regular.ttf"),
            QStringLiteral(":/fonts/Sacramento-Regular.ttf"),
            QStringLiteral(":/fonts/AlexBrush-Regular.ttf"),
            QStringLiteral(":/fonts/Allura-Regular.ttf"),
            QStringLiteral(":/fonts/DancingScript-Regular.ttf"),
        };
        QStringList result;
        for (const QString& path : files) {
            const int id = QFontDatabase::addApplicationFont(path);
            const QStringList names = QFontDatabase::applicationFontFamilies(id);
            if (!names.isEmpty()) {
                result.append(names.first());
            }
        }
        return result;
    }();
    return families;
}

} // namespace papyrus
