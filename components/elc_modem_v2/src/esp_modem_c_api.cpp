/*
 * SPDX-FileCopyrightText: 2021-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <cassert>
#include "cxx_include/esp_modem_dte.hpp"
#include "uart_terminal.hpp"
#include "esp_log.h"
#include "cxx_include/esp_modem_api.hpp"
#include "cxx_include/esp_modem_dce_factory.hpp"
#include "esp_modem_c_api.h"
#include "esp_modem_config.h"
#include "exception_stub.hpp"
#include "esp_private/c_api_wrapper.hpp"
#include "cstring"

#ifndef ESP_MODEM_C_API_STR_MAX
#define ESP_MODEM_C_API_STR_MAX 64
#endif

#ifndef HAVE_STRLCPY
size_t strlcpy(char *dest, const char *src, size_t len);
#endif

static bool modem_gotip = false;
//
// C API definitions
using namespace esp_modem;
static struct esp_modem_dce_wrap *dce_wrap = nullptr;

static EventBits_t GOT_IP = BIT1;
static void on_modem_event(void *arg, esp_event_base_t event_base,
                           int32_t event_id, void *event_data)
{
    if (event_base == IP_EVENT) {
        // //slog_info("IP event! %d", event_id);
        if (event_id == IP_EVENT_PPP_GOT_IP) {
            esp_netif_dns_info_t dns_info;

            ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
            esp_netif_t *netif = event->esp_netif;

            ESP_LOGI("MODEM","Modem Connect to PPP Server\n");
            ESP_LOGI("MODEM","~~~~~~~~~~~~~~\n");
            ESP_LOGI("MODEM","IP          : " IPSTR, IP2STR(&event->ip_info.ip));
            ESP_LOGI("MODEM","\n");
            ESP_LOGI("MODEM","Netmask     : " IPSTR, IP2STR(&event->ip_info.netmask));
            ESP_LOGI("MODEM","\n");
            ESP_LOGI("MODEM","Gateway     : " IPSTR, IP2STR(&event->ip_info.gw));
            ESP_LOGI("MODEM","\n");
            esp_netif_get_dns_info(netif, ESP_NETIF_DNS_MAIN, &dns_info);
            ESP_LOGI("MODEM","Name Server1: " IPSTR, IP2STR(&dns_info.ip.u_addr.ip4));
            ESP_LOGI("MODEM","\n");
            esp_netif_get_dns_info(netif, ESP_NETIF_DNS_BACKUP, &dns_info);
            ESP_LOGI("MODEM","Name Server2: " IPSTR, IP2STR(&dns_info.ip.u_addr.ip4));
            ESP_LOGI("MODEM","\n");
            ESP_LOGI("MODEM","~~~~~~~~~~~~~~\n");

            // //slog_info("GOT ip event!!!\n");
            xEventGroupSetBits(dce_wrap->event_group, GOT_IP);
            modem_gotip = true;
            
        } else if (event_id == IP_EVENT_PPP_LOST_IP) {
            // //slog_info("Modem Disconnect from PPP Server\n");
            // xEventGroupSetBits(event_group, LossIP);
        } else if (event_id == IP_EVENT_GOT_IP6) {
            //slog_info("GOT IPv6 event!\n");
            ip_event_got_ip6_t *event = (ip_event_got_ip6_t *)event_data;
            //slog_info("Got IPv6 address " IPV6STR, IPV62STR(event->ip6_info.ip));
        }
    }
}

extern "C" esp_modem_dce_t *esp_modem_new_dev(esp_modem_dce_device_t module, const esp_modem_dte_config_t *dte_config, const esp_modem_dce_config_t *dce_config, esp_netif_t *netif)
{
    if (dce_wrap != nullptr) 
        return nullptr;
        
    // auto dce_wrap = new (std::nothrow) esp_modem_dce_wrap;
    dce_wrap = new (std::nothrow) esp_modem_dce_wrap;
    if (dce_wrap == nullptr) {
        return nullptr;
    }
    auto dte = create_uart_dte(dte_config);
    if (dte == nullptr) {
        delete dce_wrap;
        return nullptr;
    }
    dce_wrap->dte = dte;
    dce_wrap->pppos_netif = netif;
    dce_factory::Factory f(convert_modem_enum(module));
    dce_wrap->dce = f.build(dce_config, std::move(dte), netif);
    if (dce_wrap->dce == nullptr) {
        delete dce_wrap;
        return nullptr;
    }
    dce_wrap->modem_type = convert_modem_enum(module);
    dce_wrap->dte_type = esp_modem_dce_wrap::modem_wrap_dte_type::UART;
    dce_wrap->event_group = xEventGroupCreate();
    dce_wrap->module_ready = false;
    dce_wrap->ipv4 = std::string("0.0.0.0");
    return dce_wrap;
}

extern "C" esp_modem_dce_t *esp_modem_new(const esp_modem_dte_config_t *dte_config, const esp_modem_dce_config_t *dce_config, esp_netif_t *netif)
{
    return esp_modem_new_dev(ESP_MODEM_DCE_AIR780E, dte_config, dce_config, netif);
}

extern "C" void esp_modem_destroy(esp_modem_dce_t *dce_wrap)
{
    if (dce_wrap) {
        delete dce_wrap->dce;
        delete dce_wrap;
    }
}

extern "C" esp_err_t esp_modem_set_error_cb(esp_modem_dce_t *dce_wrap, esp_modem_terminal_error_cbt err_cb)
{
    if (dce_wrap == nullptr || dce_wrap->dce == nullptr || dce_wrap->dte == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }

    if (err_cb) {
        dce_wrap->dte->set_error_cb([err_cb](terminal_error err) {
            err_cb(convert_terminal_error_enum(err));
        });
    } else {
        dce_wrap->dte->set_error_cb(nullptr);
    }
    return ESP_OK;
}

extern "C" esp_err_t esp_modem_sync(esp_modem_dce_t *dce_wrap)
{
    if (dce_wrap == nullptr || dce_wrap->dce == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }
    return command_response_to_esp_err(dce_wrap->dce->sync());
}

extern "C" void esp_modem_poweron()
{
    if (dce_wrap == nullptr || dce_wrap->dce == nullptr) {
        // return ESP_ERR_INVALID_ARG;
        return;
    }
    dce_wrap->dce->get_module()->poweron();
}

extern "C" void esp_modem_get_lbs(char *pLongi, char *pLati)
{
    std::string longi;
    std::string lati;
    static bool modem_event_registed = false;
    if (dce_wrap == nullptr || dce_wrap->dce == nullptr) {
        return;
    }
    dce_wrap->dce->get_module()->lte_network_location_get(longi, lati);
    strcpy(pLongi, longi.c_str());
    strcpy(pLati, lati.c_str());
}

extern "C" void esp_modem_reset()
{
    esp_err_t err;
    static bool modem_event_registed = false;
    if (dce_wrap == nullptr || dce_wrap->dce == nullptr) {
        return;
    }

    // esp_modem_reset_pppos();
    // esp_modem_poweron();
    if( err = esp_modem_set_mode(ESP_MODEM_MODE_COMMAND), err != ESP_OK) {
        ESP_LOGI("MODEM", "set pppos command excp:%s", esp_err_to_name(err));
    } else {
        ESP_LOGI("MODEM", "set pppos command OK");
    }
#if 1
    dce_wrap->dce->get_module()->poweron();
#else
    esp_modem_lte_reset();
#endif
    // if( !modem_event_registed )
    {
        esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, on_modem_event, NULL);
        modem_event_registed = true;
    }
}

// extern "C" esp_err_t esp_modem_wait_ready(esp_modem_dce_t *dce_wrap, uint32_t timeout_ms)
extern "C" esp_err_t esp_modem_wait_ready(uint32_t timeout_ms)
{
    if (dce_wrap == nullptr || dce_wrap->dce == nullptr) {
        // return ESP_ERR_INVALID_ARG;
        return ESP_FAIL;
    }

    return command_response_to_esp_err(dce_wrap->dce->get_module()->wait_ready(timeout_ms));
}

// extern "C" esp_err_t esp_modem_set_mode(esp_modem_dce_t *dce_wrap, esp_modem_dce_mode_t mode)
esp_err_t esp_modem_set_mode(esp_modem_dce_mode_t mode)
{
    if (dce_wrap == nullptr || dce_wrap->dce == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }
    switch (mode) {
    case ESP_MODEM_MODE_DATA:
        return dce_wrap->dce->set_mode(modem_mode::DATA_MODE) ? ESP_OK : ESP_FAIL;
    case ESP_MODEM_MODE_COMMAND:
        return dce_wrap->dce->set_mode(modem_mode::COMMAND_MODE) ? ESP_OK : ESP_FAIL;
    case ESP_MODEM_MODE_CMUX:
        return dce_wrap->dce->set_mode(modem_mode::CMUX_MODE) ? ESP_OK : ESP_FAIL;
    case ESP_MODEM_MODE_CMUX_MANUAL:
        return dce_wrap->dce->set_mode(modem_mode::CMUX_MANUAL_MODE) ? ESP_OK : ESP_FAIL;
    case ESP_MODEM_MODE_CMUX_MANUAL_EXIT:
        return dce_wrap->dce->set_mode(modem_mode::CMUX_MANUAL_EXIT) ? ESP_OK : ESP_FAIL;
    case ESP_MODEM_MODE_CMUX_MANUAL_SWAP:
        return dce_wrap->dce->set_mode(modem_mode::CMUX_MANUAL_SWAP) ? ESP_OK : ESP_FAIL;
    case ESP_MODEM_MODE_CMUX_MANUAL_DATA:
        return dce_wrap->dce->set_mode(modem_mode::CMUX_MANUAL_DATA) ? ESP_OK : ESP_FAIL;
    case ESP_MODEM_MODE_CMUX_MANUAL_COMMAND:
        return dce_wrap->dce->set_mode(modem_mode::CMUX_MANUAL_COMMAND) ? ESP_OK : ESP_FAIL;
    }
    return ESP_ERR_NOT_SUPPORTED;
}

extern "C" esp_err_t esp_modem_lte_get_state(uint8_t *urc, uint8_t *st)
{
    int a = 0;
    int b = 0;
    if (dce_wrap == nullptr || dce_wrap->dce == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }
    // ( dce_wrap->dce->get_lte_state(a,b) )?ESP_OK:ESP_FAIL;
    // ESP_LOGI("MODEM_C_API", "a:%d,b:%d", *urc,*st);
    dce_wrap->dce->get_lte_state(a,b);
    ESP_LOGI("MODEM_C_API", "a:%d,b:%d", *urc,*st);
    *urc = a;
    *st  = b;
    return ESP_OK;
}

extern "C" esp_err_t esp_modem_start_pppos(uint32_t timeout_ms){

    esp_err_t err;
    esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, on_modem_event, NULL);
#if 0
    if( err = esp_modem_set_mode(ESP_MODEM_MODE_COMMAND), err != ESP_OK) {
        ESP_LOGI("MODEM", "set pppos command excp:%s", esp_err_to_name(err));
    } else {
        ESP_LOGI("MODEM", "set pppos command OK");
    }
#endif
    if( err = esp_modem_set_mode(ESP_MODEM_MODE_CMUX), err != ESP_OK) {
        ESP_LOGI("MODEM", "set pppos excp:%s", esp_err_to_name(err));
    } else {
        ESP_LOGI("MODEM", "set pppos OK");
    }
    if ((xEventGroupWaitBits(dce_wrap->event_group, GOT_IP, pdTRUE, pdTRUE, pdMS_TO_TICKS(timeout_ms)) & GOT_IP) != GOT_IP) {
      if(dce_wrap) dce_wrap->module_ready = true;
      return ESP_FAIL;
    }
    if(dce_wrap) dce_wrap->module_ready = true;
    return ESP_OK;
}

extern "C" void esp_modem_reset_pppos(){
    esp_modem_set_mode(ESP_MODEM_MODE_COMMAND);
}

/**
 * @deprecated 放弃使用
*/
extern "C" esp_err_t esp_modem_get_ip_addr(esp_netif_ip_info_t *ip){
    if(ip == nullptr)
        return ESP_FAIL;

    return esp_netif_get_ip_info(dce_wrap->pppos_netif, ip);
}

