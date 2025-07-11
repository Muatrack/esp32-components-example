/*
 * SPDX-FileCopyrightText: 2021-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <charconv>
#include <list>
#include "esp_log.h"
#include "cxx_include/esp_modem_dte.hpp"
#include "cxx_include/esp_modem_dce_module.hpp"
#include "cxx_include/esp_modem_command_library.hpp"
#include "cxx_include/esp_modem_command_library_utils.hpp"
#include "string.h"

namespace esp_modem::dce_commands {

static const char *TAG = "command_lib";

static command_result generic_command(CommandableIf *t, const std::string &command,
                                      const std::list<std::string_view> &pass_phrase,
                                      const std::list<std::string_view> &fail_phrase,
                                      uint32_t timeout_ms)
{
    // //ESP_LOGI(TAG, "%s command %s\n", __func__, command.c_str());
    // ESP_LOGI(TAG, "----------- %s %d, cmd: [%s]", __func__, __LINE__, command.c_str() );
    return t->command(command, [&](uint8_t *data, size_t len) {
        std::string_view response((char *)data, len);
        if (data == nullptr || len == 0 || response.empty()) {
            return command_result::TIMEOUT;
        }
        // ESP_LOGI(TAG, "Response: %.*s\n", (int)response.length(), response.data());
        for (auto &it : pass_phrase)
            if (response.find(it) != std::string::npos) {
                return command_result::OK;
            }
        for (auto &it : fail_phrase)
            if (response.find(it) != std::string::npos) {
                return command_result::FAIL;
            }
        return command_result::TIMEOUT;
    }, timeout_ms);

}

static command_result wait_ready(CommandableIf *t,
                                      const std::list<std::string_view> &pass_phrase,
                                      const std::list<std::string_view> &fail_phrase,
                                      uint32_t timeout_ms)
{
    //ESP_LOGI(TAG, "----------- %s %d", __func__, __LINE__);
    return t->wait_ready([&](uint8_t *data, size_t len) {
        // ESP_LOGI("WAIT_READY", "line:%d Response %s", __LINE__, (char*)data);
        std::string_view response((char *)data, len);
        if (data == nullptr || len == 0 || response.empty()) {
            return command_result::TIMEOUT;
        }
        // ESP_LOGI("WAIT_READY", "line:%d Response: %.*s\n",  __LINE__, (int)response.length(), response.data());
        for (auto &it : pass_phrase)
            if (response.find(it) != std::string::npos) {
                return command_result::OK;
            }
        for (auto &it : fail_phrase)
            if (response.find(it) != std::string::npos) {
                return command_result::FAIL;
            }
        return command_result::TIMEOUT;
    }, timeout_ms);

}

static command_result get_apn_ip_addr(CommandableIf *t, const std::string &command, std::string &ip, std::string &apn, uint32_t timeout_ms = 500)
{
    return t->command(command, [&](uint8_t *data, size_t len) {
        size_t pos = 0, pos_1 = 0;

        std::string_view response((char *)data, len);
        while ((pos = response.find('\n')) != std::string::npos) {

            std::string_view token = response.substr(0, pos);
            for (auto it = token.end() - 1; it > token.begin(); it--) // strip trailing CR or LF
                if (*it == '\r' || *it == '\n') {
                    token.remove_suffix(1);
                }

            if ( (pos_1 = token.find("+CGDCONT: 1")) != std::string::npos) {
                token = token.substr(pos_1, -1);

                //找到包含 +CGDCONT：字串的行。在当前行中继续读取逗号分隔的字串，提取第三个作为apn, 第四个作为 ip返回

                //查找第一个逗号,失败
                if ( (pos_1 = token.find(',')) == std::string::npos ) {
                    continue;
                }

                //查找第二个逗号,失败
                token = token.substr(pos_1 + 1, -1);
                if ( (pos_1 = token.find(',')) == std::string::npos ) {
                    continue;
                }

                //查找第三个逗号,失败
                token = token.substr(pos_1 + 1, -1);
                if ( (pos_1 = token.find(',')) == std::string::npos ) {
                    continue;
                }

                std::string_view apn_view = token.substr(0, pos_1);
                apn = apn_view.data();
                apn = apn_view.substr(1, static_cast<int>(apn_view.size()) - 2);

                //查找第四个逗号,失败
                token = token.substr(pos_1 + 1, -1);
                if ( (pos_1 = token.find(',')) == std::string::npos ) {
                    continue;
                }
                std::string_view ip_view = token.substr(0, pos_1);
                ip = ip_view.data();
                ip = ip.substr(1, static_cast<int>(ip_view.size()) - 2);

                // ESP_LOGI(TAG, " --- line:%d   ip Token: %.*s    apn Token: %.*s ", __LINE__, static_cast<int>(ip_view.size()), ip_view.data(), static_cast<int>(apn_view.size()), apn_view.data());
                // ESP_LOGI(TAG, " --- line:%d   ip Token: %s    apn Token: %s ", __LINE__, ip.c_str(), apn.c_str());

                return command_result::OK;
            }

            response = response.substr(pos + 1);
        }

        return command_result::TIMEOUT;
    }, timeout_ms);
}


command_result generic_command(CommandableIf *t, const std::string &command,
                               const std::string &pass_phrase,
                               const std::string &fail_phrase, 
                               uint32_t timeout_ms)
{
    // ESP_LOGI(TAG, "----------- %s %d, cmd: [%s]", __func__, __LINE__, command.c_str() );
    const auto pass = std::list<std::string_view>({pass_phrase});
    const auto fail = std::list<std::string_view>({fail_phrase});
    return generic_command(t, command, pass, fail, timeout_ms);
}

static command_result generic_get_string(CommandableIf *t, const std::string &command, std::string_view &output, uint32_t timeout_ms = 500)
{
    // ESP_LOGI(TAG, "----------- %s.%d, cmd:[ %s ]", __func__, __LINE__, command.c_str());
    return t->command(command, [&](uint8_t *data, size_t len) {
        size_t pos = 0;
        std::string_view response((char *)data, len);
        while ((pos = response.find('\n')) != std::string::npos) {
            std::string_view token = response.substr(0, pos);
            for (auto it = token.end() - 1; it > token.begin(); it--) // strip trailing CR or LF
                if (*it == '\r' || *it == '\n') {
                    token.remove_suffix(1);
                }
                // ESP_LOGI(TAG, "Token: {%.*s}\n", static_cast<int>(token.size()), token.data());

            if (token.find("OK") != std::string::npos) {
                return command_result::OK;
            } else if (token.find("ERROR") != std::string::npos) {
                return command_result::FAIL;
            } else if (token.size() > 2) {
                output = token;
            }
            response = response.substr(pos + 1);
        }
        return command_result::TIMEOUT;
    }, timeout_ms);
}

command_result generic_get_string(CommandableIf *t, const std::string &command, std::string &output, uint32_t timeout_ms)
{
    //ESP_LOGI(TAG, "----------- %s %d", __func__, __LINE__ );
    std::string_view out;
    auto ret = generic_get_string(t, command, out, timeout_ms);
    if (ret == command_result::OK) {
        output = out;
    }
    return ret;
}


command_result generic_command_common(CommandableIf *t, const std::string &command, uint32_t timeout_ms)
{
    // ESP_LOGI(TAG, "----------- %s %d, cmd:%s", __func__, __LINE__, command.c_str() );
    return generic_command(t, command, "OK", "ERROR", timeout_ms);
}

command_result sync(CommandableIf *t)
{
    //ESP_LOGI(TAG, "----------- %s %d", __func__, __LINE__ );
    return generic_command_common(t, "AT\r\n");
}

/* 重启LTE模块 */
void lte_reset(CommandableIf *t)
{
    generic_command_common(t, "AT+RESET\r\n");
}

