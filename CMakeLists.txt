cmake_minimum_required(VERSION 3.21)
project(Papyrus VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

find_package(Qt6 REQUIRED COMPONENTS Widgets Pdf PdfWidgets PrintSupport Concurrent)

set(PDFium_DIR "${CMAKE_SOURCE_DIR}/third_party/pdfium" CACHE PATH "Path to vendored PDFium")
if(NOT EXISTS "${PDFium_DIR}/lib/libpdfium.so")
    message(FATAL_ERROR
        "PDFium not found at ${PDFium_DIR}. Run scripts/fetch-pdfium.sh first.")
endif()
find_package(PDFium REQUIRED CONFIG PATHS "${PDFium_DIR}" NO_DEFAULT_PATH)

add_subdirectory(libs/pdf-engine)
add_subdirectory(libs/conversion-engine)
add_subdirectory(libs/ocr-engine)
add_subdirectory(app)
