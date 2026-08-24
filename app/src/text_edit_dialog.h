#pragma once

#include "papyrus/pdf/document.h"
#include "papyrus/pdf/text_editor.h"

#include <QDialog>
#include <QHash>
#include <QPair>

#include <optional>

QT_BEGIN_NAMESPACE
class QGraphicsView;
class QGraphicsScene;
class QGraphicsRectItem;
class QLineEdit;
class QPushButton;
class QLabel;
QT_END_NAMESPACE

namespace papyrus {

// "Edit text" dialog: click a word on a rendered page to select it (hit-test
// via TextEditor::runAt), edit it in a field, Apply to bake the change into
// the in-memory document. Several words can be edited before one final Save.
//
// TextEditor::replaceText never removes the original text object — real
// PDFs (confirmed: French government forms) often wrap all page content in
// a single Form XObject, and removing an object nested inside one was found
// not to reliably survive save+reload. Instead it covers the original
// glyphs with a background-matched rectangle and draws the replacement as a
// fresh page-level text object on top. That means char indices returned by
// runAt() stay valid across repeated edits (nothing is ever removed), which
// is what lets this dialog let a word be re-edited: m_editedRuns remembers
// the last applied text per original (start,end) range and shows that
// instead of the now-hidden original when the same spot is clicked again.
// The trade-off — the original wording is still present in the PDF's text
// layer, just visually covered, so copy/search can still surface it — is
// disclosed to the user via m_hintLabel rather than hidden.
class TextEditDialog : public QDialog {
    Q_OBJECT
public:
    explicit TextEditDialog(const QString& filePath, int pageIndex, QWidget* parent = nullptr);
    ~TextEditDialog() override;

signals:
    void documentSaved(const QString& filePath);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    bool loadPage(const QString& sourcePath);
    void refreshPreview();
    void handleClick(const QPointF& scenePos);
    void applyEdit();
    void save();

    QString m_filePath;
    QString m_previewPath;
    int m_pageIndex;
    pdf::TextEditor m_editor;
    pdf::Document m_previewDocument;
    double m_pointsPerScenePixel = 1.0;
    bool m_hasPendingEdits = false;

    std::optional<pdf::TextRun> m_selectedRun;
    QHash<QPair<int, int>, QString> m_editedRuns;

    QGraphicsView* m_view;
    QGraphicsScene* m_scene;
    QGraphicsRectItem* m_selectionItem = nullptr;
    QLineEdit* m_textEdit;
    QPushButton* m_applyButton;
    QLabel* m_hintLabel;
};

} // namespace papyrus
