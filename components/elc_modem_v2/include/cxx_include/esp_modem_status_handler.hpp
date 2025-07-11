#include <iostream>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_modem_primitives.hpp"
#include "esp_modem_mqtt_client.hpp"
#include "ping/ping_sock.h"
#include "lwip/inet.h"
#include "lwip/netdb.h"

static const char* TAG_STATUS = "StatusHandler";

using namespace esp_modem;

class HeartBeat{
private:
    HeartBeat(std::string h): host(h)
    {
        ip_addr_t target_addr;
        memset(&target_addr, 0, sizeof(target_addr));

        esp_ping_config_t cfg = ESP_PING_DEFAULT_CONFIG();
        struct addrinfo hint;
        struct addrinfo *res = NULL;
        memset(&hint, 0, sizeof(hint));
        /* convert ip4 string or hostname to ip4 or ip6 address */
        if (getaddrinfo("8.8.8.8", NULL, &hint, &res) != 0) {
            // printf("ping: unknown host %s\n", host.c_str());
            ESP_LOGE(TAG_STATUS, "-------- %s ------- %d ---------ping: unknown host 8.8.8.8", __FUNCTION__, __LINE__);
            return;
        }

        struct in_addr addr4 = ((struct sockaddr_in *) (res->ai_addr))->sin_addr;
        inet_addr_to_ip4addr(ip_2_ip4(&target_addr), &addr4);

        freeaddrinfo(res);
    
        cfg.target_addr = target_addr;
        cfg.count = 1;

        esp_ping_callbacks_t cbs;    
        cbs.on_ping_success = HeartBeat::cmd_ping_on_ping_success;
        cbs.on_ping_timeout = HeartBeat::cmd_ping_on_ping_timeout;
        cbs.on_ping_end     = HeartBeat::cmd_ping_on_ping_end;
        cbs.cb_args         = nullptr;

        esp_ping_new_session(&cfg, &cbs, &handle);
    }

    ~HeartBeat() {
    }
public:
    void send_icmp(){
        // if (!HeartBeat::p_heart_beat) return;
        if (!HeartBeat::running) {
            // ESP_LOGI(TAG_STATUS, "-------- %s ------- %d ---------", __FUNCTION__, __LINE__);
            HeartBeat::running = true;
            esp_ping_start(handle);
        }
    }

    static HeartBeat* get_instance(std::string host)
    {
        if (!HeartBeat::p_heart_beat) {
            HeartBeat::p_heart_beat = new HeartBeat(host);
        }

        return HeartBeat::p_heart_beat;
    }

public:
    static void cmd_ping_on_ping_success(esp_ping_handle_t hdl, void *args)
    {
        // ESP_LOGI(TAG_STATUS, "-------- %s ------- %d ---------", __FUNCTION__, __LINE__);

        // uint8_t ttl;
        // uint16_t seqno;
        // uint32_t elapsed_time, recv_len;
        // ip_addr_t target_addr;
        // esp_ping_get_profile(hdl, ESP_PING_PROF_SEQNO, &seqno, sizeof(seqno));
        // esp_ping_get_profile(hdl, ESP_PING_PROF_TTL, &ttl, sizeof(ttl));
        // esp_ping_get_profile(hdl, ESP_PING_PROF_IPADDR, &target_addr, sizeof(target_addr));
        // esp_ping_get_profile(hdl, ESP_PING_PROF_SIZE, &recv_len, sizeof(recv_len));
        // esp_ping_get_profile(hdl, ESP_PING_PROF_TIMEGAP, &elapsed_time, sizeof(elapsed_time));
        // printf("%d bytes from %s icmp_seq=%d ttl=%d time=%d ms\n",
        //     recv_len, ipaddr_ntoa((ip_addr_t*)&target_addr), seqno, ttl, elapsed_time);
    }

