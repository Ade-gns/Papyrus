#!/usr/bin/env bash
# Builds a self-contained Papyrus-x86_64.AppImage from an already-configured
# and built build/ tree (run cmake --build first). Bundles Qt6 and PDFium so
# the result runs on distros without those installed system-wide.
#
# Downloads linuxdeploy, linuxdeploy-plugin-qt and appimagetool (pinned
# versions, checksum-verified) into third_party/appimage-tools/ on first run.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SRC_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
BUILD_DIR="${SRC_DIR}/build"
TOOLS_DIR="${SRC_DIR}/third_party/appimage-tools"
APPDIR="${BUILD_DIR}/AppDir"

LINUXDEPLOY_TAG="1-alpha-20251107-1"
LINUXDEPLOY_SHA256="c20cd71e3a4e3b80c3483cef793cda3f4e990aca14014d23c544ca3ce1270b4d"
LINUXDEPLOY_QT_TAG="1-alpha-20250213-1"
LINUXDEPLOY_QT_SHA256="15106be885c1c48a021198e7e1e9a48ce9d02a86dd0a1848f00bdbf3c1c92724"
APPIMAGETOOL_TAG="1.9.1"
APPIMAGETOOL_SHA256="ed4ce84f0d9caff66f50bcca6ff6f35aae54ce8135408b3fa33abfc3cb384eb0"

log() { printf '\033[1;34m==>\033[0m %s\n' "$*"; }
die() { printf '\033[1;31merreur:\033[0m %s\n' "$*" >&2; exit 1; }

fetch_tool() {
    local name="$1" tag="$2" sha="$3" url="$4"
    local dest="${TOOLS_DIR}/${name}"
    if [ -f "${dest}" ] && echo "${sha}  ${dest}" | sha256sum -c - >/dev/null 2>&1; then
        return 0
    fi
    log "Téléchargement de ${name} (${tag})..."
    mkdir -p "${TOOLS_DIR}"
    curl -fL --progress-bar -o "${dest}" "${url}"
    echo "${sha}  ${dest}" | sha256sum -c -
    chmod +x "${dest}"
}

command -v qmake6 >/dev/null 2>&1 || die "qmake6 introuvable (paquet qt6-base-dev-tools requis)."
[ -x "${BUILD_DIR}/app/papyrus" ] || die \
    "Papyrus n'est pas compilé. Lancez d'abord : cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j\$(nproc)"

fetch_tool "linuxdeploy-x86_64.AppImage" "${LINUXDEPLOY_TAG}" "${LINUXDEPLOY_SHA256}" \
    "https://github.com/linuxdeploy/linuxdeploy/releases/download/${LINUXDEPLOY_TAG}/linuxdeploy-x86_64.AppImage"
fetch_tool "linuxdeploy-plugin-qt-x86_64.AppImage" "${LINUXDEPLOY_QT_TAG}" "${LINUXDEPLOY_QT_SHA256}" \
    "https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/${LINUXDEPLOY_QT_TAG}/linuxdeploy-plugin-qt-x86_64.AppImage"
fetch_tool "appimagetool-x86_64.AppImage" "${APPIMAGETOOL_TAG}" "${APPIMAGETOOL_SHA256}" \
    "https://github.com/AppImage/appimagetool/releases/download/${APPIMAGETOOL_TAG}/appimagetool-x86_64.AppImage"

log "Préparation de l'AppDir..."
rm -rf "${APPDIR}"
cmake --install "${BUILD_DIR}" --prefix "${APPDIR}/usr" >/dev/null

log "Empaquetage des bibliothèques (linuxdeploy + plugin Qt)..."
export QMAKE="$(command -v qmake6)"
export VERSION="$(cd "${SRC_DIR}" && git describe --tags --always 2>/dev/null || echo dev)"
"${TOOLS_DIR}/linuxdeploy-x86_64.AppImage" \
    --appdir "${APPDIR}" \
    --executable "${APPDIR}/usr/bin/papyrus" \
    --desktop-file "${APPDIR}/usr/share/applications/papyrus.desktop" \
    --icon-file "${APPDIR}/usr/share/icons/hicolor/256x256/apps/papyrus.png" \
    --plugin qt

# linuxdeploy's Qt plugin re-scans papyrus's dependencies and copies
# libpdfium.so straight into usr/lib (matching the RPATH it rewrites every
# ELF file to: $ORIGIN/../lib). The private usr/lib/papyrus/ copy from our
# own install() rule — meant for the .deb, where it avoids colliding with
# system libs — is then dead weight here.
rm -rf "${APPDIR}/usr/lib/papyrus"

log "Génération de l'AppImage..."
(cd "${BUILD_DIR}" && "${TOOLS_DIR}/appimagetool-x86_64.AppImage" "${APPDIR}" "Papyrus-x86_64.AppImage")

log "Terminé : ${BUILD_DIR}/Papyrus-x86_64.AppImage"