command_result store_profile(CommandableIf *t)
{
    //ESP_LOGI(TAG, "----------- %s %d", __func__, __LINE__ );
    return generic_command_common(t, "AT&W\r\n");
}

command_result power_down(CommandableIf *t)
{
    //ESP_LOGI(TAG, "----------- %s %d", __func__, __LINE__ );
    return generic_command(t, "AT+QPOWD=1\r\n", "POWERED DOWN", "ERROR", 1000);
}

command_result power_reup(CommandableIf *t)
{
    return command_result::OK;
}

command_result power_down_sim76xx(CommandableIf *t)
{
    //ESP_LOGI(TAG, "----------- %s %d", __func__, __LINE__ );
    return generic_command_common(t, "AT+CPOF\r\n", 1000);
}

command_result power_down_sim70xx(CommandableIf *t)
{
    //ESP_LOGI(TAG, "----------- %s %d", __func__, __LINE__ );
    return generic_command(t, "AT+CPOWD=1\r\n", "POWER DOWN", "ERROR", 1000);
}

command_result power_down_sim8xx(CommandableIf *t)
{
    //ESP_LOGI(TAG, "----------- %s %d", __func__, __LINE__ );
    return generic_command(t, "AT+CPOWD=1\r\n", "POWER DOWN", "ERROR", 1000);
}