extern "C" esp_err_t esp_modem_lte_reset()
{
    bool bPinSt = false;;
    if (dce_wrap == nullptr || dce_wrap->dce == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }
    dce_wrap->dce->lte_reset();
    return ESP_OK;
}

extern "C" esp_err_t esp_modem_read_pin()
{   
    bool bPinSt = false;;
    if (dce_wrap == nullptr || dce_wrap->dce == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }

    if( command_response_to_esp_err(dce_wrap->dce->read_pin(bPinSt)) != ESP_OK )
        return ESP_FAIL;
    if( !bPinSt )
        return ESP_FAIL;
    return ESP_OK;
}

extern "C" esp_err_t esp_modem_sms_txt_mode(esp_modem_dce_t *dce_wrap, bool txt)
{
    if (dce_wrap == nullptr || dce_wrap->dce == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }

    return command_response_to_esp_err(dce_wrap->dce->sms_txt_mode(txt));
}

extern "C" esp_err_t esp_modem_send_sms(esp_modem_dce_t *dce_wrap, const char *number, const char *message)
{
    if (dce_wrap == nullptr || dce_wrap->dce == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }
    std::string number_str(number);
    std::string message_str(message);
    return command_response_to_esp_err(dce_wrap->dce->send_sms(number_str, message_str));
}

