#pragma once

#include <QStringList>

namespace papyrus {

// Loads the bundled handwriting fonts (once) and returns their family
// names, in a fixed preferred display order.
QStringList bundledSignatureFontFamilies();

} // namespace papyrus
