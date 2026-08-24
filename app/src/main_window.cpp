#include "main_window.h"

#include "annotation_dialog.h"
#include "document_printer.h"
#include "document_tab.h"
#include "images_to_pdf_dialog.h"
#include "form_fill_dialog.h"
#include "ocr_dialog.h"
#include "office_conversion_dialog.h"
#include "page_manager_dialog.h"
#include "signature_dialog.h"
#include "text_to_pdf_dialog.h"
#include "thumbnail_panel.h"
#include "update_checker.h"

#include "papyrus/pdf/document_history.h"
#include "papyrus/pdf/page_editor.h"

#include <QAction>
#include <QCloseEvent>
#include <QDockWidget>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QPrintDialog>
#include <QPrintPreviewDialog>
#include <QPrinter>
#include <QSettings>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStatusBar>
#include <QTabWidget>
#include <QTimer>
#include <QToolBar>

namespace papyrus {

namespace {
constexpr int kMaxRecentFiles = 10;
const char* kRecentFilesKey = "recentFiles";
} // namespace

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent), m_tabs(new QTabWidget(this)), m_thumbnailPanel(new ThumbnailPanel(this)),
      m_updateChecker(new UpdateChecker(this)) {
    setWindowTitle(QStringLiteral("Papyrus"));
    setAcceptDrops(true);

    m_tabs->setTabsClosable(true);
    m_tabs->setDocumentMode(true);
    setCentralWidget(m_tabs);

    connect(m_tabs, &QTabWidget::tabCloseRequested, this, &MainWindow::closeTab);
    connect(m_tabs, &QTabWidget::currentChanged, this, &MainWindow::onCurrentTabChanged);

    createDocumentDock();
    createActions();
    createToolBar();
    offerCrashRecovery();

    statusBar()->showMessage(tr("Prêt"));
    updatePageControls();

    // Deferred so it never delays showing the window; silent (no dialog
    // unless an update is actually found) so it never bothers a user who's
    // already up to date.
    QTimer::singleShot(3000, m_updateChecker, &UpdateChecker::checkSilently);
}

void MainWindow::closeEvent(QCloseEvent* event) {
    QSettings settings;
    settings.setValue(QStringLiteral("cleanShutdown"), true);
    QMainWindow::closeEvent(event);
}

void MainWindow::createDocumentDock() {
    auto* dock = new QDockWidget(tr("Pages"), this);
    dock->setWidget(m_thumbnailPanel);
    dock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetClosable);
    addDockWidget(Qt::LeftDockWidgetArea, dock);

    connect(m_thumbnailPanel, &ThumbnailPanel::pageActivated, this, [this](int page) {
        if (auto* tab = currentTab()) {
            tab->goToPage(page);
        }
    });
}

void MainWindow::createActions() {
    auto* fileMenu = menuBar()->addMenu(tr("&Fichier"));

    auto* openAction = fileMenu->addAction(tr("&Ouvrir..."), QKeySequence::Open, this,
                                            &MainWindow::promptOpenFile);
    Q_UNUSED(openAction);

    m_recentFilesMenu = fileMenu->addMenu(tr("Récemment ouverts"));
    rebuildRecentFilesMenu();

    fileMenu->addSeparator();
    fileMenu->addAction(tr("&Imprimer..."), QKeySequence::Print, this, &MainWindow::openPrintDialog);
    fileMenu->addAction(tr("Aperçu avant impression..."), this, &MainWindow::openPrintPreview);

    fileMenu->addSeparator();
    fileMenu->addAction(tr("Organiser les pages..."), this, &MainWindow::openPageManager);
    fileMenu->addAction(tr("Annoter cette page..."), this, &MainWindow::openAnnotationDialog);
    fileMenu->addAction(tr("Signer le document..."), this, &MainWindow::openSignatureDialog);
    fileMenu->addAction(tr("Rendre ce document recherchable (OCR)..."), this,
                         &MainWindow::openOcrDialog);
    fileMenu->addAction(tr("Remplir le formulaire..."), this, &MainWindow::openFormFillDialog);
    fileMenu->addAction(tr("Fusionner des documents..."), this, &MainWindow::mergeDocuments);

    fileMenu->addSeparator();
    fileMenu->addAction(tr("Créer un PDF depuis un texte..."), this, &MainWindow::openTextToPdfDialog);
    fileMenu->addAction(tr("Créer un PDF depuis des images..."), this, &MainWindow::openImagesToPdfDialog);
    fileMenu->addAction(tr("Convertir des documents Office..."), this,
                         &MainWindow::openOfficeConversionDialog);

    fileMenu->addSeparator();
    fileMenu->addAction(tr("&Quitter"), QKeySequence::Quit, this, &QWidget::close);

    auto* editMenu = menuBar()->addMenu(tr("&Édition"));
    m_undoAction = editMenu->addAction(tr("Annuler"), QKeySequence::Undo, this, &MainWindow::undo);
    m_redoAction = editMenu->addAction(tr("Rétablir"), QKeySequence::Redo, this, &MainWindow::redo);
    m_undoAction->setEnabled(false);
    m_redoAction->setEnabled(false);

    auto* helpMenu = menuBar()->addMenu(tr("&Aide"));
    helpMenu->addAction(tr("Vérifier les mises à jour..."), this,
                         [this] { m_updateChecker->checkInteractively(); });
}

