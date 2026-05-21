#!/usr/bin/env bash
# One-time installer for ESP-IDF v5.5.x and esp-homekit-sdk.
#
# Installs everything under ./toolchain/ (kept out of git by .gitignore).
# Idempotent: re-running is safe — it skips already-cloned repos but refreshes submodules.

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
TOOLCHAIN_DIR="${REPO_ROOT}/toolchain"
IDF_VERSION="${IDF_VERSION:-v5.5.3}"
HOMEKIT_VERSION="${HOMEKIT_VERSION:-master}"

# Espressif's installer uses Python's stdlib `urllib` which on some macOS Python
# builds does not trust the system keychain. Point it at the `certifi` bundle
# so downloads of toolchain tarballs over HTTPS don't fail with CERTIFICATE_VERIFY_FAILED.
if command -v python3 >/dev/null 2>&1; then
    CERT_BUNDLE="$(python3 -c 'import certifi; print(certifi.where())' 2>/dev/null || true)"
    if [ -n "${CERT_BUNDLE}" ] && [ -f "${CERT_BUNDLE}" ]; then
        export SSL_CERT_FILE="${CERT_BUNDLE}"
        export REQUESTS_CA_BUNDLE="${CERT_BUNDLE}"
    fi
fi

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

# --- 2. esp-homekit-sdk -----------------------------------------------------
if [ ! -d "esp-homekit-sdk/.git" ]; then
    echo ">>> Cloning esp-homekit-sdk ${HOMEKIT_VERSION}"
    git clone -b "${HOMEKIT_VERSION}" --depth 1 --recursive \
        https://github.com/espressif/esp-homekit-sdk.git
else
    echo ">>> esp-homekit-sdk already cloned — skipping clone"
fi

echo ""
echo "✅ Setup complete."
echo "   Next: source ./scripts/activate.sh"
