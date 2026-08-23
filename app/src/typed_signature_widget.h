#pragma once

#include <QColor>
#include <QWidget>

QT_BEGIN_NAMESPACE
class QLineEdit;
class QListWidget;
class QSpinBox;
class QDoubleSpinBox;
class QCheckBox;
class QToolButton;
class QLabel;
QT_END_NAMESPACE

namespace papyrus {

// "Type your signature" tab: pick one of several handwriting fonts, tune
// size/color/spacing/slant, see a live preview. Rotation is deliberately not
// offered here — it's handled once, generically, at placement time (see
// SignaturePlacementDialog) rather than baked into the saved image.
class TypedSignatureWidget : public QWidget {
    Q_OBJECT
public:
    explicit TypedSignatureWidget(QWidget* parent = nullptr);

    bool isEmpty() const;
    QImage renderImage() const;
    QString suggestedName() const;

signals:
    void contentChanged();

private:
    void populateFontList();
    void updatePreview();

    QLineEdit* m_textEdit;
    QListWidget* m_fontList;
    QSpinBox* m_sizeSpin;
    QDoubleSpinBox* m_spacingSpin;
    QCheckBox* m_italicCheck;
    QToolButton* m_colorButton;
    QColor m_color{0, 0, 0};
    QLabel* m_previewLabel;
};

} // namespace papyrus