void MainWindow::createToolBar() {
    auto* toolBar = addToolBar(tr("Principal"));
    toolBar->setMovable(false);

    toolBar->addAction(tr("Ouvrir"), QKeySequence::Open, this, &MainWindow::promptOpenFile);
    toolBar->addSeparator();

    toolBar->addAction(tr("−"), QKeySequence::ZoomOut, this, [this] {
        if (auto* tab = currentTab()) tab->zoomOut();
    });
    m_zoomLabel = new QLabel(QStringLiteral("100%"), this);
    m_zoomLabel->setMinimumWidth(50);
    m_zoomLabel->setAlignment(Qt::AlignCenter);
    toolBar->addWidget(m_zoomLabel);
    toolBar->addAction(tr("+"), QKeySequence::ZoomIn, this, [this] {
        if (auto* tab = currentTab()) tab->zoomIn();
    });
    toolBar->addAction(tr("Ajuster à la largeur"), this, [this] {
        if (auto* tab = currentTab()) tab->fitToWidth();
    });

    toolBar->addSeparator();
    toolBar->addAction(tr("◀"), QKeySequence::MoveToPreviousPage, this, [this] {
        if (auto* tab = currentTab()) tab->previousPage();
    });

    m_pageSpinBox = new QSpinBox(this);
    m_pageSpinBox->setMinimum(1);
    m_pageSpinBox->setMaximum(1);
    connect(m_pageSpinBox, &QSpinBox::valueChanged, this, [this](int value) {
        if (auto* tab = currentTab()) tab->goToPage(value - 1);
    });
    toolBar->addWidget(m_pageSpinBox);

    m_pageCountLabel = new QLabel(QStringLiteral("/ 1"), this);
    toolBar->addWidget(m_pageCountLabel);

    toolBar->addAction(tr("▶"), QKeySequence::MoveToNextPage, this, [this] {
        if (auto* tab = currentTab()) tab->nextPage();
    });

    toolBar->addSeparator();
    toolBar->addAction(tr("Rechercher"), QKeySequence::Find, this, [this] {
        if (auto* tab = currentTab()) tab->toggleSearchBar();
    });
}

void MainWindow::promptOpenFile() {
    const QString filePath = QFileDialog::getOpenFileName(this, tr("Ouvrir un document"), {},
                                                            tr("Documents PDF (*.pdf)"));
    if (!filePath.isEmpty()) {
        openFile(filePath);
    }
}