command_result reset(CommandableIf *t)
{
    //ESP_LOGI(TAG, "----------- %s %d", __func__, __LINE__ );
    return generic_command(t,  "AT+CRESET\r\n", "PB DONE", "ERROR", 60000);
}

command_result set_baud(CommandableIf *t, int baud)
{
    //ESP_LOGI(TAG, "----------- %s %d", __func__, __LINE__ );
    return generic_command_common(t,  "AT+IPR=" + std::to_string(baud) + "\r\n");
}

command_result hang_up(CommandableIf *t)
{
    //ESP_LOGI(TAG, "----------- %s %d", __func__, __LINE__ );
    return generic_command_common(t, "ATH\r\n", 90000);
}

command_result get_battery_status(CommandableIf *t, int &voltage, int &bcs, int &bcl)
{
    //ESP_LOGI(TAG, "----------- %s %d", __func__, __LINE__ );
    std::string_view out;
    auto ret = generic_get_string(t, "AT+CBC\r\n", out);
    if (ret != command_result::OK) {
        return ret;
    }

    constexpr std::string_view pattern = "+CBC: ";
    if (out.find(pattern) == std::string_view::npos) {
        return command_result::FAIL;
    }
    // Parsing +CBC: <bcs>,<bcl>,<voltage>
    out = out.substr(pattern.size());
    int pos, value, property = 0;
    while ((pos = out.find(',')) != std::string::npos) {
        if (std::from_chars(out.data(), out.data() + pos, value).ec == std::errc::invalid_argument) {
            return command_result::FAIL;
        }
        switch (property++) {
        case 0: bcs = value;
            break;
        case 1: bcl = value;
            break;
        default:
            return command_result::FAIL;
        }
        out = out.substr(pos + 1);
    }
    if (std::from_chars(out.data(), out.data() + out.size(), voltage).ec == std::errc::invalid_argument) {
        return command_result::FAIL;
    }
    return command_result::OK;
}

command_result get_battery_status_sim7xxx(CommandableIf *t, int &voltage, int &bcs, int &bcl)
{
    //ESP_LOGI(TAG, "----------- %s %d", __func__, __LINE__ );
    std::string_view out;
    auto ret = generic_get_string(t, "AT+CBC\r\n", out);
    if (ret != command_result::OK) {
        return ret;
    }
    // Parsing +CBC: <voltage in Volts> V
    constexpr std::string_view pattern = "+CBC: ";
    constexpr int num_pos = pattern.size();
    int dot_pos;
    if (out.find(pattern) == std::string::npos ||
            (dot_pos = out.find('.')) == std::string::npos) {
        return command_result::FAIL;
    }

    int volt, fraction;
    if (std::from_chars(out.data() + num_pos, out.data() + dot_pos, volt).ec == std::errc::invalid_argument) {
        return command_result::FAIL;
    }
    if (std::from_chars(out.data() + dot_pos + 1, out.data() + out.size() - 1, fraction).ec == std::errc::invalid_argument) {
        return command_result::FAIL;
    }
    bcl = bcs = -1; // not available for these models
    voltage = 1000 * volt + fraction;
    return command_result::OK;
}

