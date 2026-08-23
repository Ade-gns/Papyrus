add_library(conversion-engine STATIC
    include/papyrus/conversion/result.h
    include/papyrus/conversion/text_to_pdf.h
    include/papyrus/conversion/images_to_pdf.h
    include/papyrus/conversion/office_to_pdf.h
    src/text_to_pdf.cpp
    src/images_to_pdf.cpp
    src/office_to_pdf.cpp
)

target_include_directories(conversion-engine PUBLIC include)

target_link_libraries(conversion-engine PUBLIC Qt6::Gui Qt6::PrintSupport)

target_compile_features(conversion-engine PUBLIC cxx_std_20)
