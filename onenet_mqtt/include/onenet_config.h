/**************************************************************
** 文件名: onenet_config.h
** 说  明: OneNET MQTT 连接参数与Topic配置
**************************************************************/

#ifndef ONENET_CONFIG_H
#define ONENET_CONFIG_H

/* ======================== Broker 连接配置 ======================== */
#define MQTT_BROKER_HOST        "mqtts.heclouds.com"
#define MQTT_BROKER_PORT        1883
#define MQTT_KEEPALIVE          60      /* 心跳间隔(秒) */

/* ======================== 设备凭证 ======================== */
#define MQTT_CLIENT_ID          "smartdap"
#define MQTT_USERNAME           "X9Dcio5cI0"
#define MQTT_PASSWORD           \
"version=2018-10-31&res=products%2FX9Dcio5cI0%2Fdevices%2Fsmartdap&et=1872999428&method=md5&sign=9O5UvQMNFFQ6bPDtn3iHcA%3D%3D"

/* ======================== 产品信息 ======================== */
#define PRODUCT_ID              "X9Dcio5cI0"
#define DEVICE_NAME             "smartdap"

/* ======================== Topic 模板 ======================== */
/* 属性上报 */
#define TOPIC_PROPERTY_POST         "$sys/%s/%s/thing/property/post"
/* 属性上报回复 */
#define TOPIC_PROPERTY_POST_REPLY   "$sys/%s/%s/thing/property/post/reply"
/* 事件上报 */
#define TOPIC_EVENT_POST            "$sys/%s/%s/thing/event/post"
/* 事件上报回复 */
#define TOPIC_EVENT_POST_REPLY      "$sys/%s/%s/thing/event/post/reply"
/* 平台设置属性(设备订阅) */
#define TOPIC_PROPERTY_SET          "$sys/%s/%s/thing/property/set"
/* 设备回复设置结果(设备发布) */
#define TOPIC_PROPERTY_SET_REPLY    "$sys/%s/%s/thing/property/set_reply"
/* 获取期望值(设备发布) */
#define TOPIC_PROPERTY_DESIRED_GET       "$sys/%s/%s/thing/property/desired/get"
/* 获取期望值回复(设备订阅) */
#define TOPIC_PROPERTY_DESIRED_GET_REPLY "$sys/%s/%s/thing/property/desired/get/reply"

/* Topic 最大长度 */
#define TOPIC_MAX_LEN           128

/* ======================== 重连配置 ======================== */
#define RECONNECT_INTERVAL      5       /* 初始重连间隔(秒) */
#define RECONNECT_MAX           60      /* 最大重连间隔(秒) */

/* ======================== 上报周期 ======================== */
#define PROPERTY_REPORT_INTERVAL  10    /* 属性上报间隔(秒) */

#endif /* ONENET_CONFIG_H */