command_result set_flow_control(CommandableIf *t, int dce_flow, int dte_flow)
{
    //ESP_LOGI(TAG, "----------- %s %d", __func__, __LINE__ );
    return generic_command_common(t, "AT+IFC=" + std::to_string(dce_flow) + "," + std::to_string(dte_flow) + "\r\n");
}

command_result wait_lte_ready(CommandableIf *t, int timeout_ms = 5000)
{
    //ESP_LOGI(TAG, "----------- %s %d", __func__, __LINE__ );
    auto ret = wait_ready(t, std::list<std::string_view>({"+E_UTRAN Service"}), std::list<std::string_view>({"---"}), timeout_ms);
    set_echo(t, false);
    return ret;
}

command_result get_operator_name(CommandableIf *t, std::string &operator_name, int &act)
{
    //ESP_LOGI(TAG, "----------- %s %d", __func__, __LINE__ );
    std::string_view out;
    auto ret = generic_get_string(t, "AT+COPS?\r\n", out, 75000);
    if (ret != command_result::OK) {
        return ret;
    }
    auto pos = out.find("+COPS");
    auto property = 0;
    while (pos != std::string::npos) {
        // Looking for: +COPS: <mode>[, <format>[, <oper>[, <act>]]]
        if (property++ == 2) {  // operator name is after second comma (as a 3rd property of COPS string)
            operator_name = out.substr(++pos);
            auto additional_comma = operator_name.find(',');    // check for the optional ACT
            if (additional_comma != std::string::npos && std::from_chars(operator_name.data() + additional_comma + 1, operator_name.data() + operator_name.length(), act).ec != std::errc::invalid_argument) {
                operator_name = operator_name.substr(0, additional_comma);
            }
            // and strip quotes if present
            auto quote1 = operator_name.find('"');
            auto quote2 = operator_name.rfind('"');
            if (quote1 != std::string::npos && quote2 != std::string::npos) {
                operator_name = operator_name.substr(quote1 + 1, quote2 - 1);
            }
            return command_result::OK;
        }
        pos = out.find(',', ++pos);
    }
    return command_result::FAIL;
}

command_result set_echo(CommandableIf *t, bool on)
{
    //ESP_LOGI(TAG, "----------- %s %d", __func__, __LINE__ );
    if (on) {
        return generic_command_common(t, "ATE1\r\n");
    }
    return generic_command_common(t, "ATE0\r\n");
}

command_result set_pdp_context(CommandableIf *t, PdpContext &pdp)
{
    //ESP_LOGI(TAG, "----------- %s %d", __func__, __LINE__ );
    std::string pdp_command = "AT+CGDCONT=" + std::to_string(pdp.context_id) +
                              ",\"" + pdp.protocol_type + "\",\"" + pdp.apn + "\"\r\n";
    return generic_command_common(t, pdp_command, 150000);
}

command_result set_data_mode(CommandableIf *t)
{
    //ESP_LOGI(TAG, "----------- %s %d", __func__, __LINE__ );
    return generic_command(t, "ATD*99##\r\n", "CONNECT", "ERROR", 5000);
}

