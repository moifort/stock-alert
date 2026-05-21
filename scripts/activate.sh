# Source me, don't execute me: `source ./scripts/activate.sh`
#
# Loads ESP-IDF and esp-homekit-sdk into the current shell. Safe to source
# multiple times in the same shell.

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]:-$0}")/.." && pwd)"
TOOLCHAIN_DIR="${REPO_ROOT}/toolchain"

if [ ! -d "${TOOLCHAIN_DIR}/esp-idf" ] || [ ! -d "${TOOLCHAIN_DIR}/esp-homekit-sdk" ]; then
    echo "Toolchain not found at ${TOOLCHAIN_DIR}. Run ./scripts/setup.sh first." >&2
    return 1 2>/dev/null || exit 1
fi

# Ensure HTTPS downloads from idf_tools.py succeed on macOS.
if command -v python3 >/dev/null 2>&1; then
    CERT_BUNDLE="$(python3 -c 'import certifi; print(certifi.where())' 2>/dev/null || true)"
    if [ -n "${CERT_BUNDLE}" ] && [ -f "${CERT_BUNDLE}" ]; then
        export SSL_CERT_FILE="${CERT_BUNDLE}"
        export REQUESTS_CA_BUNDLE="${CERT_BUNDLE}"
    fi
fi

export HOMEKIT_PATH="${TOOLCHAIN_DIR}/esp-homekit-sdk"

# shellcheck disable=SC1091
source "${TOOLCHAIN_DIR}/esp-idf/export.sh"

echo "✅ ESP-IDF: $(idf.py --version 2>/dev/null | head -1)"
echo "✅ esp-homekit-sdk at ${HOMEKIT_PATH}"
