cmake_minimum_required(VERSION 3.16)

if(NOT DEFINED ENV{HOMEKIT_PATH})
    message(FATAL_ERROR
        "HOMEKIT_PATH is not set. Run `source ./scripts/activate.sh` first.")
endif()

set(PROJECT_VER "0.1.0")
set(PROJECT_VER_NUMBER 1)

set(HOMEKIT_PATH $ENV{HOMEKIT_PATH})

# esp-homekit-sdk ships its HAP components under components/homekit and a few
# example helpers (app_wifi, app_hap_setup_payload, qrcode) under examples/common.
set(EXTRA_COMPONENT_DIRS
    "${HOMEKIT_PATH}/components"
    "${HOMEKIT_PATH}/components/homekit"
    "${HOMEKIT_PATH}/examples/common")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(stock_alert)

idf_build_set_property(CXX_COMPILE_OPTIONS "-std=gnu++17;-Os" APPEND)
idf_build_set_property(C_COMPILE_OPTIONS "-Os" APPEND)
