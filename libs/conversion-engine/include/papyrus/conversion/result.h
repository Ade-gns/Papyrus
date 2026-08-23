#pragma once

namespace papyrus::conversion {

enum class ConversionResult {
    Ok,
    SourceNotFound,
    SourceUnreadable,
    WriteFailed,
};

} // namespace papyrus::conversion
