#include "search_bar.h"

#include <QEvent>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QToolButton>

namespace papyrus {

SearchBar::SearchBar(QWidget* parent)
    : QWidget(parent),
      m_input(new QLineEdit(this)),
      m_resultLabel(new QLabel(this)),
      m_previousButton(new QToolButton(this)),
      m_nextButton(new QToolButton(this)) {
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(6, 4, 6, 4);

    m_input->setPlaceholderText(tr("Rechercher dans le document..."));
    m_input->setClearButtonEnabled(true);
    m_input->installEventFilter(this);
    layout->addWidget(m_input, 1);

    m_resultLabel->setMinimumWidth(70);
    m_resultLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_resultLabel);

    m_previousButton->setText(QStringLiteral("▲"));
    m_previousButton->setToolTip(tr("Résultat précédent (Maj+Entrée)"));
    layout->addWidget(m_previousButton);

    m_nextButton->setText(QStringLiteral("▼"));
    m_nextButton->setToolTip(tr("Résultat suivant (Entrée)"));
    layout->addWidget(m_nextButton);

    auto* highlightButton = new QToolButton(this);
    highlightButton->setText(tr("🖍 Surligner"));
    highlightButton->setToolTip(tr("Surligner le résultat courant dans le document"));
    layout->addWidget(highlightButton);

    auto* closeButton = new QToolButton(this);
    closeButton->setText(QStringLiteral("✕"));
    closeButton->setToolTip(tr("Fermer (Échap)"));
    layout->addWidget(closeButton);

    connect(m_input, &QLineEdit::textChanged, this, &SearchBar::searchTextChanged);
    connect(m_nextButton, &QToolButton::clicked, this, &SearchBar::nextRequested);
    connect(m_previousButton, &QToolButton::clicked, this, &SearchBar::previousRequested);
    connect(highlightButton, &QToolButton::clicked, this, &SearchBar::highlightRequested);
    connect(closeButton, &QToolButton::clicked, this, &SearchBar::closed);

    setResultText(QString());
}

void SearchBar::focusInput() {
    m_input->setFocus();
    m_input->selectAll();
}

void SearchBar::setResultText(const QString& text) {
    m_resultLabel->setText(text);
}

bool SearchBar::eventFilter(QObject* watched, QEvent* event) {
    if (watched == m_input && event->type() == QEvent::KeyPress) {
        auto* keyEvent = static_cast<QKeyEvent*>(event);
        if (keyEvent->key() == Qt::Key_Escape) {
            emit closed();
            return true;
        }
        if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) {
            if (keyEvent->modifiers() & Qt::ShiftModifier) {
                emit previousRequested();
            } else {
                emit nextRequested();
            }
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

} // namespace papyrus
