/*
 * SPDX-FileCopyrightText: 2021-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "cxx_include/esp_modem_api.hpp"
#include "cxx_include/esp_modem_dce_module.hpp"
#include "generate/esp_modem_command_declare.inc"

namespace esp_modem {

GenericModule::GenericModule(std::shared_ptr<DTE> dte, const dce_config *config) :
    dte(std::move(dte)), pdp(std::make_unique<PdpContext>(config->apn)), powon_pin(config->powon_pin), powoff_pin(config->powoff_pin) {}

//
// Define preprocessor's forwarding to dce_commands definitions
//

// Helper macros to handle multiple arguments of declared API
#define ARGS0
#define ARGS1 , p1
#define ARGS2 , p1 , p2
#define ARGS3 , p1 , p2 , p3
#define ARGS4 , p1 , p2 , p3, p4
#define ARGS5 , p1 , p2 , p3, p4, p5
#define ARGS6 , p1 , p2 , p3, p4, p5, p6

#define _ARGS(x)  ARGS ## x
#define ARGS(x)  _ARGS(x)

//
// Repeat all declarations and forward to the AT commands defined in esp_modem::dce_commands:: namespace
//
#define ESP_MODEM_DECLARE_DCE_COMMAND(name, return_type, arg_nr, ...) \
     return_type GenericModule::name(__VA_ARGS__) { return esp_modem::dce_commands::name(dte.get() ARGS(arg_nr) ); }

DECLARE_ALL_COMMAND_APIS(return_type name(...) )

#undef ESP_MODEM_DECLARE_DCE_COMMAND

//
// Handle specific commands for specific supported modems
//
command_result SIM7600::get_battery_status(int &voltage, int &bcs, int &bcl)
{
    return dce_commands::get_battery_status_sim7xxx(dte.get(), voltage, bcs, bcl);
}

command_result SIM7600::set_network_bands(const std::string &mode, const int *bands, int size)
{
    return dce_commands::set_network_bands_sim76xx(dte.get(), mode, bands, size);
}

command_result SIM7600::set_gnss_power_mode(int mode)
{
    return dce_commands::set_gnss_power_mode_sim76xx(dte.get(), mode);
}

command_result SIM7600::power_down()
{
    return dce_commands::power_down_sim76xx(dte.get());
}

command_result SIM7070::power_down()
{
    return dce_commands::power_down_sim70xx(dte.get());
}

command_result SIM7000::power_down()
{
    return dce_commands::power_down_sim70xx(dte.get());
}

command_result SIM800::power_down()
{
    return dce_commands::power_down_sim8xx(dte.get());
}

command_result SIM800::set_data_mode()
{
    return dce_commands::set_data_mode_sim8xx(dte.get());
}

command_result AIR780E::set_data_mode() {
    return dce_commands::set_data_mode_air780e(dte.get());
}

bool AIR780E::setup_data_mode() {
    if (power_reup() != command_result::OK) {
        return false;
    }
    
    if (set_echo(false) != command_result::OK) {
        return false;
    }

    //查询模块名称
    std::string module_name;
    if (get_module_name(module_name) != command_result::OK) {
        return false;
    }
    set_module_name(module_name);

    sleep(4);
    if (set_pdp_context(*pdp) != command_result::OK) {
        return false;
    }
    return true;
}

command_result AIR780E::power_reup() {
    return command_result::OK;
}

// command_result AIR780E::get_signal_quality(int& rssi, int& ber) {
//     return get_signal_quality(rssi, ber);
// }

command_result AIR780E::lte_network_state_get(int &reportSt, int &netSt){
    // ESP_LOGI("MODEM_MODULES","Hello, in class AIR780E");
    get_lte_network_state( reportSt, netSt );
    return command_result::OK;
}

void AIR780E::lte_module_reset(void) { lte_reset(); }

}

