#include "papyrus/pdf/form_engine.h"

#include "pdfium_runtime.h"

#include <fpdf_annot.h>
#include <fpdf_formfill.h>
#include <fpdf_save.h>
#include <fpdfview.h>

#include <QFile>
#include <QFileInfo>

#include <cstring>

namespace papyrus::pdf {

using detail::acquirePdfiumLibrary;
using detail::releasePdfiumLibrary;

namespace {

constexpr int kFormTypeNone = 0;

// FPDF_FORMFILLINFO requires plain C function pointers; these stubs are
// enough for headless value-setting (no on-screen rendering of the form
// widgets is needed, only the value/appearance-commit side effects).
void ffiInvalidate(FPDF_FORMFILLINFO*, FPDF_PAGE, double, double, double, double) {}
void ffiSetCursor(FPDF_FORMFILLINFO*, int) {}
int ffiSetTimer(FPDF_FORMFILLINFO*, int, TimerCallback) { return 0; }
void ffiKillTimer(FPDF_FORMFILLINFO*, int) {}
FPDF_SYSTEMTIME ffiGetLocalTime(FPDF_FORMFILLINFO*) { return FPDF_SYSTEMTIME{}; }
int ffiGetRotation(FPDF_FORMFILLINFO*, FPDF_PAGE) { return 0; }
void ffiExecuteNamedAction(FPDF_FORMFILLINFO*, FPDF_BYTESTRING) {}
void ffiSetTextFieldFocus(FPDF_FORMFILLINFO*, FPDF_WIDESTRING, FPDF_DWORD, FPDF_BOOL) {}
void ffiDoURIAction(FPDF_FORMFILLINFO*, FPDF_BYTESTRING) {}
void ffiDoGoToAction(FPDF_FORMFILLINFO*, int, int, float*, int) {}

FormFieldType mapFieldType(int type) {
    switch (type) {
    case FPDF_FORMFIELD_PUSHBUTTON:
        return FormFieldType::PushButton;
    case FPDF_FORMFIELD_CHECKBOX:
        return FormFieldType::CheckBox;
    case FPDF_FORMFIELD_RADIOBUTTON:
        return FormFieldType::RadioButton;
    case FPDF_FORMFIELD_COMBOBOX:
        return FormFieldType::ComboBox;
    case FPDF_FORMFIELD_LISTBOX:
        return FormFieldType::ListBox;
    case FPDF_FORMFIELD_TEXTFIELD:
        return FormFieldType::TextField;
    case FPDF_FORMFIELD_SIGNATURE:
        return FormFieldType::Signature;
    default:
        return FormFieldType::Unknown;
    }
}

QString readFieldValue(FPDF_FORMHANDLE form, FPDF_ANNOTATION annot) {
    unsigned short buffer[1024];
    const unsigned long len = FPDFAnnot_GetFormFieldValue(form, annot, buffer, sizeof(buffer));
    if (len < 2) {
        return {};
    }
    return QString::fromUtf16(reinterpret_cast<const char16_t*>(buffer), static_cast<int>(len / 2 - 1));
}

QString readFieldName(FPDF_FORMHANDLE form, FPDF_ANNOTATION annot) {
    unsigned short buffer[512];
    const unsigned long len = FPDFAnnot_GetFormFieldName(form, annot, buffer, sizeof(buffer));
    if (len < 2) {
        return {};
    }
    return QString::fromUtf16(reinterpret_cast<const char16_t*>(buffer), static_cast<int>(len / 2 - 1));
}

QStringList readOptions(FPDF_FORMHANDLE form, FPDF_ANNOTATION annot) {
    QStringList options;
    const int count = FPDFAnnot_GetOptionCount(form, annot);
    for (int i = 0; i < count; ++i) {
        unsigned short buffer[512];
        const unsigned long len = FPDFAnnot_GetOptionLabel(form, annot, i, buffer, sizeof(buffer));
        options.append(len < 2 ? QString()
                                : QString::fromUtf16(reinterpret_cast<const char16_t*>(buffer),
                                                      static_cast<int>(len / 2 - 1)));
    }
    return options;
}

struct FileWriter : FPDF_FILEWRITE {
    explicit FileWriter(const QString& path) : file(path) {
        version = 1;
        WriteBlock = &FileWriter::write;
    }
    bool open() { return file.open(QIODevice::WriteOnly | QIODevice::Truncate); }
    static int write(FPDF_FILEWRITE* self, const void* data, unsigned long size) {
        auto* writer = static_cast<FileWriter*>(self);
        return writer->file.write(static_cast<const char*>(data), static_cast<qint64>(size)) ==
                       static_cast<qint64>(size)
                   ? 1
                   : 0;
    }
    QFile file;
};

} // namespace

struct FormEngine::Impl {
    Impl() { acquirePdfiumLibrary(); }
    ~Impl() {
        if (form) {
            FPDFDOC_ExitFormFillEnvironment(form);
        }
        if (document) {
            FPDF_CloseDocument(document);
        }
        releasePdfiumLibrary();
    }

