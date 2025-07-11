
/**
 *  esp32 wifi 操作封装
*/

#include "os_lte_wrapper.h"
#include "string.h"
#include "esp_modem_c_api.h"
#include "esp_err.h"
#include "esp_log.h"
#include "memory.h"

#define TAG "OS_LTE_WRAPPER"

gw_netif_t *_gw_netif = NULL;

static void air724_gw_netif_set( gw_netif_t *pNetIf ) {
    _gw_netif = pNetIf;
}

static gw_netif_t* air724_gw_netif_get()
{
    return _gw_netif;
}

static void air724_ppp_state_set( lte_ppp_status_t st ){
    air724_gw_netif_get()->pppSt = st;
}

void air724_module_info_get(char *pFw, char *pImei, char *pImsi, char *pIp, char *pLbs ) {
    esp_netif_ip_info_t ip4_info;
    gw_netif_t *pNetIf = NULL;

    pNetIf = air724_gw_netif_get();
    if( pNetIf == NULL )
        return;
    strcpy( pFw,   pNetIf->module_info.fw  );
    strcpy( pImei, pNetIf->module_info.imei);
    strcpy( pImsi, pNetIf->module_info.imsi);
    if( pNetIf->pNetIf ) {
        esp_netif_get_ip_info( pNetIf->pNetIf, &ip4_info);
        sprintf( pIp, IPSTR, IP2STR(&ip4_info.ip));
    }
    sprintf( pLbs, "%s,%s", pNetIf->lbs.lati,pNetIf->lbs.longi);
}

/*将PPP拨号状态码转为可读字串,简短*/
static char* air724_ppp_state2str(){
    switch( air724_gw_netif_get()->pppSt ) {
        case    PPP_STAT_INIT:    return "查询USIM";  /*初始状态*/
        case    PPP_SIM_OK:       return "USIM正常";  /*usim 卡存在*/
        case    PPP_SIM_NO:       return "USIM无";  /*usim卡不存在*/
        case    PPP_NET_OK:       return "注网正常";  /*注网成功*/
        case    PPP_NET_NO:       return "注网中";  /*正在注网*/
        case    PPP_GOTIP_OK:     return "已获取IP";  /*联网成功，获取IP*/
        case    PPP_GOTIP_WAIT:   return "等待IP";  /*联网中，等待IP*/
        case    PPP_GOTIP_TIMOUT: return "IP超时";  /*联网失败，未获得IP*/
        default: return "无效码";
    }
}

lte_ppp_status_t air724_ppp_state_get(char *pOut){
    gw_netif_t *pNetIf = NULL;
    pNetIf = air724_gw_netif_get();
    if( pNetIf == NULL )
        return PPP_EXCP;
    if( pOut != NULL ) {
        strcpy(pOut, air724_ppp_state2str());
    }
    return pNetIf->pppSt;
}

/**
 * 在AT指令阶段，读取模块信息并缓存。 供web查询
 * return 全部查询完毕true, false。
*/
bool air724_module_info_fill(){
    
    gw_netif_t *pNetIf = air724_gw_netif_get();
    
    /*判断fw为空, 获取fw*/
    // ESP_LOGI("NET_LTE", "fw len:%d, val:%.*s", strlen(pNetIf->module_info.fw), strlen(pNetIf->module_info.fw), pNetIf->module_info.fw);
    if( strlen(pNetIf->module_info.fw) < 16 ) {
        esp_modem_get_module_ver(pNetIf->module_info.fw);
        goto recheck;
    }

    /**/
    // ESP_LOGI("NET_LTE", "imei len:%d, val:%.*s", strlen(pNetIf->module_info.imei), strlen(pNetIf->module_info.imei), pNetIf->module_info.imei);
    if( strlen(pNetIf->module_info.imei) <= strlen("imei") ) {
        esp_modem_get_imei(pNetIf->module_info.imei);
        goto recheck;
    }
    
    /**/
    // ESP_LOGI("NET_LTE", "imsi len:%d, val:%.*s", strlen(pNetIf->module_info.imsi), strlen(pNetIf->module_info.imsi), pNetIf->module_info.imsi);
    if( strlen(pNetIf->module_info.imsi) <= strlen("imsi") ) {
        esp_modem_get_imsi(pNetIf->module_info.imsi);
        goto recheck;
    }
    
    /*apn信息，当前并未使用。 不考虑获取*/
    /*ip信息， 当前并未执行拨号。获取不到ip*/

    /*所有变量已获取后 返回true*/
    return true;
recheck:
    return false;
}

