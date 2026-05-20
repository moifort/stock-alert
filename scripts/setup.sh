#!/usr/bin/env bash
# One-time installer for ESP-IDF v5.5.x and ESP-Matter SDK v1.4.x.
#
# Installs everything under ./toolchain/ (kept out of git by .gitignore).
# Idempotent: re-running is safe — it skips already-cloned repos but refreshes submodules.

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
TOOLCHAIN_DIR="${REPO_ROOT}/toolchain"
IDF_VERSION="${IDF_VERSION:-v5.5.3}"
MATTER_VERSION="${MATTER_VERSION:-release/v1.4.2}"

mkdir -p "${TOOLCHAIN_DIR}"
cd "${TOOLCHAIN_DIR}"

# --- 1. ESP-IDF -------------------------------------------------------------
if [ ! -d "esp-idf/.git" ]; then
    echo ">>> Cloning ESP-IDF ${IDF_VERSION}"
    git clone -b "${IDF_VERSION}" --depth 1 --recursive \
        https://github.com/espressif/esp-idf.git
else
    echo ">>> ESP-IDF already cloned at ${TOOLCHAIN_DIR}/esp-idf — skipping clone"
fi

echo ">>> Installing ESP-IDF tools for esp32s3"
./esp-idf/install.sh esp32s3

# --- 2. ESP-Matter ----------------------------------------------------------
if [ ! -d "esp-matter/.git" ]; then
    echo ">>> Cloning ESP-Matter ${MATTER_VERSION}"
    git clone -b "${MATTER_VERSION}" --depth 1 \
        https://github.com/espressif/esp-matter.git
fi

cd esp-matter
echo ">>> Initializing connectedhomeip submodule (this can take several minutes)"
git submodule update --init --depth 1
cd connectedhomeip/connectedhomeip
./scripts/checkout_submodules.py --shallow --platform esp32 --recursive
cd ../..

echo ">>> Bootstrapping Matter SDK"
# shellcheck disable=SC1091
source "${TOOLCHAIN_DIR}/esp-idf/export.sh"
./install.sh

echo ""
echo "✅ Setup complete."
echo "   Next: source ./scripts/activate.sh"