void MainWindow::openFile(const QString& filePath) {
    if (!QFileInfo::exists(filePath)) {
        QMessageBox::warning(this, tr("Fichier introuvable"),
                              tr("Le fichier « %1 » n'existe pas.").arg(filePath));
        return;
    }

    auto* tab = new DocumentTab(m_tabs);
    const pdf::LoadResult result = tab->load(filePath);
    if (result != pdf::LoadResult::Ok) {
        delete tab;
        const QString reason = result == pdf::LoadResult::PasswordProtected
                                    ? tr("ce document est protégé par un mot de passe.")
                                    : tr("le fichier est invalide ou corrompu.");
        QMessageBox::warning(this, tr("Impossible d'ouvrir le document"), reason);
        return;
    }

    connect(tab, &DocumentTab::currentPageChanged, this, [this, tab](int page) {
        if (currentTab() == tab) {
            updatePageControls();
            m_thumbnailPanel->setCurrentPage(page);
        }
    });
    connect(tab, &DocumentTab::zoomFactorChanged, this, [this, tab](qreal) {
        if (currentTab() == tab) {
            updatePageControls();
        }
    });
    connect(tab, &DocumentTab::annotationsSaved, this, &MainWindow::reloadTabForFile);

    const int index = m_tabs->addTab(tab, tab->displayName());
    m_tabs->setCurrentIndex(index);
    addRecentFile(filePath);
    trackTabOpened(filePath);
}

void MainWindow::closeTab(int index) {
    QWidget* widget = m_tabs->widget(index);
    if (auto* tab = qobject_cast<DocumentTab*>(widget)) {
        trackTabClosed(tab->filePath());
    }
    m_tabs->removeTab(index);
    widget->deleteLater();
}

void MainWindow::openPrintDialog() {
    DocumentTab* tab = currentTab();
    if (!tab) {
        return;
    }

    QPrinter printer(QPrinter::HighResolution);
    printer.setDocName(tab->displayName());
    printer.setFromTo(1, tab->pageCount());

    QPrintDialog dialog(&printer, this);
    dialog.setOption(QAbstractPrintDialog::PrintPageRange);
    dialog.setOption(QAbstractPrintDialog::PrintCurrentPage);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    printDocument(*tab->document(), printer, tab->currentPage());
}

void MainWindow::openPrintPreview() {
    DocumentTab* tab = currentTab();
    if (!tab) {
        return;
    }

    QPrinter printer(QPrinter::HighResolution);
    printer.setDocName(tab->displayName());

    QPrintPreviewDialog preview(&printer, this);
    connect(&preview, &QPrintPreviewDialog::paintRequested, this, [tab](QPrinter* p) {
        printDocument(*tab->document(), *p, tab->currentPage());
    });
    preview.exec();
}