extern "C" esp_err_t esp_modem_sms_character_set(esp_modem_dce_t *dce_wrap)
{
    if (dce_wrap == nullptr || dce_wrap->dce == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }

    return command_response_to_esp_err(dce_wrap->dce->sms_character_set());
}

extern "C" esp_err_t esp_modem_set_pin(esp_modem_dce_t *dce_wrap, const char *pin)
{
    if (dce_wrap == nullptr || dce_wrap->dce == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }
    std::string pin_str(pin);
    return command_response_to_esp_err(dce_wrap->dce->set_pin(pin_str));
}

extern "C" esp_err_t esp_modem_at(esp_modem_dce_t *dce_wrap, const char *at, char *p_out, int timeout)
{
    if (dce_wrap == nullptr || dce_wrap->dce == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }
    std::string out;
    std::string at_str(at);
    auto ret = command_response_to_esp_err(dce_wrap->dce->at(at_str, out, timeout));
    if ((p_out != NULL) && (!out.empty())) {
        strlcpy(p_out, out.c_str(), ESP_MODEM_C_API_STR_MAX);
    }
    return ret;
}

extern "C" esp_err_t esp_modem_get_signal_quality(int *rssi, int *ber)
{
    if (dce_wrap == nullptr || dce_wrap->dce == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }

    if (dce_wrap == nullptr || dce_wrap->dce == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }
    return command_response_to_esp_err(dce_wrap->dce->get_signal_quality(*rssi, *ber));
}

