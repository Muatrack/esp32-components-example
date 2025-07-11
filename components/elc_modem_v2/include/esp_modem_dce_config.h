/*
 * SPDX-FileCopyrightText: 2021-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/** @addtogroup ESP_MODEM_CONFIG
 * @{
 */

/**
 * @brief ESP Modem DCE Default Configuration
 *
 */
#define ESP_MODEM_DCE_DEFAULT_CONFIG(APN, POWON, POWOFF)       \
    {                                           \
        .powon_pin = POWON,                        \
        .powoff_pin = POWOFF,                        \
        .apn = APN,                        \
    }

#define ESP_TEST_CONFIG()   \
    {   \
    }

typedef struct esp_modem_dce_config esp_modem_dce_config_t;

/**
 * @brief DCE configuration structure
 */
struct esp_modem_dce_config {
    int powon_pin;
    int powoff_pin;
    const char *apn;  /*!< APN: Logical name of the Access point */
};

/**
 * @}
 */

#ifdef __cplusplus
}
#endif
