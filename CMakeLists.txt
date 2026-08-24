cmake_minimum_required(VERSION 3.21)
project(Papyrus VERSION 0.1.0 LANGUAGES CXX)

include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

find_package(Qt6 REQUIRED COMPONENTS Widgets Pdf PdfWidgets PrintSupport Concurrent Network)

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

# --- Packaging (Phase 9): `cpack -G DEB` or `cpack -G RPM` from the build
# directory (or `cmake --build . --target package` for the platform default).
# AppImage/Flatpak are separate, not CPack-driven.
set(CPACK_PACKAGE_NAME "papyrus")
set(CPACK_PACKAGE_VENDOR "Papyrus")
set(CPACK_PACKAGE_CONTACT "Adelphe Borges <borges.adelphe@gmail.com>")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Suite bureautique PDF (lecture, édition, annotation, signature, OCR)")
set(CPACK_RPM_PACKAGE_DESCRIPTION [[Papyrus est une application de bureau pour lire, éditer, annoter, signer,
convertir et faire de l'OCR sur des documents PDF, sans dépendre d'un
service en ligne. Basée sur Qt6 et PDFium.]])
set(CPACK_PACKAGING_INSTALL_PREFIX "/usr")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/Ade-gns/Papyrus")

set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Adelphe Borges <borges.adelphe@gmail.com>")
set(CPACK_DEBIAN_PACKAGE_SECTION "office")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
# soffice/tesseract are invoked as subprocesses only for the Office-conversion
# and OCR features; the rest of the app works without them, hence Recommends
# rather than Depends.
set(CPACK_DEBIAN_PACKAGE_RECOMMENDS "libreoffice-writer, libreoffice-impress, tesseract-ocr")
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)

set(CPACK_PACKAGE_FILE_NAME "papyrus_${PROJECT_VERSION}_amd64")

set(CPACK_RPM_PACKAGE_LICENSE "MIT")
set(CPACK_RPM_PACKAGE_GROUP "Applications/Office")
set(CPACK_RPM_PACKAGE_RELEASE "1")
set(CPACK_RPM_FILE_NAME "papyrus-${PROJECT_VERSION}-1.x86_64.rpm")
# Built on Ubuntu, targeting Fedora/openSUSE: rpmbuild's automatic
# dependency scan (file/soname -> RPM provides) needs a local RPM package
# database to resolve against, which doesn't exist on a dpkg-based system —
# autoreq would silently produce an empty or wrong Requires: list here, not
# a merely-incomplete one. Disabled, with a manually written list instead
# (Fedora package names, unverified — build and check on real Fedora before
# treating this dependency list as trustworthy).
set(CPACK_RPM_PACKAGE_AUTOREQ "no")
set(CPACK_RPM_PACKAGE_REQUIRES "qt6-qtbase >= 6.4, qt6-qtpdf >= 6.4")
set(CPACK_RPM_PACKAGE_SUGGESTS "libreoffice-writer, libreoffice-impress, tesseract")

include(CPack)
