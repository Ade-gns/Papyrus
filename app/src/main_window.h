#pragma once

#include <QMainWindow>

QT_BEGIN_NAMESPACE
class QTabWidget;
class QSpinBox;
class QLabel;
class QMenu;
QT_END_NAMESPACE

namespace papyrus {

class DocumentTab;
class ThumbnailPanel;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);

public slots:
    void openFile(const QString& filePath);

protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private:
    void createActions();
    void createToolBar();
    void createDocumentDock();
    void promptOpenFile();
    void addRecentFile(const QString& filePath);
    void rebuildRecentFilesMenu();
    void closeTab(int index);
    void onCurrentTabChanged(int index);
    void openPageManager();
    void openAnnotationDialog();
    void openTextToPdfDialog();
    void openImagesToPdfDialog();
    void openOfficeConversionDialog();
    void openSignatureDialog();
    void openOcrDialog();
    void openFormFillDialog();
    void mergeDocuments();
    void reloadTabForFile(const QString& filePath);
    DocumentTab* currentTab() const;
    void updatePageControls();
    void updateWindowTitle();

    QTabWidget* m_tabs;
    ThumbnailPanel* m_thumbnailPanel;
    QMenu* m_recentFilesMenu = nullptr;

    QSpinBox* m_pageSpinBox;
    QLabel* m_pageCountLabel;
    QLabel* m_zoomLabel;
};

} // namespace papyrus