command_result get_lte_network_location( CommandableIf *t, std::string &lati, std::string &longi )
{
    int longiPos = 0;
    int latiPos  = 0;
    int endPos  = 0;
    std::string strOut;
    std::string_view out;
    std::string_view tmpStr;
    std::string_view pattern = "+CIPGSMLOC: ";

#if 0
    if ( 0 )
        goto excp;
#else
    generic_command_common(t, "AT+SAPBR=3,1,\"CONTYPE\",\"GPRS\"\r\n", 1000);
    generic_command_common(t, "AT+SAPBR=3,1,\"APN\",\"\"\r\n", 1000);
    generic_command_common(t, "AT+SAPBR=1,1\r\n", 1000);
    generic_command_common( t, "AT+SAPBR=2,1\r\n");
    // demo strign: {+CIPGSMLOC: 0,31.254763,120.468111,2024/05/25,22:40:31}
    if( generic_get_string( t, "AT+CIPGSMLOC=1,1\r\n", out ) != command_result::OK ) {  // 查询位置和时间 
        goto excp;
    }
    // ESP_LOGW(TAG, "------------------- %s.%d", __func__, __LINE__);
    if( (out.find(pattern)) != std::string::npos ) {
        // ESP_LOGW(TAG, "------------------- %s.%d, token:%s", __func__, __LINE__, out.data());
        /* 查找第一个 , */
        if( (latiPos = out.find(",")) != std::string::npos ) {
            tmpStr = out.substr(latiPos + 1);
            endPos = tmpStr.find(",");
            lati =  tmpStr.substr(0, endPos);
            
            longiPos = endPos;
            tmpStr   = tmpStr.substr(longiPos + 1);
            endPos   = tmpStr.find(",");
            longi    = tmpStr.substr(0, endPos);

            ESP_LOGW(TAG, "--------- longi size:%d val:%s, lati size:%d val:%s", longi.size(), longi.c_str(), lati.size(), lati.c_str());
        }
    }
#endif
    // ESP_LOGI( TAG, "longitude:%s, latitude:%s", longi.data(), lati.data());
    generic_command_common( t, "AT+SAPBR=0,1\r\n");
    return command_result::OK;
excp:
    return command_result::FAIL;
}

/** 读取网络注册状态 */
command_result get_lte_network_state(CommandableIf *t, int &a, int &b)
{
    char *pStr = NULL;
    char *netSt = NULL;
    a = 100;
    b = 200;

    /* +CREG: 0,0*/
    int dot_pos;
    int volt, fraction;
    std::string_view out;
    std::string retStr = std::string(out);
    constexpr std::string_view pattern = "+CEREG: ";
    constexpr int num_pos = pattern.size();
    
    if( generic_get_string( t, "AT+CEREG?\r\n", out ) != command_result::OK ) {
        goto excp;
    }
    if (out.find(pattern) == std::string::npos ||
            (dot_pos = out.find(',')) == std::string::npos) {
        return command_result::FAIL;
    }
    if (std::from_chars(out.data() + num_pos, out.data() + dot_pos, a).ec == std::errc::invalid_argument) {
        return command_result::FAIL;
    }
    if (std::from_chars(out.data() + dot_pos + 1, out.data() + dot_pos + 2, b).ec == std::errc::invalid_argument) {
        return command_result::FAIL;
    }
    return command_result::OK;
excp:
    return command_result::FAIL;
}

command_result set_data_mode_air780e(CommandableIf *t)
{
    //ESP_LOGI(TAG, "----------- %s %d", __func__, __LINE__ );    
    generic_command(t, "ATH\r\n", "OK", "ERROR", 5000);
    return generic_command(t, "ATD*99#\r\n", "CONNECT", "ERROR", 5000);
}

command_result set_data_mode_sim8xx(CommandableIf *t)
{
    //ESP_LOGI(TAG, "----------- %s %d", __func__, __LINE__ );
    return generic_command(t, "ATD*99#\r\n", "CONNECT", "ERROR", 5000);
}

command_result resume_data_mode(CommandableIf *t)
{
    //ESP_LOGI(TAG, "----------- %s %d", __func__, __LINE__ );
    return generic_command(t, "ATO\r\n", "CONNECT", "ERROR", 5000);
}

command_result set_command_mode(CommandableIf *t)
{
    //ESP_LOGI(TAG, "----------- %s %d", __func__, __LINE__ );
    const auto pass = std::list<std::string_view>({"NO CARRIER", "OK"});
    const auto fail = std::list<std::string_view>({"ERROR"});
    return generic_command(t, "+++", pass, fail, 5000);
}

command_result get_imsi(CommandableIf *t, std::string &imsi_number)
{
    //ESP_LOGI(TAG, "----------- %s %d", __func__, __LINE__ );
    return generic_get_string(t, "AT+CIMI\r\n", imsi_number, 5000);
}

command_result get_imei(CommandableIf *t, std::string &out)
{
    //ESP_LOGI(TAG, "----------- %s %d", __func__, __LINE__ );
    return generic_get_string(t, "AT+CGSN\r\n", out, 5000);
}

