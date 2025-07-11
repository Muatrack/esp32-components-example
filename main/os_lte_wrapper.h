#ifndef _OS_LTE_WRAPPER_H_
#define _OS_LTE_WRAPPER_H_

#include <stdbool.h>
#include <esp_netif.h>

/*定义模块拨号过程阶段的状态*/
typedef enum {
    PPP_EXCP    =   0,  /*数据结构异常*/
    PPP_STAT_INIT,      /*初始状态*/
    PPP_SIM_OK,         /*usim 卡存在*/
    PPP_SIM_NO,         /*usim卡不存在*/
    PPP_NET_OK,         /*注网成功*/
    PPP_NET_NO,         /*正在注网*/
    PPP_GOTIP_OK,       /*联网成功，获取IP*/
    PPP_GOTIP_WAIT,     /*联网中，等待IP*/
    PPP_GOTIP_TIMOUT,   /*联网失败，未获得IP*/
}lte_ppp_status_t;

typedef struct {
    esp_netif_t *pNetIf;
    /*存储模块信息, 其中部分信息应当考虑flash存储*/
    struct {
        char fw[32];   /*模块固件信息， 型号，版本 .... e.g: AirM2M_Air724UG_V401880_LTE_AT */
        char imei[16]; /*模块识别码（全球唯一) 15字符*/
        char imsi[16]; /*SIM卡识别吗（全球唯一）15字符*/
        char ip[16];   /*IP地址 15字符*/
        char apn[0];   /*保留，暂无用*/
    }module_info;
    /*存储基站定位信息*/
    struct{
        char longi[10];
        char lati[10];
    }lbs;
    lte_ppp_status_t pppSt; /*pppos 拨号过程，状态记录*/
}gw_netif_t;

/* 拨号联网 TASK 入口*/
void air724_task(void *p);

/**查询当前PPP拨号状态
 * param pOut 为将拨号状态码转成可读字串的输出地址,[ 可以为空 ]
 * @return 返回拨号状态码
*/
lte_ppp_status_t air724_ppp_state_get(char *pOut);

/**
 * 为减少CMUX场景下的at执行频率， 
 * 将模块信息存储于内存中.
 * 优先从内存读取模块及usim信息
 * @param pFw lte模组固件信息输出地址， 长度32
 * @param pImei 模组全球id输出地址, 长度15
 * @param pImsi USIM全球id输出地址, 长度15
 */
void air724_module_info_get(char *pFw, char *pImei, char *pImsi, char *pIp, char *pLbs );

#endif