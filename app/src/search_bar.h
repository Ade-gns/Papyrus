#pragma once

#include <QWidget>

QT_BEGIN_NAMESPACE
class QLineEdit;
class QLabel;
class QToolButton;
QT_END_NAMESPACE

namespace papyrus {

// Slim, dismissible search bar shown above a document's view (Ctrl+F style,
// like a browser's find-in-page). Purely a UI component: it emits requests
// and displays a result count, all search logic lives in DocumentTab.
class SearchBar : public QWidget {
    Q_OBJECT
public:
    explicit SearchBar(QWidget* parent = nullptr);

    void focusInput();
    void setResultText(const QString& text);

signals:
    void searchTextChanged(const QString& text);
    void nextRequested();
    void previousRequested();
    void highlightRequested();
    void closed();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    QLineEdit* m_input;
    QLabel* m_resultLabel;
    QToolButton* m_previousButton;
    QToolButton* m_nextButton;
};

} // namespace papyrus
