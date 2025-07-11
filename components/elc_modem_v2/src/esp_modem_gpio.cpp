
#include "esp_modem_gpio.hpp"
#include ""

// LTEPowGpio::LTEPowGpio(gpio_num_t onPin, gpio_num_t offPin): powon_pin(onPin), powoff_pin(offPin){
        
//         gpio_config_t gt = {
//             .pin_bit_mask = ((1U << 12)),
//             .mode   =   GPIO_MODE_OUTPUT,  
//             .pull_up_en = GPIO_PULLUP_ENABLE,
//         };

//         gpio_config(&gt);

//         // gpio_set_level(14, 1);
//         gpio_set_level(12, 1);
//         ESP_LOGI("MODEM_GPIO", "--------- %s ---------- %d -- set 1 holding 10s----", __FUNCTION__, __LINE__);
//         // usleep(100);
//         sleep(10);
//         gpio_set_level(12, 0);
//         // usleep(100);
//         ESP_LOGI("MODEM_GPIO", "--------- %s ---------- %d -- set 0 holding 10s----", __FUNCTION__, __LINE__);
//         sleep(10);
//         gpio_set_level(12, 1);
//         // usleep(100);
//         ESP_LOGI("MODEM_GPIO", "--------- %s ---------- %d -- set 1 holding 10s----", __FUNCTION__, __LINE__);
//         sleep(10);
//         gpio_set_level(12, 0);
//         ESP_LOGI("MODEM_GPIO", "--------- %s ---------- %d -- set 0 ----", __FUNCTION__, __LINE__);
// }

void LTEPowGpio::lte_poweron(){
    // gpio_set_level(powon_pin, 1);
    // usleep(1500000);
    // usleep(100);
};

void LTEPowGpio::lte_poweroff(){
    gpio_set_level(powoff_pin, 0);
    usleep(100);
    gpio_set_level(powoff_pin, 1);
    usleep(100);
    gpio_set_level(powoff_pin, 0);
};