extern "C" esp_err_t esp_modem_get_module_ver(char* p_ver)
{
    sprintf(p_ver, "module name");
    if (dce_wrap == nullptr || dce_wrap->dce == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }

    std::string ver;
    auto ret = command_response_to_esp_err(dce_wrap->dce->get_module_ver(ver));
    if (ret == ESP_OK && !ver.empty()) {
        strlcpy(p_ver, ver.c_str(), ESP_MODEM_C_API_STR_MAX);
    }
    return ret;
}

/**
 * @brief 回写 ipv4 addr
*/
extern "C" uint8_t esp_modem_save_ip(char* p_ip)
{
    if(dce_wrap)
        dce_wrap->ipv4 = std::string(p_ip);

    return 0;
}

/**
 * @brief 获取4g lte模块的ipv4地址、在用的apn
 * @param p_ip 保存ip的变量地址， p_apn 保存apn的变量地址
 * @return 成功返回 ESP_OK, 失败返回其它
*/
extern "C" esp_err_t esp_modem_get_apn_ip(char* p_ip, char* p_apn)
{
    sprintf(p_ip, "0.0.0.0");
    sprintf(p_apn, "apn");

    if (dce_wrap == nullptr || dce_wrap->dce == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }

    if(dce_wrap->module_ready == false) {
        sprintf(p_ip, "0.0.0.0");
        return ESP_FAIL;
    }

    sprintf(p_ip, dce_wrap->ipv4.c_str());
    return ESP_OK;

    // std::string ip, apn;
    // auto ret = command_response_to_esp_err(dce_wrap->dce->get_apn_ip_addr(ip, apn));
    // if (ret == ESP_OK && !ip.empty()) {
    //     strlcpy(p_ip, ip.c_str(), ESP_MODEM_C_API_STR_MAX);
    //     strlcpy(p_apn, apn.c_str(), ESP_MODEM_C_API_STR_MAX);
    // }
    // return ret;
}

