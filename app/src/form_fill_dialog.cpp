#include "form_fill_dialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFile>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

namespace papyrus {

namespace {
bool isFillable(pdf::FormFieldType type) {
    return type == pdf::FormFieldType::TextField || type == pdf::FormFieldType::CheckBox ||
           type == pdf::FormFieldType::RadioButton || type == pdf::FormFieldType::ComboBox ||
           type == pdf::FormFieldType::ListBox;
}
} // namespace

FormFillDialog::FormFillDialog(const QString& filePath, QWidget* parent)
    : QDialog(parent), m_filePath(filePath) {
    setWindowTitle(tr("Remplir le formulaire — %1").arg(QFileInfo(filePath).fileName()));
    resize(560, 640);

    auto* layout = new QVBoxLayout(this);

    if (m_engine.load(filePath) != pdf::EditResult::Ok || !m_engine.hasForm()) {
        layout->addWidget(new QLabel(tr("Ce document ne contient pas de formulaire à remplir."), this));
        auto* closeButton = new QPushButton(tr("Fermer"), this);
        connect(closeButton, &QPushButton::clicked, this, &QDialog::accept);
        layout->addWidget(closeButton);
        return;
    }

    auto* scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    auto* formWidget = new QWidget(scrollArea);
    m_form = new QFormLayout(formWidget);
    scrollArea->setWidget(formWidget);
    layout->addWidget(scrollArea, 1);

    auto* saveButton = new QPushButton(tr("Enregistrer"), this);
    saveButton->setDefault(true);
    connect(saveButton, &QPushButton::clicked, this, &FormFillDialog::save);
    auto* closeButton = new QPushButton(tr("Fermer"), this);
    connect(closeButton, &QPushButton::clicked, this, &QDialog::reject);
    auto* bottomRow = new QHBoxLayout();
    bottomRow->addStretch(1);
    bottomRow->addWidget(closeButton);
    bottomRow->addWidget(saveButton);
    layout->addLayout(bottomRow);

    buildFieldWidgets();
}

void FormFillDialog::buildFieldWidgets() {
    m_fields = m_engine.fields();
    m_fieldWidgets.assign(m_fields.size(), nullptr);

    for (std::size_t i = 0; i < m_fields.size(); ++i) {
        const pdf::FormField& field = m_fields[i];
        if (!isFillable(field.type)) {
            continue;
        }
        const QString label = field.name.isEmpty() ? tr("(champ sans nom)") : field.name;

        switch (field.type) {
        case pdf::FormFieldType::TextField: {
            auto* edit = new QLineEdit(field.value, this);
            m_form->addRow(label, edit);
            m_fieldWidgets[i] = edit;
            break;
        }
        case pdf::FormFieldType::CheckBox:
        case pdf::FormFieldType::RadioButton: {
            auto* check = new QCheckBox(this);
            check->setChecked(field.isChecked);
            m_form->addRow(label, check);
            m_fieldWidgets[i] = check;
            break;
        }
        case pdf::FormFieldType::ComboBox:
        case pdf::FormFieldType::ListBox: {
            auto* combo = new QComboBox(this);
            combo->addItems(field.options);
            const int index = field.options.indexOf(field.value);
            combo->setCurrentIndex(index >= 0 ? index : 0);
            m_form->addRow(label, combo);
            m_fieldWidgets[i] = combo;
            break;
        }
        default:
            break;
        }
    }

    if (m_form->rowCount() == 0) {
        m_form->addRow(new QLabel(tr("Aucun champ modifiable trouvé."), this));
    }
}

void FormFillDialog::save() {
    for (std::size_t i = 0; i < m_fields.size(); ++i) {
        QWidget* widget = m_fieldWidgets[i];
        if (!widget) {
            continue;
        }
        const pdf::FormField& field = m_fields[i];
        switch (field.type) {
        case pdf::FormFieldType::TextField:
            m_engine.setTextValue(field.pageIndex, field.annotIndex,
                                   qobject_cast<QLineEdit*>(widget)->text());
            break;
        case pdf::FormFieldType::CheckBox:
        case pdf::FormFieldType::RadioButton:
            m_engine.setChecked(field.pageIndex, field.annotIndex,
                                 qobject_cast<QCheckBox*>(widget)->isChecked());
            break;
        case pdf::FormFieldType::ComboBox:
        case pdf::FormFieldType::ListBox:
            m_engine.setSelectedOption(field.pageIndex, field.annotIndex,
                                        qobject_cast<QComboBox*>(widget)->currentIndex());
            break;
        default:
            break;
        }
    }

    const QString tempPath = m_filePath + QStringLiteral(".papyrus-tmp");
    if (m_engine.save(tempPath) != pdf::EditResult::Ok) {
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
