#include "page_manager_dialog.h"

#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QTransform>
#include <QVBoxLayout>

#include <algorithm>

namespace papyrus {

namespace {
constexpr int kIconWidth = pdf::ThumbnailCache::kWidth;
constexpr int kIconHeight = kIconWidth * 14 / 10; // roughly A4 portrait
} // namespace

PageManagerDialog::PageManagerDialog(const QString& filePath, QWidget* parent)
    : QDialog(parent), m_list(new QListWidget(this)) {
    setWindowTitle(tr("Organiser les pages — %1").arg(QFileInfo(filePath).fileName()));
    resize(760, 560);

    m_list->setViewMode(QListView::IconMode);
    m_list->setIconSize(QSize(kIconWidth, kIconHeight));
    m_list->setResizeMode(QListView::Adjust);
    m_list->setSpacing(10);
    m_list->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_list->setDragDropMode(QAbstractItemView::NoDragDrop);

    auto* editToolBar = new QHBoxLayout();
    auto addButton = [&](const QString& label, auto slot) {
        auto* button = new QPushButton(label, this);
        connect(button, &QPushButton::clicked, this, slot);
        editToolBar->addWidget(button);
        return button;
    };
    m_requiresSelectionButtons.append(addButton(tr("↺ Pivoter à gauche"), [this] { rotateSelected(-1); }));
    m_requiresSelectionButtons.append(addButton(tr("↻ Pivoter à droite"), [this] { rotateSelected(1); }));
    m_requiresSelectionButtons.append(addButton(tr("Dupliquer"), &PageManagerDialog::duplicateSelected));
    m_requiresSelectionButtons.append(addButton(tr("Supprimer"), &PageManagerDialog::deleteSelected));
    m_requiresSingleSelectionButtons.append(addButton(tr("▲ Monter"), [this] { moveSelected(-1); }));
    m_requiresSingleSelectionButtons.append(addButton(tr("▼ Descendre"), [this] { moveSelected(1); }));
    editToolBar->addStretch(1);
    m_requiresSelectionButtons.append(
        addButton(tr("Extraire la sélection..."), &PageManagerDialog::extractSelected));

    auto* bottomBar = new QHBoxLayout();
    bottomBar->addStretch(1);
    addButton(tr("Enregistrer sous..."), &PageManagerDialog::saveAs);
    auto* saveButton = addButton(tr("Enregistrer"), &PageManagerDialog::saveInPlace);
    saveButton->setDefault(true);
    auto* closeButton = new QPushButton(tr("Fermer"), this);
    connect(closeButton, &QPushButton::clicked, this, &QDialog::accept);
    bottomBar->addWidget(closeButton);

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(editToolBar);
    layout->addWidget(m_list, 1);
    layout->addLayout(bottomBar);

    connect(m_list, &QListWidget::itemSelectionChanged, this, &PageManagerDialog::updateActionsEnabled);

    if (loadDocument(filePath)) {
        populateList();
    }
    updateActionsEnabled();
}

bool PageManagerDialog::loadDocument(const QString& filePath) {
    if (m_previewDocument.load(filePath) != pdf::LoadResult::Ok) {
        QMessageBox::warning(this, tr("Impossible d'ouvrir"),
                              tr("Le document n'a pas pu être ouvert pour édition."));
        return false;
    }
    if (m_editor.load(filePath) != pdf::EditResult::Ok) {
        QMessageBox::warning(this, tr("Impossible d'ouvrir"),
                              tr("Le moteur d'édition n'a pas pu ouvrir ce document."));
        return false;
    }
    m_filePath = filePath;
    m_thumbnailCache = std::make_unique<pdf::ThumbnailCache>(&m_previewDocument, this);
    connect(m_thumbnailCache.get(), &pdf::ThumbnailCache::thumbnailReady, this,
            [this](int sourceIndex, const QPixmap&) {
                for (int row = 0; row < m_list->count(); ++row) {
                    QListWidgetItem* item = m_list->item(row);
                    if (item->data(SourceIndexRole).toInt() == sourceIndex) {
                        applyItemIcon(item);
                    }
                }
            });
    return true;
}

void PageManagerDialog::populateList() {
    m_list->clear();
    for (int i = 0; i < m_previewDocument.pageCount(); ++i) {
        auto* item = new QListWidgetItem(tr("Page %1").arg(i + 1));
        item->setData(SourceIndexRole, i);
        item->setData(RotationRole, 0);
        m_list->addItem(item);
        applyItemIcon(item);
    }
}

void PageManagerDialog::applyItemIcon(QListWidgetItem* item) {
    const int sourceIndex = item->data(SourceIndexRole).toInt();
    const QPixmap pixmap = m_thumbnailCache->thumbnail(sourceIndex);
    if (pixmap.isNull()) {
        return; // not rendered yet; thumbnailReady will call us again
    }
    const int rotation = item->data(RotationRole).toInt();
    item->setIcon(QIcon(rotation == 0 ? pixmap : pixmap.transformed(QTransform().rotate(rotation * 90))));
}

std::vector<int> PageManagerDialog::selectedRows() const {
    std::vector<int> rows;
    for (QListWidgetItem* item : m_list->selectedItems()) {
        rows.push_back(m_list->row(item));
    }
    std::sort(rows.begin(), rows.end());
    return rows;
}

void PageManagerDialog::rotateSelected(int quarterTurns) {
    for (QListWidgetItem* item : m_list->selectedItems()) {
        int rotation = item->data(RotationRole).toInt();
        rotation = ((rotation + quarterTurns) % 4 + 4) % 4;
        item->setData(RotationRole, rotation);
        applyItemIcon(item);
    }
}

void PageManagerDialog::deleteSelected() {
    std::vector<int> rows = selectedRows();
    for (auto it = rows.rbegin(); it != rows.rend(); ++it) {
        delete m_list->takeItem(*it);
    }
    for (int row = 0; row < m_list->count(); ++row) {
        m_list->item(row)->setText(tr("Page %1").arg(row + 1));
    }
    updateActionsEnabled();
}

void PageManagerDialog::duplicateSelected() {
    std::vector<int> rows = selectedRows();
    for (auto it = rows.rbegin(); it != rows.rend(); ++it) {
        QListWidgetItem* source = m_list->item(*it);
        auto* copy = new QListWidgetItem(source->text());
        copy->setData(SourceIndexRole, source->data(SourceIndexRole));
        copy->setData(RotationRole, source->data(RotationRole));
        copy->setIcon(source->icon());
        m_list->insertItem(*it + 1, copy);
    }
    for (int row = 0; row < m_list->count(); ++row) {
        m_list->item(row)->setText(tr("Page %1").arg(row + 1));
    }
}

void PageManagerDialog::moveSelected(int direction) {
    const std::vector<int> rows = selectedRows();
    if (rows.size() != 1) {
        return;
    }
    const int row = rows.front();
    const int target = row + direction;
    if (target < 0 || target >= m_list->count()) {
        return;
    }
    QListWidgetItem* item = m_list->takeItem(row);
    m_list->insertItem(target, item);
    m_list->setCurrentItem(item);
    for (int r = 0; r < m_list->count(); ++r) {
        m_list->item(r)->setText(tr("Page %1").arg(r + 1));
    }
}

void PageManagerDialog::syncEditorFromList() {
    std::vector<int> sourceIndices;
    std::vector<int> rotations;
    sourceIndices.reserve(m_list->count());
    rotations.reserve(m_list->count());
    for (int row = 0; row < m_list->count(); ++row) {
        QListWidgetItem* item = m_list->item(row);
        sourceIndices.push_back(item->data(SourceIndexRole).toInt());
        rotations.push_back(item->data(RotationRole).toInt());
    }
    m_editor.setEntries(sourceIndices, rotations);
}

void PageManagerDialog::extractSelected() {
    const std::vector<int> rows = selectedRows();
    if (rows.empty()) {
        return;
    }
    const QString path = QFileDialog::getSaveFileName(this, tr("Extraire vers"), {}, tr("PDF (*.pdf)"));
    if (path.isEmpty()) {
        return;
    }
    syncEditorFromList();
    const pdf::EditResult result = m_editor.extractPages(rows, path);
    if (result != pdf::EditResult::Ok) {
        QMessageBox::warning(this, tr("Échec"), tr("L'extraction a échoué."));
        return;
    }
    QMessageBox::information(this, tr("Extraction réussie"),
                              tr("%1 page(s) extraite(s) vers %2.").arg(rows.size()).arg(path));
}

void PageManagerDialog::saveAs() {
    const QString path =
        QFileDialog::getSaveFileName(this, tr("Enregistrer sous"), m_filePath, tr("PDF (*.pdf)"));
    if (path.isEmpty()) {
        return;
    }
    syncEditorFromList();
    const pdf::EditResult result = m_editor.save(path);
    if (result != pdf::EditResult::Ok) {
        QMessageBox::warning(this, tr("Échec"), tr("L'enregistrement a échoué."));
        return;
    }
    QMessageBox::information(this, tr("Enregistré"), tr("Document enregistré sous %1.").arg(path));
}

void PageManagerDialog::saveInPlace() {
    if (QMessageBox::question(this, tr("Enregistrer"),
                               tr("Remplacer définitivement « %1 » par cette version ?")
                                   .arg(QFileInfo(m_filePath).fileName())) != QMessageBox::Yes) {
        return;
    }

    syncEditorFromList();
    const QString tempPath = m_filePath + QStringLiteral(".papyrus-tmp");
    if (m_editor.save(tempPath) != pdf::EditResult::Ok) {
        QMessageBox::warning(this, tr("Échec"), tr("L'enregistrement a échoué."));
        QFile::remove(tempPath);
        return;
    }
    QFile::remove(m_filePath);
    if (!QFile::rename(tempPath, m_filePath)) {
        QMessageBox::warning(this, tr("Échec"), tr("Impossible de remplacer le fichier d'origine."));
        return;
    }

    if (loadDocument(m_filePath)) {
        populateList();
    }
    emit documentSaved(m_filePath);
}

void PageManagerDialog::updateActionsEnabled() {
    const int count = m_list->selectedItems().count();
    for (QPushButton* button : m_requiresSelectionButtons) {
        button->setEnabled(count > 0);
    }
    for (QPushButton* button : m_requiresSingleSelectionButtons) {
        button->setEnabled(count == 1);
    }
}

} // namespace papyrus
