#include "text_edit_dialog.h"

#include <QEvent>
#include <QFile>
#include <QFileInfo>
#include <QGraphicsRectItem>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QVBoxLayout>

namespace papyrus {

namespace {
constexpr int kRenderWidthPx = 1000;
constexpr double kHitTolerancePoints = 4.0;
} // namespace

TextEditDialog::TextEditDialog(const QString& filePath, int pageIndex, QWidget* parent)
    : QDialog(parent),
      m_filePath(filePath),
      m_previewPath(filePath + QStringLiteral(".papyrus-preview-tmp")),
      m_pageIndex(pageIndex),
      m_view(new QGraphicsView(this)),
      m_scene(new QGraphicsScene(this)),
      m_textEdit(new QLineEdit(this)),
      m_applyButton(new QPushButton(tr("Appliquer"), this)),
      m_hintLabel(new QLabel(this)) {
    setWindowTitle(tr("Modifier le texte — %1 (page %2)").arg(QFileInfo(filePath).fileName()).arg(pageIndex + 1));
    resize(900, 750);

    m_view->setScene(m_scene);
    m_view->setRenderHint(QPainter::Antialiasing);
    m_view->viewport()->installEventFilter(this);

    m_hintLabel->setText(
        tr("Cliquez sur un mot pour le sélectionner, modifiez-le puis cliquez sur Appliquer. Le texte "
           "original reste présent dans la couche invisible du PDF (recherche/copie) sous le nouveau texte."));
    m_hintLabel->setWordWrap(true);
    m_hintLabel->setStyleSheet(QStringLiteral("color: #666; font-style: italic;"));

    m_textEdit->setEnabled(false);
    m_textEdit->setPlaceholderText(tr("Cliquez sur un mot du document..."));
    connect(m_textEdit, &QLineEdit::returnPressed, this, &TextEditDialog::applyEdit);

    m_applyButton->setEnabled(false);
    connect(m_applyButton, &QPushButton::clicked, this, &TextEditDialog::applyEdit);

    auto* editRow = new QHBoxLayout();
    editRow->addWidget(new QLabel(tr("Texte :"), this));
    editRow->addWidget(m_textEdit, 1);
    editRow->addWidget(m_applyButton);

    auto* saveButton = new QPushButton(tr("Enregistrer"), this);
    saveButton->setDefault(true);
    connect(saveButton, &QPushButton::clicked, this, &TextEditDialog::save);
    auto* closeButton = new QPushButton(tr("Fermer"), this);
    connect(closeButton, &QPushButton::clicked, this, &QDialog::reject);
    auto* bottomRow = new QHBoxLayout();
    bottomRow->addStretch(1);
    bottomRow->addWidget(closeButton);
    bottomRow->addWidget(saveButton);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(m_hintLabel);
    layout->addWidget(m_view, 1);
    layout->addLayout(editRow);
    layout->addLayout(bottomRow);

    if (m_editor.load(filePath) != pdf::EditResult::Ok) {
        QMessageBox::warning(this, tr("Impossible d'ouvrir"), tr("Le document n'a pas pu être chargé."));
        return;
    }
    loadPage(m_filePath);
}

TextEditDialog::~TextEditDialog() {
    QFile::remove(m_previewPath);
}

bool TextEditDialog::loadPage(const QString& sourcePath) {
    if (m_previewDocument.load(sourcePath) != pdf::LoadResult::Ok) {
        QMessageBox::warning(this, tr("Impossible d'ouvrir"), tr("Le document n'a pas pu être chargé."));
        return false;
    }
    const QPixmap pixmap = QPixmap::fromImage(m_previewDocument.renderPage(m_pageIndex, kRenderWidthPx));
    if (pixmap.isNull()) {
        QMessageBox::warning(this, tr("Impossible d'ouvrir"), tr("La page n'a pas pu être rendue."));
        return false;
    }
    m_scene->clear();
    m_selectionItem = nullptr;
    m_scene->addPixmap(pixmap);
    m_scene->setSceneRect(pixmap.rect());

    const QSizeF pageSize = m_previewDocument.pagePointSize(m_pageIndex);
    m_pointsPerScenePixel = pageSize.width() / pixmap.width();
    return true;
}

void TextEditDialog::refreshPreview() {
    if (m_editor.save(m_previewPath) != pdf::EditResult::Ok) {
        return;
    }
    loadPage(m_previewPath);
}

bool TextEditDialog::eventFilter(QObject* watched, QEvent* event) {
    if (watched != m_view->viewport() || event->type() != QEvent::MouseButtonPress) {
        return QDialog::eventFilter(watched, event);
    }
    auto* mouseEvent = static_cast<QMouseEvent*>(event);
    if (mouseEvent->button() != Qt::LeftButton) {
        return false;
    }
    handleClick(m_view->mapToScene(mouseEvent->pos()));
    return true;
}

void TextEditDialog::handleClick(const QPointF& scenePos) {
    const QPointF pdfPoint(scenePos.x() * m_pointsPerScenePixel, scenePos.y() * m_pointsPerScenePixel);
    auto run = m_editor.runAt(m_pageIndex, pdfPoint, kHitTolerancePoints);
    if (!run) {
        return;
    }
    m_selectedRun = run;

    const QPair<int, int> key(run->startCharIndex, run->endCharIndex);
    const auto edited = m_editedRuns.constFind(key);
    m_textEdit->setText(edited != m_editedRuns.constEnd() ? edited.value() : run->text);
    m_textEdit->setEnabled(true);
    m_textEdit->setFocus();
    m_textEdit->selectAll();
    m_applyButton->setEnabled(true);

    if (m_selectionItem) {
        m_scene->removeItem(m_selectionItem);
        delete m_selectionItem;
    }
    const QRectF sceneRect(run->boundsPoints.left() / m_pointsPerScenePixel,
                            run->boundsPoints.top() / m_pointsPerScenePixel,
                            run->boundsPoints.width() / m_pointsPerScenePixel,
                            run->boundsPoints.height() / m_pointsPerScenePixel);
    m_selectionItem = m_scene->addRect(sceneRect.adjusted(-2, -2, 2, 2), QPen(QColor(30, 120, 220), 2, Qt::DashLine));
}

void TextEditDialog::applyEdit() {
    if (!m_selectedRun) {
        return;
    }
    const QString newText = m_textEdit->text();
    if (!m_editor.replaceText(m_pageIndex, *m_selectedRun, newText)) {
        QMessageBox::warning(this, tr("Échec"), tr("Impossible de modifier ce texte."));
        return;
    }
    m_editedRuns.insert(QPair<int, int>(m_selectedRun->startCharIndex, m_selectedRun->endCharIndex), newText);
    m_hasPendingEdits = true;
    m_selectedRun.reset();
    m_textEdit->clear();
    m_textEdit->setEnabled(false);
    m_applyButton->setEnabled(false);
    if (m_selectionItem) {
        m_scene->removeItem(m_selectionItem);
        delete m_selectionItem;
        m_selectionItem = nullptr;
    }
    refreshPreview();
}

void TextEditDialog::save() {
    if (!m_hasPendingEdits) {
        QMessageBox::information(this, tr("Rien à enregistrer"), tr("Modifiez au moins un mot d'abord."));
        return;
    }

    const QString tempPath = m_filePath + QStringLiteral(".papyrus-tmp");
    if (m_editor.save(tempPath) != pdf::EditResult::Ok) {
        QFile::remove(tempPath);
        QMessageBox::warning(this, tr("Échec"), tr("L'enregistrement a échoué."));
        return;
    }
    QFile::remove(m_filePath);
    if (!QFile::rename(tempPath, m_filePath)) {
        QMessageBox::warning(this, tr("Échec"), tr("Impossible de remplacer le fichier d'origine."));
        return;
    }

    emit documentSaved(m_filePath);
    accept();
}

} // namespace papyrus
