#!/usr/bin/env bash
# Downloads the prebuilt PDFium shared library (BSD-3-Clause) used as the
# write-capable PDF engine. Vendored binaries aren't committed to git; run
# this once after cloning (CMake also depends on third_party/pdfium existing).
set -euo pipefail

PDFIUM_TAG="chromium/8009"
PDFIUM_ASSET="pdfium-linux-x64.tgz"
PDFIUM_SHA256="be513e8021a5bf8eb2116e00d78c3bacb82c5a02b3785156ae14fe5e33084385"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DEST_DIR="${SCRIPT_DIR}/../third_party/pdfium"
URL="https://github.com/bblanchon/pdfium-binaries/releases/download/${PDFIUM_TAG}/${PDFIUM_ASSET}"

if [ -f "${DEST_DIR}/lib/libpdfium.so" ]; then
    echo "PDFium already present in ${DEST_DIR}, skipping."
    exit 0
fi

mkdir -p "${DEST_DIR}"
TMP_ARCHIVE="$(mktemp)"
trap 'rm -f "${TMP_ARCHIVE}"' EXIT

echo "Downloading PDFium ${PDFIUM_TAG} for linux-x64..."
curl -fL --progress-bar -o "${TMP_ARCHIVE}" "${URL}"

echo "Verifying checksum..."
echo "${PDFIUM_SHA256}  ${TMP_ARCHIVE}" | sha256sum -c -

tar xzf "${TMP_ARCHIVE}" -C "${DEST_DIR}"
echo "PDFium installed in ${DEST_DIR}"
