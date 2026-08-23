#include "typed_signature_widget.h"

#include "signature_fonts.h"

#include <QCheckBox>
#include <QColorDialog>
#include <QDoubleSpinBox>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPainter>
#include <QSpinBox>
#include <QToolButton>
#include <QVBoxLayout>

namespace papyrus {

namespace {
constexpr int kPreviewPointSize = 12;
}

TypedSignatureWidget::TypedSignatureWidget(QWidget* parent)
    : QWidget(parent),
      m_textEdit(new QLineEdit(this)),
      m_fontList(new QListWidget(this)),
      m_sizeSpin(new QSpinBox(this)),
      m_spacingSpin(new QDoubleSpinBox(this)),
      m_italicCheck(new QCheckBox(tr("Incliner"), this)),
      m_colorButton(new QToolButton(this)),
      m_previewLabel(new QLabel(this)) {
    m_textEdit->setPlaceholderText(tr("Votre nom..."));

    populateFontList();
    m_fontList->setMaximumWidth(220);

    m_sizeSpin->setRange(12, 200);
    m_sizeSpin->setValue(48);

    m_spacingSpin->setRange(-5, 20);
    m_spacingSpin->setValue(0);
    m_spacingSpin->setSuffix(tr(" px"));

    m_colorButton->setText(tr("Couleur"));
    m_colorButton->setStyleSheet(QStringLiteral("background-color: %1").arg(m_color.name()));
    connect(m_colorButton, &QToolButton::clicked, this, [this] {
        const QColor chosen = QColorDialog::getColor(m_color, this, tr("Choisir une couleur"));
        if (chosen.isValid()) {
            m_color = chosen;
            m_colorButton->setStyleSheet(QStringLiteral("background-color: %1").arg(m_color.name()));
            updatePreview();
        }
    });

    auto* controlsRow = new QHBoxLayout();
    controlsRow->addWidget(new QLabel(tr("Taille"), this));
    controlsRow->addWidget(m_sizeSpin);
    controlsRow->addWidget(new QLabel(tr("Espacement"), this));
    controlsRow->addWidget(m_spacingSpin);
    controlsRow->addWidget(m_italicCheck);
    controlsRow->addWidget(m_colorButton);
    controlsRow->addStretch(1);

    m_previewLabel->setMinimumHeight(120);
    m_previewLabel->setAlignment(Qt::AlignCenter);
    m_previewLabel->setStyleSheet(QStringLiteral("background-color: white; border: 1px solid #ccc;"));

    auto* rightColumn = new QVBoxLayout();
    rightColumn->addWidget(m_textEdit);
    rightColumn->addLayout(controlsRow);
    rightColumn->addWidget(m_previewLabel, 1);

    auto* mainRow = new QHBoxLayout(this);
    mainRow->addWidget(m_fontList);
    mainRow->addLayout(rightColumn, 1);

    connect(m_textEdit, &QLineEdit::textChanged, this, [this] {
        updatePreview();
        emit contentChanged();
    });
    connect(m_fontList, &QListWidget::currentRowChanged, this, [this] { updatePreview(); });
    connect(m_sizeSpin, &QSpinBox::valueChanged, this, [this] { updatePreview(); });
    connect(m_spacingSpin, &QDoubleSpinBox::valueChanged, this, [this] { updatePreview(); });
    connect(m_italicCheck, &QCheckBox::toggled, this, [this] { updatePreview(); });

    if (m_fontList->count() > 0) {
        m_fontList->setCurrentRow(0);
    }
    updatePreview();
}

void TypedSignatureWidget::populateFontList() {
    for (const QString& family : bundledSignatureFontFamilies()) {
        auto* item = new QListWidgetItem(family, m_fontList);
        item->setFont(QFont(family, kPreviewPointSize));
    }
}

bool TypedSignatureWidget::isEmpty() const {
    return m_textEdit->text().trimmed().isEmpty();
}

QString TypedSignatureWidget::suggestedName() const {
    const QString text = m_textEdit->text().trimmed();
    return text.isEmpty() ? tr("Signature") : text;
}

QImage TypedSignatureWidget::renderImage() const {
    const QString text = m_textEdit->text().trimmed();
    if (text.isEmpty() || !m_fontList->currentItem()) {
        return {};
    }

    QFont font(m_fontList->currentItem()->text(), m_sizeSpin->value());
    font.setItalic(m_italicCheck->isChecked());
    if (m_spacingSpin->value() != 0) {
        font.setLetterSpacing(QFont::AbsoluteSpacing, m_spacingSpin->value());
    }

    const QFontMetrics metrics(font);
    const QRect textRect = metrics.boundingRect(text);
    const int margin = m_sizeSpin->value() / 4;
    const QSize imageSize(textRect.width() + 2 * margin, metrics.height() + 2 * margin);

    QImage image(imageSize, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::TextAntialiasing);
    painter.setFont(font);
    painter.setPen(m_color);
    const QRect drawRect(QPoint(margin - textRect.left(), 0), QSize(textRect.width(), imageSize.height()));
    painter.drawText(drawRect, Qt::AlignLeft | Qt::AlignVCenter, text);
    painter.end();
    return image;
}

void TypedSignatureWidget::updatePreview() {
    const QImage image = renderImage();
    if (image.isNull()) {
        m_previewLabel->clear();
        return;
    }
    m_previewLabel->setPixmap(QPixmap::fromImage(image).scaled(
        m_previewLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

} // namespace papyrus
