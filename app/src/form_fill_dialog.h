#pragma once

#include "papyrus/pdf/form_engine.h"

#include <QDialog>

QT_BEGIN_NAMESPACE
class QFormLayout;
class QLabel;
QT_END_NAMESPACE

namespace papyrus {

// Lists every fillable AcroForm field (text/checkbox/radio/choice) with an
// appropriate input widget, rather than click-on-page editing — QPdfView
// has no hooks for forwarding mouse/keyboard into PDFium's form system, so
// a field list is the pragmatic way to fill a form in this app. See
// papyrus::pdf::FormEngine for how values are actually written.
class FormFillDialog : public QDialog {
    Q_OBJECT
public:
    explicit FormFillDialog(const QString& filePath, QWidget* parent = nullptr);

signals:
    void documentSaved(const QString& filePath);

private:
    void buildFieldWidgets();
    void save();

    QString m_filePath;
    pdf::FormEngine m_engine;
    std::vector<pdf::FormField> m_fields;
    std::vector<QWidget*> m_fieldWidgets; // parallel to m_fields; nullptr where not fillable
    QFormLayout* m_form = nullptr;
};

} // namespace papyrus