command_result get_module_ver(CommandableIf *t, std::string &out)
{
    std::string out_str;
    //ESP_LOGI(TAG, "----------- %s %d", __func__, __LINE__ );
    auto ret = generic_get_string(t, "AT+CGMR\r\n", out_str, 5000);
    if (ret != command_result::OK) {
        return ret;
    }

    int pos = -1;
    std::string_view pattern = "+CGMR: ";
    if (pos = out_str.find(pattern), pos == std::string::npos) {
        return command_result::FAIL;
    }

    // //ESP_LOGI(TAG, "----------- %s %d, pos:%d", __func__, __LINE__, pos );
    out = out_str.substr(pos + 7);
    out = out.substr(1,  static_cast<int>(out.size() - 2));
    return command_result::OK;
}

command_result get_module_name(CommandableIf *t, std::string &out)
{
    std::string out_str;

    //ESP_LOGI(TAG, "----------- %s %d", __func__, __LINE__ );
    auto ret = generic_get_string(t, "AT+CGMM\r\n", out_str, 5000);
    if (ret != command_result::OK) {
        return ret;
    }

    int pos = -1;
    std::string_view pattern = "+CGMM: ";
    if (pos = out_str.find(pattern), pos == std::string::npos) {
        return command_result::FAIL;
    }

    // //ESP_LOGI(TAG, "----------- %s %d, pos:%d", __func__, __LINE__, pos );
    out = out_str.substr(pos + 7);
    return command_result::OK;
}

command_result sms_txt_mode(CommandableIf *t, bool txt = true)
{
    //ESP_LOGI(TAG, "----------- %s %d", __func__, __LINE__ );
    if (txt) {
        return generic_command_common(t, "AT+CMGF=1\r\n");    // Text mode (default)
    }
    return generic_command_common(t, "AT+CMGF=0\r\n");     // PDU mode
}

command_result sms_character_set(CommandableIf *t)
{
    // Sets the default GSM character set
    //ESP_LOGI(TAG, "----------- %s %d", __func__, __LINE__ );
    return generic_command_common(t, "AT+CSCS=\"GSM\"\r\n");
}

command_result send_sms(CommandableIf *t, const std::string &number, const std::string &message)
{
    //ESP_LOGI(TAG, "----------- %s %d", __func__, __LINE__ );
    auto ret = t->command("AT+CMGS=\"" + number + "\"\r\n", [&](uint8_t *data, size_t len) {
        std::string_view response((char *)data, len);
        //ESP_LOGI(TAG, "Send SMS response %.*s", static_cast<int>(response.size()), response.data());
        if (response.find('>') != std::string::npos) {
            return command_result::OK;
        }
        return command_result::TIMEOUT;
    }, 5000, ' ');
    if (ret != command_result::OK) {
        return ret;
    }
    return generic_command_common(t, message + "\x1A", 120000);
}


command_result set_cmux(CommandableIf *t)
{
    //ESP_LOGI(TAG, "----------- %s %d", __func__, __LINE__ );
    generic_command_common(t, "AT\r\n");
    return generic_command_common(t, "AT+CMUX=0\r\n");
}

command_result read_pin(CommandableIf *t, bool &pin_ok)
{
    //ESP_LOGI(TAG, "----------- %s %d", __func__, __LINE__ );
    std::string_view out;
    generic_command_common(t, "AT\r\n");
    auto ret = generic_get_string(t, "AT+CPIN?\r\n", out);
    if (ret != command_result::OK) {
        return ret;
    }
    if (out.find("OK") != std::string::npos) {
        return command_result::FAIL;
    }
    if (out.find("+CPIN:") == std::string::npos) {
        return command_result::FAIL;
    }
    if (out.find("SIM PIN") != std::string::npos || out.find("SIM PUK") != std::string::npos) {
        pin_ok = false;
        return command_result::OK;
    }
    if (out.find("READY") != std::string::npos) {
        pin_ok = true;
        return command_result::OK;
    }
    return command_result::FAIL; // Neither pin-ok, nor waiting for pin/puk -> mark as error
}

