#pragma once

#include <QDialog>
#include <QImage>

QT_BEGIN_NAMESPACE
class QListWidget;
class QTabWidget;
class QLabel;
class QCheckBox;
QT_END_NAMESPACE

namespace papyrus {

class SignaturePadWidget;
class TypedSignatureWidget;

// Signature hub: "Mes signatures" (saved, reusable) on the left; three
// creation tabs (draw / import / type) on the right; place the selected
// signature onto the current document via SignaturePlacementDialog.
class SignatureDialog : public QDialog {
    Q_OBJECT
public:
    SignatureDialog(const QString& documentFilePath, int currentPageIndex, QWidget* parent = nullptr);

signals:
    void documentSaved(const QString& filePath);

private:
    void refreshSignatureList();
    void saveDrawn();
    void saveImported();
    void saveTyped();
    void removeSelected();
    void placeSelected();
    void pickImportFile();
    void updateImportPreview();
    void promptAndSave(const QImage& image, const QString& suggestedName);

    QString m_documentFilePath;
    int m_pageIndex;

    QListWidget* m_signatureList;

    SignaturePadWidget* m_padWidget;
    TypedSignatureWidget* m_typedWidget;

    QLabel* m_importPreviewLabel;
    QCheckBox* m_removeBackgroundCheck;
    QImage m_importedRawImage;
    QString m_importedBaseName;
};

} // namespace papyrus
