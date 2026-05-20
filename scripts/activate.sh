# Source me, don't execute me: `source ./scripts/activate.sh`
#
# Loads ESP-IDF and ESP-Matter into the current shell. Safe to source multiple
# times in the same shell.

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]:-$0}")/.." && pwd)"
TOOLCHAIN_DIR="${REPO_ROOT}/toolchain"

if [ ! -d "${TOOLCHAIN_DIR}/esp-idf" ] || [ ! -d "${TOOLCHAIN_DIR}/esp-matter" ]; then
    echo "Toolchain not found at ${TOOLCHAIN_DIR}. Run ./scripts/setup.sh first." >&2
    return 1 2>/dev/null || exit 1
fi

# shellcheck disable=SC1091
source "${TOOLCHAIN_DIR}/esp-idf/export.sh"

export ESP_MATTER_PATH="${TOOLCHAIN_DIR}/esp-matter"
export ESP_MATTER_DEVICE_PATH="${ESP_MATTER_PATH}/device_hal/device/esp32s3_devkit_c"

echo "✅ ESP-IDF: $(idf.py --version 2>/dev/null | head -1)"
echo "✅ ESP-Matter at ${ESP_MATTER_PATH}"
