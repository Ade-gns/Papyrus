#pragma once

#include "papyrus/pdf/page_editor.h" // reuses EditResult

#include <QString>
#include <QStringList>

#include <memory>
#include <vector>

namespace papyrus::pdf {

enum class FormFieldType {
    Unknown,
    PushButton,
    CheckBox,
    RadioButton,
    ComboBox,
    ListBox,
    TextField,
    Signature,
};

struct FormField {
    int pageIndex = 0;
    int annotIndex = 0; // position within FPDFPage_GetAnnot for that page — identifies the field
    FormFieldType type = FormFieldType::Unknown;
    QString name;
    QString value;          // current text value, or selected option's label for choice fields
    bool isChecked = false; // CheckBox/RadioButton
    QStringList options;    // ComboBox/ListBox
};

// Fills existing AcroForm fields (text/checkbox/radio/choice). PDFium has no
// direct "set field value" call — every setter here simulates the same
// focus → edit → commit sequence a real click-and-type interaction would
// produce (FORM_SetFocusedAnnot, FORM_SelectAllText + FORM_ReplaceSelection
// for text, FORM_OnLButtonDown/Up for checkboxes, FORM_SetIndexSelected for
// choice fields), which is the only way PDFium's public API regenerates the
// field's appearance and persists the value through FPDF_SaveAsCopy. This
// was verified against hand-built test PDFs for all three cases before
// relying on it here — see project notes.
//
// Creating new fields, and XFA forms, are out of scope for this pass.
class FormEngine {
public:
    FormEngine();
    ~FormEngine();

    FormEngine(const FormEngine&) = delete;
    FormEngine& operator=(const FormEngine&) = delete;

    EditResult load(const QString& filePath);
    bool hasForm() const;
    std::vector<FormField> fields() const;

    bool setTextValue(int pageIndex, int annotIndex, const QString& value);
    bool setChecked(int pageIndex, int annotIndex, bool checked);
    bool setSelectedOption(int pageIndex, int annotIndex, int optionIndex);

    EditResult save(const QString& outputFilePath) const;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace papyrus::pdf
