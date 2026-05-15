# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
# SPDX-License-Identifier: Apache-2.0

idf_component_register(
    SRC_DIRS "."
    PRIV_INCLUDE_DIRS "."
    PRIV_REQUIRES unity esp_config_manager nvs_flash
    WHOLE_ARCHIVE TRUE
)
