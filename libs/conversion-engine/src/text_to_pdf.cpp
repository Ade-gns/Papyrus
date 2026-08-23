#include "papyrus/conversion/text_to_pdf.h"

#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QPrinter>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextStream>

namespace papyrus::conversion {

ConversionResult convertTextFileToPdf(const QString& textFilePath, const TextToPdfOptions& options,
                                       const QString& outputPdfPath) {
    if (!QFileInfo::exists(textFilePath)) {
        return ConversionResult::SourceNotFound;
    }
    QFile file(textFilePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return ConversionResult::SourceUnreadable;
    }
    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    const QString text = stream.readAll();
    file.close();

    QTextDocument document;
    document.setDefaultFont(QFont(options.fontFamily, options.fontPointSize));

    QTextOption textOption;
    textOption.setAlignment(options.alignment);
    document.setDefaultTextOption(textOption);

    QTextCursor cursor(&document);
    QTextBlockFormat blockFormat;
    blockFormat.setLineHeight(options.lineSpacingPercent, QTextBlockFormat::ProportionalHeight);
    blockFormat.setAlignment(options.alignment);
    cursor.setBlockFormat(blockFormat);

    QTextCharFormat charFormat;
    charFormat.setForeground(options.textColor);
    cursor.setCharFormat(charFormat);
    // insertText splits on '\n' into separate blocks, each inheriting the
    // format set on the cursor above.
    cursor.insertText(text);

    QPrinter printer(QPrinter::HighResolution);
    printer.setOutputFormat(QPrinter::PdfFormat);
    printer.setOutputFileName(outputPdfPath);
    printer.setPageSize(options.pageSize);
    printer.setPageOrientation(options.orientation);
    printer.setPageMargins(QMarginsF(options.marginsMm, options.marginsMm, options.marginsMm, options.marginsMm),
                            QPageLayout::Millimeter);

    document.print(&printer);

    return QFileInfo::exists(outputPdfPath) ? ConversionResult::Ok : ConversionResult::WriteFailed;
}

} // namespace papyrus::conversion
