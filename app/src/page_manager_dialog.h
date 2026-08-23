#pragma once

#include "papyrus/pdf/document.h"
#include "papyrus/pdf/page_editor.h"
#include "papyrus/pdf/thumbnail_cache.h"

#include <QDialog>
#include <QList>

#include <memory>

QT_BEGIN_NAMESPACE
class QListWidget;
class QListWidgetItem;
class QPushButton;
QT_END_NAMESPACE

namespace papyrus {

// "Organize pages" dialog for one document: rotate, delete, duplicate,
// reorder (move up/down) and extract pages, then save.
//
// While the dialog is open, every edit only touches the QListWidget itself
// (each item carries its source page index and pending rotation as item
// data) — nothing is pushed to PageEditor until Save/Save As/Extract is
// clicked, which syncs the list's current order into the engine via
// PageEditor::setEntries() first. This avoids reconciling PDFium state
// incrementally against arbitrary list edits.
class PageManagerDialog : public QDialog {
    Q_OBJECT
public:
    explicit PageManagerDialog(const QString& filePath, QWidget* parent = nullptr);

signals:
    // Emitted after a successful save that overwrote filePath, so the caller
    // can reload the corresponding open tab.
    void documentSaved(const QString& filePath);

private:
    enum Roles { SourceIndexRole = Qt::UserRole, RotationRole };

    bool loadDocument(const QString& filePath);
    void populateList();
    void requestThumbnail(QListWidgetItem* item);
    void applyItemIcon(QListWidgetItem* item);

    std::vector<int> selectedRows() const;
    void rotateSelected(int quarterTurns);
    void deleteSelected();
    void duplicateSelected();
    void moveSelected(int direction);
    void extractSelected();
    void saveAs();
    void saveInPlace();
    void syncEditorFromList();
    void updateActionsEnabled();

    QString m_filePath;
    pdf::Document m_previewDocument;
    std::unique_ptr<pdf::ThumbnailCache> m_thumbnailCache;
    pdf::PageEditor m_editor;

    QListWidget* m_list;
    QList<QPushButton*> m_requiresSelectionButtons;
    QList<QPushButton*> m_requiresSingleSelectionButtons;
};

} // namespace papyrus