// extern "C" esp_err_t esp_modem_get_imsi(esp_modem_dce_t *dce_wrap, char *p_imsi)
extern "C" uint8_t esp_modem_get_imsi(char *p_imsi)
{
    sprintf(p_imsi, "imsi");

    if (dce_wrap == nullptr || dce_wrap->dce == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }

    std::string imsi;
    auto ret = command_response_to_esp_err(dce_wrap->dce->get_imsi(imsi));
    if (ret == ESP_OK && !imsi.empty()) {
        strlcpy(p_imsi, imsi.c_str(), ESP_MODEM_C_API_STR_MAX);
    }
    return (ret == ESP_OK)?0:1;
}

extern "C" esp_err_t esp_modem_set_flow_control(esp_modem_dce_t *dce_wrap, int dce_flow, int dte_flow)
{
    if (dce_wrap == nullptr || dce_wrap->dce == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }
    return command_response_to_esp_err(dce_wrap->dce->set_flow_control(dce_flow, dte_flow));
}

extern "C" esp_err_t esp_modem_store_profile(esp_modem_dce_t *dce_wrap)
{
    if (dce_wrap == nullptr || dce_wrap->dce == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }
    return command_response_to_esp_err(dce_wrap->dce->store_profile());
}

// extern "C" esp_err_t esp_modem_get_imei(esp_modem_dce_t *dce_wrap, char *p_imei)
extern "C" uint8_t esp_modem_get_imei(char *p_imei)
{
    sprintf(p_imei, "imei");

    if (dce_wrap == nullptr || dce_wrap->dce == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }

    std::string imei;
    auto ret = command_response_to_esp_err(dce_wrap->dce->get_imei(imei));
    if (ret == ESP_OK && !imei.empty()) {
        strlcpy(p_imei, imei.c_str(), ESP_MODEM_C_API_STR_MAX);
    }
    return ret;
}

extern "C" esp_err_t esp_modem_get_operator_name(esp_modem_dce_t *dce_wrap, char *p_name, int *p_act)
{
    if (dce_wrap == nullptr || dce_wrap->dce == nullptr || p_name == nullptr || p_act == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }
    std::string name;
    int act;
    auto ret = command_response_to_esp_err(dce_wrap->dce->get_operator_name(name, act));
    if (ret == ESP_OK && !name.empty()) {
        strlcpy(p_name, name.c_str(), ESP_MODEM_C_API_STR_MAX);
        *p_act = act;
    }
    return ret;
}

extern "C" esp_err_t esp_modem_get_module_name(esp_modem_dce_t *dce_wrap, char *p_name)
{
    if (dce_wrap == nullptr || dce_wrap->dce == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }
    std::string name;
    auto ret = command_response_to_esp_err(dce_wrap->dce->get_module_name(name));
    if (ret == ESP_OK && !name.empty()) {
        strlcpy(p_name, name.c_str(), ESP_MODEM_C_API_STR_MAX);
    }
    return ret;
}

