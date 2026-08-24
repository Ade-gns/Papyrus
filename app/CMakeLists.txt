qt_add_executable(papyrus
    src/main.cpp
    src/main_window.h
    src/main_window.cpp
    src/document_tab.h
    src/document_tab.cpp
    src/document_printer.h
    src/document_printer.cpp
    src/thumbnail_panel.h
    src/thumbnail_panel.cpp
    src/search_bar.h
    src/search_bar.cpp
    src/page_manager_dialog.h
    src/page_manager_dialog.cpp
    src/annotation_dialog.h
    src/annotation_dialog.cpp
    src/text_edit_dialog.h
    src/text_edit_dialog.cpp
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
    src/update_checker.h
    src/update_checker.cpp
    resources/fonts.qrc
    resources/i18n.qrc
)

target_link_libraries(papyrus PRIVATE
    Qt6::Widgets
    Qt6::Pdf
    Qt6::PdfWidgets
    Qt6::PrintSupport
    Qt6::Network
    pdf-engine
    conversion-engine
    ocr-engine
)

target_include_directories(papyrus PRIVATE src)

# Dev-tree convenience: run straight from the build dir without LD_LIBRARY_PATH.
# The installed/packaged binary instead ships libpdfium.so next to it (see
# install() rules below) and finds it via INSTALL_RPATH.
set_target_properties(papyrus PROPERTIES BUILD_RPATH "${PDFium_DIR}/lib")

# --- Install rules (Phase 9 packaging) ---
# libpdfium.so isn't a system library (it's vendored, fetched by
# scripts/fetch-pdfium.sh), so it ships in a private directory next to the
# binary rather than a shared libdir; INSTALL_RPATH points there with
# $ORIGIN so it works regardless of install prefix.
set_target_properties(papyrus PROPERTIES
    INSTALL_RPATH "$ORIGIN/../${CMAKE_INSTALL_LIBDIR}/papyrus"
    BUILD_WITH_INSTALL_RPATH FALSE
)

install(TARGETS papyrus RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES "${PDFium_DIR}/lib/libpdfium.so" DESTINATION "${CMAKE_INSTALL_LIBDIR}/papyrus")

install(FILES "${CMAKE_SOURCE_DIR}/packaging/papyrus.desktop"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/applications")
install(FILES "${CMAKE_SOURCE_DIR}/packaging/icons/papyrus-48.png"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/48x48/apps" RENAME papyrus.png)
install(FILES "${CMAKE_SOURCE_DIR}/packaging/icons/papyrus-128.png"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/128x128/apps" RENAME papyrus.png)
install(FILES "${CMAKE_SOURCE_DIR}/packaging/icons/papyrus-256.png"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/256x256/apps" RENAME papyrus.png)
