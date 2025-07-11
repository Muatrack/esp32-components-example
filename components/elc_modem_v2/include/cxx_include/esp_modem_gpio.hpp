#include <iostream>
#include "driver/gpio.h"

class LTEPowGpio {
    public:
        LTEPowGpio(gpio_num_t onPin, gpio_num_t offPin): powon_pin(onPin), powoff_pin(offPin){
            gpio_config_t gt = {
                .pin_bit_mask = ((1U << onPin) | (1U << offPin)),
                .mode   =   GPIO_MODE_OUTPUT,  
                .pull_up_en = GPIO_PULLUP_ENABLE,
                .pull_down_en = GPIO_PULLDOWN_DISABLE,
                .intr_type = GPIO_INTR_DISABLE
            };

            gpio_config(&gt);

            gpio_set_level(powon_pin, 1);
            gpio_set_level(powoff_pin, 1);
        }

        ~LTEPowGpio(){
            gpio_set_level(powon_pin, 1);
            gpio_set_level(powoff_pin, 1);
        }
    public:
        void lte_poweron(){
            gpio_set_level(powon_pin, 1);
            gpio_set_level(powoff_pin, 1);
            usleep(10);
            gpio_set_level(powon_pin, 0);
            usleep(1500000);
            gpio_set_level(powon_pin, 1);
            sleep(2);
        };

        void lte_poweroff(){
            gpio_set_level(powon_pin, 1);    
            gpio_set_level(powoff_pin, 1);
            usleep(10);
            gpio_set_level(powoff_pin, 0);
            usleep(10);
            gpio_set_level(powoff_pin, 1);
        };

    private:
        gpio_num_t powon_pin;
        gpio_num_t powoff_pin;
};