    static void cmd_ping_on_ping_timeout(esp_ping_handle_t hdl, void *args)
    {
        ESP_LOGI(TAG_STATUS, "-------- %s ------- %d ---------", __FUNCTION__, __LINE__);
        // uint16_t seqno;
        // ip_addr_t target_addr;
        // esp_ping_get_profile(hdl, ESP_PING_PROF_SEQNO, &seqno, sizeof(seqno));
        // esp_ping_get_profile(hdl, ESP_PING_PROF_IPADDR, &target_addr, sizeof(target_addr));
        // printf("From %s icmp_seq=%d timeout\n",ipaddr_ntoa((ip_addr_t*)&target_addr), seqno);
    }

    static void cmd_ping_on_ping_end(esp_ping_handle_t hdl, void *args)
    {
        // ESP_LOGI(TAG_STATUS, "-------- %s ------- %d ---------", __FUNCTION__, __LINE__);

        // ip_addr_t target_addr;
        // uint32_t transmitted;
        // uint32_t received;
        // uint32_t total_time_ms;
        // esp_ping_get_profile(hdl, ESP_PING_PROF_REQUEST, &transmitted, sizeof(transmitted));
        // esp_ping_get_profile(hdl, ESP_PING_PROF_REPLY, &received, sizeof(received));
        // esp_ping_get_profile(hdl, ESP_PING_PROF_IPADDR, &target_addr, sizeof(target_addr));
        // esp_ping_get_profile(hdl, ESP_PING_PROF_DURATION, &total_time_ms, sizeof(total_time_ms));
        // uint32_t loss = (uint32_t)((1 - ((float)received) / transmitted) * 100);
        // if (IP_IS_V4(&target_addr)) {
        //     printf("\n--- %s ping statistics ---\n", inet_ntoa(*ip_2_ip4(&target_addr)));
        // } else {
        //     printf("\n--- %s ping statistics ---\n", inet6_ntoa(*ip_2_ip6(&target_addr)));
        // }
        // printf("%d packets transmitted, %d received, %d%% packet loss, time %dms\n",
        //     transmitted, received, loss, total_time_ms);
        // // delete the ping sessions, so that we clean up all resources and can create a new ping session
        // // we don't have to call delete function in the callback, instead we can call delete function from other tasks
        // esp_ping_delete_session(hdl);
        HeartBeat::running = false;
    }
private:
    static bool running;
    std::string host;    
    esp_ping_handle_t handle;
    static HeartBeat* p_heart_beat;
};

HeartBeat* HeartBeat::p_heart_beat = nullptr;
bool HeartBeat::running = false;


using pppos_entry_t = void(*)(void);

class StatusHandler {
public:
    static constexpr auto IP_Event      = SignalGroup::bit0;
    static constexpr auto MQTT_Connect  = SignalGroup::bit1;
    static constexpr auto MQTT_Data     = SignalGroup::bit2;

    StatusHandler(pppos_entry_t f1 = nullptr) : s_retry_num(0), start_pppos(f1)
    {
        ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, on_event, this)); 
        ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, on_event, this)); 
        // ESP_ERROR_CHECK(esp_event_handler_register(ESP_MODEM_EVENT, ESP_EVENT_ANY_ID, on_event, this));

        heart_beat_handle = HeartBeat::get_instance("bing.com");
        // heart_beat_handle->send_icmp();
    }

    ~StatusHandler()
    {
        esp_event_handler_unregister(IP_EVENT, ESP_EVENT_ANY_ID, on_event);
        esp_event_handler_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, on_event);
    }

    void handle_mqtt(MqttClient *client)
    {
        mqtt_client = client;
        client->register_handler(ESP_EVENT_ANY_ID, on_event, this);
    }

    esp_err_t wait_for(decltype(IP_Event) event, int milliseconds)
    {
        return signal.wait_any(event, milliseconds);
    }

    ip_event_t get_ip_event_type()
    {
        return ip_event_type;
    }