    FPDF_DOCUMENT document = nullptr;
    FPDF_FORMFILLINFO formInfo{};
    FPDF_FORMHANDLE form = nullptr;

    // Runs `action` with the page loaded and the form environment attached,
    // bracketed exactly as PDFium expects (OnAfterLoadPage/OnBeforeClosePage).
    template <typename Action>
    auto withPage(int pageIndex, Action action) {
        FPDF_PAGE page = FPDF_LoadPage(document, pageIndex);
        if (!page) {
            return decltype(action(page)){};
        }
        FORM_OnAfterLoadPage(page, form);
        auto result = action(page);
        FORM_OnBeforeClosePage(page, form);
        FPDF_ClosePage(page);
        return result;
    }
};

FormEngine::FormEngine() : m_impl(std::make_unique<Impl>()) {}
FormEngine::~FormEngine() = default;

EditResult FormEngine::load(const QString& filePath) {
    if (!QFileInfo::exists(filePath)) {
        return EditResult::FileNotFound;
    }
    FPDF_DOCUMENT doc = FPDF_LoadDocument(filePath.toUtf8().constData(), nullptr);
    if (!doc) {
        switch (FPDF_GetLastError()) {
        case FPDF_ERR_FORMAT:
            return EditResult::InvalidFormat;
        case FPDF_ERR_PASSWORD:
            return EditResult::PasswordProtected;
        default:
            return EditResult::Unknown;
        }
    }

    if (m_impl->form) {
        FPDFDOC_ExitFormFillEnvironment(m_impl->form);
        m_impl->form = nullptr;
    }
    if (m_impl->document) {
        FPDF_CloseDocument(m_impl->document);
    }
    m_impl->document = doc;

    std::memset(&m_impl->formInfo, 0, sizeof(m_impl->formInfo));
    m_impl->formInfo.version = 1;
    m_impl->formInfo.FFI_Invalidate = ffiInvalidate;
    m_impl->formInfo.FFI_SetCursor = ffiSetCursor;
    m_impl->formInfo.FFI_SetTimer = ffiSetTimer;
    m_impl->formInfo.FFI_KillTimer = ffiKillTimer;
    m_impl->formInfo.FFI_GetLocalTime = ffiGetLocalTime;
    m_impl->formInfo.FFI_GetRotation = ffiGetRotation;
    m_impl->formInfo.FFI_ExecuteNamedAction = ffiExecuteNamedAction;
    m_impl->formInfo.FFI_SetTextFieldFocus = ffiSetTextFieldFocus;
    m_impl->formInfo.FFI_DoURIAction = ffiDoURIAction;
    m_impl->formInfo.FFI_DoGoToAction = ffiDoGoToAction;
    m_impl->form = FPDFDOC_InitFormFillEnvironment(doc, &m_impl->formInfo);

    return m_impl->form ? EditResult::Ok : EditResult::Unknown;
}

bool FormEngine::hasForm() const {
    return m_impl->form != nullptr && FPDF_GetFormType(m_impl->document) != kFormTypeNone;
}

std::vector<FormField> FormEngine::fields() const {
    std::vector<FormField> result;
    if (!hasForm()) {
        return result;
    }

    const int pageCount = FPDF_GetPageCount(m_impl->document);
    for (int pageIndex = 0; pageIndex < pageCount; ++pageIndex) {
        m_impl->withPage(pageIndex, [&](FPDF_PAGE page) {
            const int annotCount = FPDFPage_GetAnnotCount(page);
            for (int annotIndex = 0; annotIndex < annotCount; ++annotIndex) {
                FPDF_ANNOTATION annot = FPDFPage_GetAnnot(page, annotIndex);
                if (FPDFAnnot_GetSubtype(annot) == FPDF_ANNOT_WIDGET) {
                    const int rawType = FPDFAnnot_GetFormFieldType(m_impl->form, annot);
                    if (rawType >= 0) {
                        FormField field;
                        field.pageIndex = pageIndex;
                        field.annotIndex = annotIndex;
                        field.type = mapFieldType(rawType);
                        field.name = readFieldName(m_impl->form, annot);
                        field.value = readFieldValue(m_impl->form, annot);
                        if (field.type == FormFieldType::CheckBox || field.type == FormFieldType::RadioButton) {
                            field.isChecked = FPDFAnnot_IsChecked(m_impl->form, annot);
                        }
                        if (field.type == FormFieldType::ComboBox || field.type == FormFieldType::ListBox) {
                            field.options = readOptions(m_impl->form, annot);
                        }
                        result.push_back(std::move(field));
                    }
                }
                FPDFPage_CloseAnnot(annot);
            }
            return true;
        });
    }
    return result;
}

bool FormEngine::setTextValue(int pageIndex, int annotIndex, const QString& value) {
    return m_impl->withPage(pageIndex, [&](FPDF_PAGE page) {
        FPDF_ANNOTATION annot = FPDFPage_GetAnnot(page, annotIndex);
        if (!annot) {
            return false;
        }
        FORM_SetFocusedAnnot(m_impl->form, annot);
        FORM_SelectAllText(m_impl->form, page);
        const std::u16string wide = value.toStdU16String();
        FORM_ReplaceSelection(m_impl->form, page, reinterpret_cast<FPDF_WIDESTRING>(wide.c_str()));
        FORM_ForceToKillFocus(m_impl->form);
        FPDFPage_CloseAnnot(annot);
        return true;
    });
}

bool FormEngine::setChecked(int pageIndex, int annotIndex, bool checked) {
    return m_impl->withPage(pageIndex, [&](FPDF_PAGE page) {
        FPDF_ANNOTATION annot = FPDFPage_GetAnnot(page, annotIndex);
        if (!annot) {
            return false;
        }
        const bool currentlyChecked = FPDFAnnot_IsChecked(m_impl->form, annot);
        FS_RECTF rect{};
        FPDFAnnot_GetRect(annot, &rect);
        FPDFPage_CloseAnnot(annot);

        if (currentlyChecked != checked) {
            const double centerX = (rect.left + rect.right) / 2.0;
            const double centerY = (rect.top + rect.bottom) / 2.0;
            FORM_OnLButtonDown(m_impl->form, page, 0, centerX, centerY);
            FORM_OnLButtonUp(m_impl->form, page, 0, centerX, centerY);
        }
        FORM_ForceToKillFocus(m_impl->form);
        return true;
    });
}

bool FormEngine::setSelectedOption(int pageIndex, int annotIndex, int optionIndex) {
    return m_impl->withPage(pageIndex, [&](FPDF_PAGE page) {
        FPDF_ANNOTATION annot = FPDFPage_GetAnnot(page, annotIndex);
        if (!annot) {
            return false;
        }
        FORM_SetFocusedAnnot(m_impl->form, annot);
        const bool ok = FORM_SetIndexSelected(m_impl->form, page, optionIndex, true);
        FORM_ForceToKillFocus(m_impl->form);
        FPDFPage_CloseAnnot(annot);
        return static_cast<bool>(ok);
    });
}

EditResult FormEngine::save(const QString& outputFilePath) const {
    FileWriter writer(outputFilePath);
    if (!writer.open()) {
        return EditResult::SaveFailed;
    }
    const bool ok = FPDF_SaveAsCopy(m_impl->document, &writer, 0);
    writer.file.close();
    return ok ? EditResult::Ok : EditResult::SaveFailed;
}

} // namespace papyrus::pdf