command_result set_pin(CommandableIf *t, const std::string &pin)
{
    //ESP_LOGI(TAG, "----------- %s %d", __func__, __LINE__ );
    std::string set_pin_command = "AT+CPIN=" + pin + "\r\n";
    return generic_command_common(t, set_pin_command);
}

command_result at(CommandableIf *t, const std::string &cmd, std::string &out, int timeout = 500)
{
    std::string at_command = cmd + "\r\n";
    return generic_get_string(t, at_command, out, timeout);
}

command_result get_apn_ip_addr(CommandableIf *t, std::string &ip, std::string &apn)
{
    generic_command_common(t, "ATE0\r\n");
    return get_apn_ip_addr(t, "AT+CGDCONT?\r\n", ip, apn);
}

command_result get_signal_quality(CommandableIf *t, int &rssi, int &ber)
{
    // //ESP_LOGI(TAG, "----------- %s %d", __func__, __LINE__ );
    std::string_view out;
    auto ret = generic_get_string(t, "AT+CSQ\r\n", out);
    if (ret != command_result::OK) {
        return ret;
    }

    constexpr std::string_view pattern = "+CSQ: ";
    constexpr int rssi_pos = pattern.size();
    int ber_pos;
    if (out.find(pattern) == std::string::npos ||
            (ber_pos = out.find(',')) == std::string::npos) {
        return command_result::FAIL;
    }

    if (std::from_chars(out.data() + rssi_pos, out.data() + ber_pos, rssi).ec == std::errc::invalid_argument) {
        return command_result::FAIL;
    }
    if (std::from_chars(out.data() + ber_pos + 1, out.data() + out.size(), ber).ec == std::errc::invalid_argument) {
        return command_result::FAIL;
    }
    return command_result::OK;
}

command_result set_operator(CommandableIf *t, int mode, int format, const std::string &oper)
{
    //ESP_LOGI(TAG, "----------- %s %d", __func__, __LINE__ );
    return generic_command_common(t, "AT+COPS=" + std::to_string(mode) + "," + std::to_string(format) + ",\"" + oper + "\"\r\n", 90000);
}

command_result set_network_attachment_state(CommandableIf *t, int state)
{
    //ESP_LOGI(TAG, "----------- %s %d", __func__, __LINE__ );
    return generic_command_common(t, "AT+CGATT=" + std::to_string(state) + "\r\n");
}

command_result get_network_attachment_state(CommandableIf *t, int &state)
{
    //ESP_LOGI(TAG, "----------- %s %d", __func__, __LINE__ );
    std::string_view out;
    auto ret = generic_get_string(t, "AT+CGATT?\r\n", out);
    if (ret != command_result::OK) {
        return ret;
    }
    constexpr std::string_view pattern = "+CGATT: ";
    constexpr int pos = pattern.size();
    if (out.find(pattern) == std::string::npos) {
        return command_result::FAIL;
    }

    if (std::from_chars(out.data() + pos, out.data() + out.size(), state).ec == std::errc::invalid_argument) {
        return command_result::FAIL;
    }

    return command_result::OK;
}

command_result set_radio_state(CommandableIf *t, int state)
{
    //ESP_LOGI(TAG, "----------- %s %d", __func__, __LINE__ );
    return generic_command_common(t, "AT+CFUN=" + std::to_string(state) + "\r\n", 15000);
}

command_result get_radio_state(CommandableIf *t, int &state)
{
    //ESP_LOGI(TAG, "----------- %s %d", __func__, __LINE__ );
    std::string_view out;
    auto ret = generic_get_string(t, "AT+CFUN?\r\n", out);
    if (ret != command_result::OK) {
        return ret;
    }
    constexpr std::string_view pattern = "+CFUN: ";
    constexpr int pos = pattern.size();
    if (out.find(pattern) == std::string::npos) {
        return command_result::FAIL;
    }

    if (std::from_chars(out.data() + pos, out.data() + out.size(), state).ec == std::errc::invalid_argument) {
        return command_result::FAIL;
    }

    return command_result::OK;
}