private:
    static void on_event(void *arg, esp_event_base_t base, int32_t event, void *data)
    {
        // ESP_LOGI(TAG_STATUS, " --- line:%d PPP state changed event %d", __LINE__, event);

        auto *handler = static_cast<StatusHandler *>(arg);

        if (base == IP_EVENT) {
            handler->ip_event(event, data);
        } else if (base == WIFI_EVENT) {
            handler->wifi_event(event, data);
        } else {
            handler->mqtt_event(event, data);
        }

    }

    void ip_event(int32_t id, void *data)
    {
        // ESP_LOGI(TAG_STATUS, " --- line:%d PPP state changed event %d", __LINE__, id);
        
        ip_event_type = static_cast<ip_event_t>(id);

        if (id == IP_EVENT_PPP_GOT_IP || id == IP_EVENT_STA_GOT_IP) {
            auto *event = (ip_event_got_ip_t *)data;
            ESP_LOGI(TAG_STATUS, "IP          : " IPSTR, IP2STR(&event->ip_info.ip));
            ESP_LOGI(TAG_STATUS, "Netmask     : " IPSTR, IP2STR(&event->ip_info.netmask));
            ESP_LOGI(TAG_STATUS, "Gateway     : " IPSTR, IP2STR(&event->ip_info.gw));
            signal.set(IP_Event);
        } else if (id == IP_EVENT_PPP_LOST_IP) {
            signal.set(IP_Event);
        }

        // ip_event_type = static_cast<ip_event_t>(id);
    }

    void wifi_event(int32_t id, void *data) {
        
        auto *event = (ip_event_got_ip_t *)data;
        ip_event_type = static_cast<ip_event_t>(id);

        if (id == WIFI_EVENT_STA_START) {
            // ESP_LOGI(TAG_STATUS, "--------%s ------ %d ----.", __FUNCTION__, __LINE__);
            esp_wifi_connect();
        } else if (id == WIFI_EVENT_STA_CONNECTED) {
            ESP_LOGI(TAG_STATUS, "--------%s ------ %d - wifi connected---.", __FUNCTION__, __LINE__); 
            s_retry_num = 0;
            signal.set(IP_Event);
            // ESP_LOGI(TAG_STATUS, "--------%s ------ %d ----.", __FUNCTION__, __LINE__); 
        } else if (id == WIFI_EVENT_STA_DISCONNECTED) {
            // ESP_LOGI(TAG_STATUS, "--------%s ------ %d ----.", __FUNCTION__, __LINE__);
            ESP_LOGI(TAG_STATUS, "----------------- wifi event sta disconnected.");
            esp_wifi_connect();
            s_retry_num ++;
            if (s_retry_num == 10) {
                ESP_LOGI(TAG_STATUS, "--------%s ------ %d ---- 连接wifi 超时，启动 pppos.", __FUNCTION__, __LINE__); 
                //启动 lte pppos
                start_pppos();
            }
        } else if (id == IP_EVENT_STA_GOT_IP) {
            ESP_LOGI(TAG_STATUS, "--------%s ------ %d ---- wifi got ip.", __FUNCTION__, __LINE__); 
            ESP_LOGI(TAG_STATUS, "IP          : " IPSTR, IP2STR(&event->ip_info.ip));
            signal.set(IP_Event);
        }
    }

    void mqtt_event(int32_t event, void *data)
    {
        if (mqtt_client && event == mqtt_client->get_event(MqttClient::Event::CONNECT)) {
            signal.set(MQTT_Connect);
        } else if (mqtt_client && event == mqtt_client->get_event(MqttClient::Event::DATA)) {
            // ESP_LOGI(TAG_STATUS, " TOPIC: %s", mqtt_client->get_topic(data).c_str());
            ESP_LOGI(TAG_STATUS, " DATA: %s", mqtt_client->get_data(data).c_str());
            signal.set(MQTT_Data);
        }
    }

    esp_modem::SignalGroup signal{};
    MqttClient *mqtt_client{nullptr};
    ip_event_t ip_event_type;
    int s_retry_num;
    pppos_entry_t start_pppos;
public:
    HeartBeat* heart_beat_handle;
};