extern "C" esp_err_t esp_modem_get_battery_status(esp_modem_dce_t *dce_wrap, int *p_volt, int *p_bcs, int *p_bcl)
{
    if (dce_wrap == nullptr || dce_wrap->dce == nullptr || p_bcs == nullptr || p_bcl == nullptr || p_volt == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }
    int bcs, bcl, volt;
    auto ret = command_response_to_esp_err(dce_wrap->dce->get_battery_status(volt, bcs, bcl));
    if (ret == ESP_OK) {
        *p_volt = volt;
        *p_bcs = bcs;
        *p_bcl = bcl;
    }
    return ret;
}

extern "C" esp_err_t esp_modem_power_down(esp_modem_dce_t *dce_wrap)
{
    if (dce_wrap == nullptr || dce_wrap->dce == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }
    return command_response_to_esp_err(dce_wrap->dce->power_down());
}

extern "C" esp_err_t esp_modem_set_operator(esp_modem_dce_t *dce_wrap, int mode, int format, const char *oper)
{
    if (dce_wrap == nullptr || dce_wrap->dce == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }
    std::string operator_str(oper);
    return command_response_to_esp_err(dce_wrap->dce->set_operator(mode, format, operator_str));
}

extern "C" esp_err_t esp_modem_set_network_attachment_state(esp_modem_dce_t *dce_wrap, int state)
{
    if (dce_wrap == nullptr || dce_wrap->dce == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }
    return command_response_to_esp_err(dce_wrap->dce->set_network_attachment_state(state));
}

extern "C" esp_err_t esp_modem_get_network_attachment_state(esp_modem_dce_t *dce_wrap, int *p_state)
{
    if (dce_wrap == nullptr || dce_wrap->dce == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }
    int state;
    auto ret = command_response_to_esp_err(dce_wrap->dce->get_network_attachment_state(state));
    if (ret == ESP_OK) {
        *p_state = state;
    }
    return ret;
}

extern "C" esp_err_t esp_modem_set_radio_state(esp_modem_dce_t *dce_wrap, int state)
{
    if (dce_wrap == nullptr || dce_wrap->dce == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }
    return command_response_to_esp_err(dce_wrap->dce->set_radio_state(state));
}

extern "C" esp_err_t esp_modem_get_radio_state(esp_modem_dce_t *dce_wrap, int *p_state)
{
    if (dce_wrap == nullptr || dce_wrap->dce == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }
    int state;
    auto ret = command_response_to_esp_err(dce_wrap->dce->get_radio_state(state));
    if (ret == ESP_OK) {
        *p_state = state;
    }
    return ret;
}

extern "C" esp_err_t esp_modem_set_network_mode(esp_modem_dce_t *dce_wrap, int mode)
{
    if (dce_wrap == nullptr || dce_wrap->dce == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }
    return command_response_to_esp_err(dce_wrap->dce->set_network_mode(mode));
}

extern "C" esp_err_t esp_modem_set_preferred_mode(esp_modem_dce_t *dce_wrap, int mode)
{
    if (dce_wrap == nullptr || dce_wrap->dce == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }
    return command_response_to_esp_err(dce_wrap->dce->set_preferred_mode(mode));
}

extern "C" esp_err_t esp_modem_set_network_bands(esp_modem_dce_t *dce_wrap, const char *mode, const int *bands, int size)
{
    if (dce_wrap == nullptr || dce_wrap->dce == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }
    std::string mode_str(mode);
    return command_response_to_esp_err(dce_wrap->dce->set_network_bands(mode, bands, size));
}

extern "C" esp_err_t esp_modem_get_network_system_mode(esp_modem_dce_t *dce_wrap, int *p_mode)
{
    if (dce_wrap == nullptr || dce_wrap->dce == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }
    int mode;
    auto ret = command_response_to_esp_err(dce_wrap->dce->get_network_system_mode(mode));
    if (ret == ESP_OK) {
        *p_mode = mode;
    }
    return ret;
}

