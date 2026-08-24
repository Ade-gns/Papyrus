add_library(pdf-engine STATIC
    include/papyrus/pdf/document.h
    include/papyrus/pdf/thumbnail_cache.h
    include/papyrus/pdf/page_editor.h
    include/papyrus/pdf/annotation_writer.h
    include/papyrus/pdf/form_engine.h
    include/papyrus/pdf/document_history.h
    src/document.cpp
    src/thumbnail_cache.cpp
    src/page_editor.cpp
    src/annotation_writer.cpp
    src/form_engine.cpp
    src/document_history.cpp
    src/pdfium_runtime.h
    src/pdfium_runtime.cpp
)

target_include_directories(pdf-engine PUBLIC include)

target_link_libraries(pdf-engine
    PUBLIC Qt6::Pdf
    PRIVATE pdfium
)

target_compile_features(pdf-engine PUBLIC cxx_std_20)