void MainWindow::openPageManager() {
    DocumentTab* tab = currentTab();
    if (!tab) {
        return;
    }
    const QString filePath = tab->filePath();
    const QByteArray before = pdf::DocumentHistory::capture(filePath);

    auto* dialog = new PageManagerDialog(filePath, this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    connect(dialog, &PageManagerDialog::documentSaved, this, [this, before](const QString& path) {
        pdf::DocumentHistory::commit(path, before);
        reloadTabForFile(path);
    });
    dialog->exec();
}

void MainWindow::openAnnotationDialog() {
    DocumentTab* tab = currentTab();
    if (!tab) {
        return;
    }
    const QByteArray before = pdf::DocumentHistory::capture(tab->filePath());
    auto* dialog = new AnnotationDialog(tab->filePath(), tab->currentPage(), this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    connect(dialog, &AnnotationDialog::documentSaved, this, [this, before](const QString& path) {
        pdf::DocumentHistory::commit(path, before);
        reloadTabForFile(path);
    });
    dialog->exec();
}

void MainWindow::reloadTabForFile(const QString& filePath) {
    for (int i = 0; i < m_tabs->count(); ++i) {
        if (auto* tab = qobject_cast<DocumentTab*>(m_tabs->widget(i)); tab && tab->filePath() == filePath) {
            closeTab(i);
            openFile(filePath);
            return;
        }
    }
}

void MainWindow::openSignatureDialog() {
    DocumentTab* tab = currentTab();
    if (!tab) {
        return;
    }
    const QByteArray before = pdf::DocumentHistory::capture(tab->filePath());
    auto* dialog = new SignatureDialog(tab->filePath(), tab->currentPage(), this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    connect(dialog, &SignatureDialog::documentSaved, this, [this, before](const QString& path) {
        pdf::DocumentHistory::commit(path, before);
        reloadTabForFile(path);
    });
    dialog->exec();
}

void MainWindow::openFormFillDialog() {
    DocumentTab* tab = currentTab();
    if (!tab) {
        return;
    }
    const QByteArray before = pdf::DocumentHistory::capture(tab->filePath());
    auto* dialog = new FormFillDialog(tab->filePath(), this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    connect(dialog, &FormFillDialog::documentSaved, this, [this, before](const QString& path) {
        pdf::DocumentHistory::commit(path, before);
        reloadTabForFile(path);
    });
    dialog->exec();
}

void MainWindow::openOcrDialog() {
    DocumentTab* tab = currentTab();
    if (!tab) {
        return;
    }
    auto* dialog = new OcrDialog(tab->filePath(), this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    connect(dialog, &OcrDialog::pdfCreated, this, &MainWindow::openFile);
    dialog->exec();
}

void MainWindow::openTextToPdfDialog() {
    auto* dialog = new TextToPdfDialog(this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    connect(dialog, &TextToPdfDialog::pdfCreated, this, &MainWindow::openFile);
    dialog->exec();
}

void MainWindow::openImagesToPdfDialog() {
    auto* dialog = new ImagesToPdfDialog(this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    connect(dialog, &ImagesToPdfDialog::pdfCreated, this, &MainWindow::openFile);
    dialog->exec();
}

void MainWindow::openOfficeConversionDialog() {
    auto* dialog = new OfficeConversionDialog(this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    connect(dialog, &OfficeConversionDialog::pdfsCreated, this, [this](const QStringList& paths) {
        for (const QString& path : paths) {
            openFile(path);
        }
    });
    dialog->exec();
}

void MainWindow::mergeDocuments() {
    const QStringList files = QFileDialog::getOpenFileNames(this, tr("Choisir les documents à fusionner"),
                                                              {}, tr("Documents PDF (*.pdf)"));
    if (files.size() < 2) {
        return;
    }
    const QString output = QFileDialog::getSaveFileName(this, tr("Enregistrer le document fusionné"), {},
                                                          tr("PDF (*.pdf)"));
    if (output.isEmpty()) {
        return;
    }

    std::vector<QString> inputs(files.begin(), files.end());
    const pdf::EditResult result = pdf::PageEditor::mergeFiles(inputs, output);
    if (result != pdf::EditResult::Ok) {
        QMessageBox::warning(this, tr("Échec de la fusion"),
                              tr("Impossible de fusionner ces documents."));
        return;
    }
    openFile(output);
}

void MainWindow::onCurrentTabChanged(int index) {
    Q_UNUSED(index);
    m_thumbnailPanel->setDocumentTab(currentTab());
    updatePageControls();
    updateWindowTitle();
    updateUndoRedoActions();
}

void MainWindow::undo() {
    DocumentTab* tab = currentTab();
    if (!tab) {
        return;
    }
    const QString filePath = tab->filePath();
    if (!pdf::DocumentHistory::undo(filePath)) {
        return;
    }
    reloadTabForFile(filePath);
}

void MainWindow::redo() {
    DocumentTab* tab = currentTab();
    if (!tab) {
        return;
    }
    const QString filePath = tab->filePath();
    if (!pdf::DocumentHistory::redo(filePath)) {
        return;
    }
    reloadTabForFile(filePath);
}

void MainWindow::updateUndoRedoActions() {
    DocumentTab* tab = currentTab();
    const bool hasDocument = tab != nullptr;
    m_undoAction->setEnabled(hasDocument && pdf::DocumentHistory::canUndo(tab->filePath()));
    m_redoAction->setEnabled(hasDocument && pdf::DocumentHistory::canRedo(tab->filePath()));
}

void MainWindow::trackTabOpened(const QString& filePath) {
    QSettings settings;
    QStringList open = settings.value(QStringLiteral("openTabs")).toStringList();
    if (!open.contains(filePath)) {
        open.append(filePath);
        settings.setValue(QStringLiteral("openTabs"), open);
    }
}

void MainWindow::trackTabClosed(const QString& filePath) {
    QSettings settings;
    QStringList open = settings.value(QStringLiteral("openTabs")).toStringList();
    if (open.removeAll(filePath) > 0) {
        settings.setValue(QStringLiteral("openTabs"), open);
    }
}

void MainWindow::offerCrashRecovery() {
    QSettings settings;
    const bool cleanShutdown = settings.value(QStringLiteral("cleanShutdown"), true).toBool();
    const QStringList openTabs = settings.value(QStringLiteral("openTabs")).toStringList();
    settings.setValue(QStringLiteral("cleanShutdown"), false);

    if (cleanShutdown) {
        return;
    }

    QStringList existing;
    for (const QString& path : openTabs) {
        if (QFileInfo::exists(path)) {
            existing << path;
        }
    }
    if (existing.isEmpty()) {
        return;
    }

    const auto choice = QMessageBox::question(
        this, tr("Récupération après fermeture inattendue"),
        tr("Papyrus ne s'est pas fermé proprement la dernière fois. Rouvrir les %1 document(s) "
           "qui étaient ouverts ?")
            .arg(existing.size()),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
    if (choice == QMessageBox::Yes) {
        for (const QString& path : existing) {
            openFile(path);
        }
    }
}

DocumentTab* MainWindow::currentTab() const {
    return qobject_cast<DocumentTab*>(m_tabs->currentWidget());
}

void MainWindow::updatePageControls() {
    DocumentTab* tab = currentTab();
    const bool hasDocument = tab != nullptr;

    m_pageSpinBox->setEnabled(hasDocument);
    const int pageCount = hasDocument ? tab->pageCount() : 1;
    {
        const QSignalBlocker blocker(m_pageSpinBox);
        m_pageSpinBox->setMaximum(pageCount);
        m_pageSpinBox->setValue(hasDocument ? tab->currentPage() + 1 : 1);
    }
    m_pageCountLabel->setText(QStringLiteral("/ %1").arg(pageCount));
    m_zoomLabel->setText(QStringLiteral("%1%").arg(hasDocument ? qRound(tab->zoomFactor() * 100) : 100));

    if (hasDocument) {
        m_thumbnailPanel->setCurrentPage(tab->currentPage());
    }
}

void MainWindow::updateWindowTitle() {
    DocumentTab* tab = currentTab();
    setWindowTitle(tab ? QStringLiteral("%1 — Papyrus").arg(tab->displayName())
                        : QStringLiteral("Papyrus"));
}

void MainWindow::addRecentFile(const QString& filePath) {
    QSettings settings;
    QStringList recent = settings.value(kRecentFilesKey).toStringList();
    recent.removeAll(filePath);
    recent.prepend(filePath);
    while (recent.size() > kMaxRecentFiles) {
        recent.removeLast();
    }
    settings.setValue(kRecentFilesKey, recent);
    rebuildRecentFilesMenu();
}

void MainWindow::rebuildRecentFilesMenu() {
    m_recentFilesMenu->clear();
    const QSettings settings;
    const QStringList recent = settings.value(kRecentFilesKey).toStringList();

    if (recent.isEmpty()) {
        QAction* empty = m_recentFilesMenu->addAction(tr("(aucun)"));
        empty->setEnabled(false);
        return;
    }

    for (const QString& filePath : recent) {
        m_recentFilesMenu->addAction(QFileInfo(filePath).fileName(), this,
                                      [this, filePath] { openFile(filePath); });
    }
}

void MainWindow::dragEnterEvent(QDragEnterEvent* event) {
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void MainWindow::dropEvent(QDropEvent* event) {
    for (const QUrl& url : event->mimeData()->urls()) {
        if (url.isLocalFile() && url.toLocalFile().endsWith(".pdf", Qt::CaseInsensitive)) {
            openFile(url.toLocalFile());
        }
    }
}

} // namespace papyrus
