qt_add_executable(papyrus
    src/main.cpp
    src/main_window.h
    src/main_window.cpp
    src/document_tab.h
    src/document_tab.cpp
    src/thumbnail_panel.h
    src/thumbnail_panel.cpp
    src/search_bar.h
    src/search_bar.cpp
    src/page_manager_dialog.h
    src/page_manager_dialog.cpp
    src/annotation_dialog.h
    src/annotation_dialog.cpp
    src/text_to_pdf_dialog.h
    src/text_to_pdf_dialog.cpp
    src/images_to_pdf_dialog.h
    src/images_to_pdf_dialog.cpp
    src/office_conversion_dialog.h
    src/office_conversion_dialog.cpp
    src/signature_fonts.h
    src/signature_fonts.cpp
    src/signature_store.h
    src/signature_store.cpp
    src/signature_pad_widget.h
    src/signature_pad_widget.cpp
    src/typed_signature_widget.h
    src/typed_signature_widget.cpp
    src/signature_placement_dialog.h
    src/signature_placement_dialog.cpp
    src/signature_dialog.h
    src/signature_dialog.cpp
    src/ocr_dialog.h
    src/ocr_dialog.cpp
    src/form_fill_dialog.h
    src/form_fill_dialog.cpp
    resources/fonts.qrc
)

target_link_libraries(papyrus PRIVATE
    Qt6::Widgets
    Qt6::Pdf
    Qt6::PdfWidgets
    Qt6::PrintSupport
    pdf-engine
    conversion-engine
    ocr-engine
)

target_include_directories(papyrus PRIVATE src)

# Dev-tree convenience: run straight from the build dir without LD_LIBRARY_PATH.
# Packaging will instead ship libpdfium.so next to the executable with a
# $ORIGIN rpath (see packaging/ in a later phase).
set_target_properties(papyrus PROPERTIES BUILD_RPATH "${PDFium_DIR}/lib")
