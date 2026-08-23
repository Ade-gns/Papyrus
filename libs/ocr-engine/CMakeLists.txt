add_library(ocr-engine STATIC
    include/papyrus/ocr/ocr_job.h
    src/ocr_job.cpp
)

target_include_directories(ocr-engine PUBLIC include)

target_link_libraries(ocr-engine PUBLIC Qt6::Concurrent Qt6::Pdf pdf-engine)

target_compile_features(ocr-engine PUBLIC cxx_std_20)
