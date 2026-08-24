#!/usr/bin/env bash
# One-command installer for Debian/Ubuntu (and derivatives): installs build
# dependencies, compiles Papyrus, builds a .deb, and installs it via apt so
# runtime dependencies are resolved automatically.
#
#   curl -fsSL https://raw.githubusercontent.com/Ade-gns/Papyrus/main/scripts/install.sh | bash
#
# Or, from an existing checkout: ./scripts/install.sh
#
# No prebuilt package is published yet, so this always builds from source —
# expect it to take a few minutes and to ask for your sudo password twice
# (build dependencies, then the built package itself).
set -euo pipefail

REPO_URL="https://github.com/Ade-gns/Papyrus.git"
BUILD_TYPE="Release"

log() { printf '\033[1;34m==>\033[0m %s\n' "$*"; }
die() { printf '\033[1;31merreur:\033[0m %s\n' "$*" >&2; exit 1; }

if ! command -v apt-get >/dev/null 2>&1; then
    die "Ce script suppose une distribution basée sur apt (Debian/Ubuntu). Voir README.md pour compiler manuellement sur une autre distribution."
fi

# Reuse the checkout this script lives in when run locally (./scripts/install.sh);
# otherwise (piped in via curl, so there is no local checkout) clone one into a
# cache directory. Only the cloned-cache path is ever reset/updated — a local
# checkout you're already working in is never touched beyond being read.
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" 2>/dev/null && pwd || true)"
if [ -n "${SCRIPT_DIR}" ] && [ -f "${SCRIPT_DIR}/../CMakeLists.txt" ]; then
    SRC_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
    log "Utilisation du dépôt existant : ${SRC_DIR}"
else
    SRC_DIR="${HOME}/.cache/papyrus-src"
    if [ -d "${SRC_DIR}/.git" ]; then
        log "Mise à jour du dépôt dans ${SRC_DIR}..."
        git -C "${SRC_DIR}" fetch --depth 1 origin main
        git -C "${SRC_DIR}" reset --hard origin/main
    else
        log "Clonage de Papyrus dans ${SRC_DIR}..."
        rm -rf "${SRC_DIR}"
        git clone --depth 1 "${REPO_URL}" "${SRC_DIR}"
    fi
fi

log "Installation des dépendances de compilation (sudo requis)..."
sudo apt-get update
sudo apt-get install -y \
    build-essential cmake git curl ca-certificates \
    qt6-base-dev qt6-pdf-dev

log "Récupération de PDFium..."
"${SRC_DIR}/scripts/fetch-pdfium.sh"

BUILD_DIR="${SRC_DIR}/build"
log "Compilation (${BUILD_TYPE})..."
cmake -S "${SRC_DIR}" -B "${BUILD_DIR}" -DCMAKE_BUILD_TYPE="${BUILD_TYPE}"
cmake --build "${BUILD_DIR}" -j"$(nproc)"

log "Génération du paquet .deb..."
(cd "${BUILD_DIR}" && cpack -G DEB)

DEB_FILE="$(find "${BUILD_DIR}" -maxdepth 1 -name 'papyrus_*_amd64.deb' -print -quit)"
[ -n "${DEB_FILE}" ] || die "Le paquet .deb n'a pas été généré (voir la sortie de cpack ci-dessus)."

log "Installation de ${DEB_FILE} (sudo requis)..."
sudo apt-get install -y "${DEB_FILE}"

log "Papyrus est installé — lancez-le avec 'papyrus' ou depuis le menu des applications."