extern "C" esp_err_t esp_modem_set_gnss_power_mode(esp_modem_dce_t *dce_wrap, int mode)
{
    if (dce_wrap == nullptr || dce_wrap->dce == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }
    return command_response_to_esp_err(dce_wrap->dce->set_gnss_power_mode(mode));
}

extern "C" esp_err_t esp_modem_get_gnss_power_mode(esp_modem_dce_t *dce_wrap, int *p_mode)
{
    if (dce_wrap == nullptr || dce_wrap->dce == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }
    int mode;
    auto ret = command_response_to_esp_err(dce_wrap->dce->get_gnss_power_mode(mode));
    if (ret == ESP_OK) {
        *p_mode = mode;
    }
    return ret;
}

extern "C" esp_err_t esp_modem_set_pdp_context(esp_modem_dce_t *dce_wrap, esp_modem_PdpContext_t *c_api_pdp)
{
    if (dce_wrap == nullptr || dce_wrap->dce == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_modem::PdpContext pdp{c_api_pdp->apn};
    pdp.context_id = c_api_pdp->context_id;
    pdp.protocol_type = c_api_pdp->protocol_type;
    return command_response_to_esp_err(dce_wrap->dce->set_pdp_context(pdp));
}

extern "C" esp_err_t esp_modem_command(esp_modem_dce_t *dce_wrap, const char *command, esp_err_t(*got_line_fn)(uint8_t *data, size_t len), uint32_t timeout_ms)
{
    if (dce_wrap == nullptr || dce_wrap->dce == nullptr || command == nullptr || got_line_fn == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }
    std::string cmd(command);
    return command_response_to_esp_err(dce_wrap->dce->command(cmd, [got_line_fn](uint8_t *data, size_t len) {
        switch (got_line_fn(data, len)) {
        case ESP_OK:
            return command_result::OK;
        case ESP_FAIL:
            return command_result::FAIL;
        default:
            return command_result::TIMEOUT;
        }
    }, timeout_ms));
}

extern "C" esp_err_t esp_modem_set_baud(esp_modem_dce_t *dce_wrap, int baud)
{
    return command_response_to_esp_err(dce_wrap->dce->set_baud(baud));
}

/**
 * @brief 初始化pppos 配置
 * @param gpio_tx uart tx gpio num
 * @param gpio_rx uart rx gpio num
 * @param gpio_pw lte模块电源控制 gpio num
 * @return 成功返回 pppos netif_t* ,失败返回 NULL
*/
extern "C" esp_netif_t* esp_netif_create_pppos_default(uint8_t gpio_tx, uint8_t gpio_rx, uint8_t gpio_pw){
    esp_modem_dte_config_t dte_config   = ESP_MODEM_DTE_DEFAULT_CONFIG();
    /* setup UART specific configuration based on kconfig options */

    dte_config.uart_config.tx_io_num = gpio_tx;//13;  // 25;//CONFIG_EXAMPLE_MODEM_UART_TX_PIN;
    dte_config.uart_config.rx_io_num = gpio_rx;//34;  // 26;//CONFIG_EXAMPLE_MODEM_UART_RX_PIN;
    dte_config.uart_config.rts_io_num = -1; //CONFIG_EXAMPLE_MODEM_UART_RTS_PIN;
    dte_config.uart_config.cts_io_num = -1; //CONFIG_EXAMPLE_MODEM_UART_CTS_PIN;

    esp_modem_dce_config_t dce_config   = ESP_MODEM_DCE_DEFAULT_CONFIG("", gpio_pw/*2*/, 27);
    esp_netif_config_t      pppos_config = ESP_NETIF_DEFAULT_PPP();
    esp_netif_t            *pppos_netif = esp_netif_new(&pppos_config);

    // esp_modem_dce_t        *dce   = NULL;
    esp_modem_new(&dte_config, &dce_config, pppos_netif);

    return (dce_wrap == nullptr)?nullptr:pppos_netif;
}