/**
 * 初始化lte数据缓存区
*/
void air724_module_block_reset(){
    gw_netif_t *pNetIf = air724_gw_netif_get();
    air724_ppp_state_set( PPP_STAT_INIT );
    if( pNetIf ) {
        memset( (void*)&(pNetIf->lbs), 0, sizeof( pNetIf->lbs ));
        memset( (void*)&(pNetIf->module_info), 0, sizeof( pNetIf->module_info ));
    }
}

void air724_task(void *p)
{
    static bool bGotLbs = false; /* 标记是否已经获取 LBS */
    gw_netif_t *pGwIf = (gw_netif_t*)p;    
    uint8_t lteURC = 0; /*注网状态上报主动/被动*/
    uint8_t lteSt  = 0; /*注网状态*/
    /**
     * sim卡检测，连续失败计数-不循环
     * 每次重启模块(15s/次)，变量增1.
     * 当该值>=10.(视为无lte模块). 结束 air724_task
     */
    static  uint8_t terminalCheckExcpCounter = 0; 
    static  uint8_t pinCheckExcpCounter = 0; /*sim卡检测，连续失败计数*/
    static  uint8_t netCheckExcpCounter = 0; /*注网状态检测，连续异常计数*/

    // AIR780E重新上电
    esp_modem_reset();
    /*初始化air724 netif 数据缓冲指针*/
    air724_gw_netif_set( pGwIf );
    /* 初始化模块数据缓存区*/
    air724_module_block_reset();

    if( pGwIf == NULL ) {
        ESP_LOGE("NET_LTE", "%s(), pGwIf is NULL\r\n", __func__);
        vTaskDelete(NULL);
    }
    /* 初始化Ping */
    // network_lte_ping_init( 0, 1, 3, 3 );
    
reconnect:
    ESP_LOGI("NET_LTE", "---- NETWORK SUPPORT.reconnect ----\r\n");
    /* 延时2s, 等待模块初始化完成 */
    vTaskDelay(pdMS_TO_TICKS(2000));    /* 延时2s*/
    /*判断sim卡是否存在*/
    if(esp_modem_read_pin() != ESP_OK) {
        pinCheckExcpCounter += 1;        
        if( pinCheckExcpCounter > 3 ) {
            pinCheckExcpCounter = 0;
            terminalCheckExcpCounter += 1;
            if( terminalCheckExcpCounter >= 10 ) {
                ESP_LOGI("NET_LTE", "task. air724 deleted\r\n");
                vTaskDelete(NULL);
            }
            air724_ppp_state_set( PPP_SIM_NO );
            goto excp;
        }
        vTaskDelay(pdMS_TO_TICKS(3000));   /* 延时 3s + 2s = 5s */
        // ESP_LOGI("NET_LTE", "---------- pinCheckExcpCounter:%d\r\n", pinCheckExcpCounter);
        // ESP_LOGI("NET_LTE", "---- no sim ----\r\n");
        goto reconnect;
    } else {
        air724_ppp_state_set( PPP_SIM_OK );
        pinCheckExcpCounter = 0;
        terminalCheckExcpCounter = 0;
        ESP_LOGI("NET_LTE", "---- sim true\r\n");
    }

reget_module_info:
    /* 获取模块，USIM相关的多条信息。为控制at指令频率，间隔1s获取一条*/
    if( air724_module_info_fill() == false ) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        goto reget_module_info;
    }