command_result set_network_mode(CommandableIf *t, int mode)
{
    //ESP_LOGI(TAG, "----------- %s %d", __func__, __LINE__ );
    return generic_command_common(t, "AT+CNMP=" + std::to_string(mode) + "\r\n");
}

command_result set_preferred_mode(CommandableIf *t, int mode)
{
    //ESP_LOGI(TAG, "----------- %s %d", __func__, __LINE__ );
    return generic_command_common(t, "AT+CMNB=" + std::to_string(mode) + "\r\n");
}

command_result set_network_bands(CommandableIf *t, const std::string &mode, const int *bands, int size)
{
    //ESP_LOGI(TAG, "----------- %s %d", __func__, __LINE__ );
    std::string band_string = "";
    for (int i = 0; i < size - 1; ++i) {
        band_string += std::to_string(bands[i]) + ",";
    }
    band_string += std::to_string(bands[size - 1]);

    return generic_command_common(t, "AT+CBANDCFG=\"" + mode + "\"," + band_string + "\r\n");
}

// mode is expected to be 64bit string (in hex)
// any_mode = "0xFFFFFFFF7FFFFFFF";
command_result set_network_bands_sim76xx(CommandableIf *t, const std::string &mode, const int *bands, int size)
{
    //ESP_LOGI(TAG, "----------- %s %d", __func__, __LINE__ );
    static const char *hexDigits = "0123456789ABCDEF";
    uint64_t band_bits = 0;
    int hex_len = 16;
    std::string band_string(hex_len, '0');
    for (int i = 0; i < size; ++i) {
        // OR-operation to add bands
        auto band = bands[i] - 1; // Sim7600 has 0-indexed band selection (band 20 has to be shifted 19 places)
        band_bits |= 1 << band;
    }
    for (int i = hex_len; i > 0; i--) {
        band_string[i - 1] = hexDigits[(band_bits >> ((hex_len - i) * 4)) & 0xF];
    }
    return generic_command_common(t, "AT+CNBP=" + mode + ",0x" + band_string + "\r\n");
}

command_result get_network_system_mode(CommandableIf *t, int &mode)
{
    //ESP_LOGI(TAG, "----------- %s %d", __func__, __LINE__ );
    std::string_view out;
    auto ret = generic_get_string(t, "AT+CNSMOD?\r\n", out);
    if (ret != command_result::OK) {
        return ret;
    }

    constexpr std::string_view pattern = "+CNSMOD: ";
    int mode_pos = out.find(",") + 1; // Skip "<n>,"
    if (out.find(pattern) == std::string::npos) {
        return command_result::FAIL;
    }

    if (std::from_chars(out.data() + mode_pos, out.data() + out.size(), mode).ec == std::errc::invalid_argument) {
        return command_result::FAIL;
    }

    return command_result::OK;
}

command_result set_gnss_power_mode(CommandableIf *t, int mode)
{
    //ESP_LOGI(TAG, "----------- %s %d", __func__, __LINE__ );
    return generic_command_common(t, "AT+CGNSPWR=" + std::to_string(mode) + "\r\n");
}

command_result get_gnss_power_mode(CommandableIf *t, int &mode)
{
    //ESP_LOGI(TAG, "----------- %s %d", __func__, __LINE__ );
    std::string_view out;
    auto ret = generic_get_string(t, "AT+CGNSPWR?\r\n", out);
    if (ret != command_result::OK) {
        return ret;
    }
    constexpr std::string_view pattern = "+CGNSPWR: ";
    constexpr int pos = pattern.size();
    if (out.find(pattern) == std::string::npos) {
        return command_result::FAIL;
    }

    if (std::from_chars(out.data() + pos, out.data() + out.size(), mode).ec == std::errc::invalid_argument) {
        return command_result::FAIL;
    }

    return command_result::OK;
}

command_result set_gnss_power_mode_sim76xx(CommandableIf *t, int mode)
{
    //ESP_LOGI(TAG, "----------- %s %d", __func__, __LINE__ );
    return generic_command_common(t, "AT+CGPS=" + std::to_string(mode) + "\r\n");
}

} // esp_modem::dce_commands