re_creg:
    /* 读取注册状态 */
    esp_modem_lte_get_state(&lteURC, &lteSt);
    // ESP_LOGI("NET_LTE", "lte network, lteURC:%d lteST:%d", lteURC, lteSt);
    air724_ppp_state_set( PPP_NET_NO );
    switch ( lteSt ) {
        case 0:
            ESP_LOGI("NET_LTE", "lte network, not registed\r\n");
            break;
        case 1: /* lte network registed*/
            air724_ppp_state_set( PPP_NET_OK );
            goto creg_after;
        case 2:
            ESP_LOGI("NET_LTE", "lte network, searching \r\n");
            break;
        case 3:
            ESP_LOGI("NET_LTE", "lte network, rejected \r\n");
            break;
        default:
            ESP_LOGI("NET_LTE", "lte network st-val: [%d]\r\n", lteSt);
    }
    vTaskDelay( pdMS_TO_TICKS(3000));
    goto re_creg;
creg_after:

    /* 读取基站定位 */
    if( bGotLbs == false ) {
        memset((void*)&(pGwIf->lbs), 0, sizeof(pGwIf->lbs));        
        esp_modem_get_lbs(pGwIf->lbs.longi, pGwIf->lbs.lati);
        if( strlen(pGwIf->lbs.longi) > 0 && strlen(pGwIf->lbs.lati) > 0) {
            bGotLbs = true;
        }
        ESP_LOGI("NET_LTE", "Got lbs: longi:%s, lati:%s\r\n", pGwIf->lbs.longi, pGwIf->lbs.lati);
    }
    air724_ppp_state_set( PPP_GOTIP_WAIT );
    /* 设置拨号 */
    if (esp_modem_start_pppos(30000) != ESP_OK) {
        air724_ppp_state_set( PPP_GOTIP_TIMOUT );
        ESP_LOGI("NET_LTE", "--------- pppos timeout ---------\r\n");
        goto excp;
    } else {
        air724_ppp_state_set( PPP_GOTIP_OK );
    }
    /**
     *  检测lte网络 
     *  while 执行逻辑, 每1分钟执行一组 ping, 一组3次, 间隔1s. 共计3s. 设置超时3s.
     *  一组ping操作, 有3次超时即认为lte已断开. 需要重新发起pppos.
     *  1 while 起始延时30s
     *  2 判断lte 是否获得ip, 未获得ip 延时30s, 重新发起pppos连接. 已获得ip. 执行ping操作监测lte网络.
    */
    while( true ) {
        // network_lte_ping_start();
        // vTaskDelay(pdMS_TO_TICKS(10000));
        // network_lte_ping_stop();
        // vTaskDelay(pdMS_TO_TICKS(3000));
        vTaskDelay( pdMS_TO_TICKS( 5000 ));
        if(esp_modem_read_pin() != ESP_OK) { /* USIM卡被拔出 */
            pinCheckExcpCounter += 1;             
            if( pinCheckExcpCounter > 3 ) {
                pinCheckExcpCounter = 0;
                goto excp;
            }
            ESP_LOGI("NET_LTE", "---------- while.pinCheckExcpCounter:%d\r\n", pinCheckExcpCounter);
            continue;
        } else {
            pinCheckExcpCounter = 0;
        }

        esp_modem_lte_get_state(&lteURC, &lteSt);
        if( lteSt != 1 ) {
            ESP_LOGW("LTE_MODEM", "网络已断开. lteSt:%d", lteSt);
            netCheckExcpCounter += 1;
            if( netCheckExcpCounter > 3 ) {
                netCheckExcpCounter = 0;
                goto excp;
            }
            ESP_LOGI("NET_LTE", "---------- netCheckExcpCounter:%d \r\n", netCheckExcpCounter);
        } else {
            netCheckExcpCounter = 0;
        }
    }
excp:
    ESP_LOGI("NET_LTE", ">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> RESET <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<\r\n");
    esp_modem_reset();
    pinCheckExcpCounter = 0;
    netCheckExcpCounter = 0;
    goto reconnect;
}