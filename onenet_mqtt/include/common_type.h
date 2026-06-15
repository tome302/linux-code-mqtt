/**************************************************************
** 文件名: common_type.h
** 说  明: 公共类型定义(错误码/状态枚举)
**************************************************************/

#ifndef COMMON_TYPE_H
#define COMMON_TYPE_H

#include <stdint.h>
#include <stdbool.h>

/* ======================== 错误码 ======================== */
typedef enum {
    ONENET_OK             =  0,
    ONENET_ERR_PARAM      = -1,   /* 参数错误 */
    ONENET_ERR_CONNECT    = -2,   /* 连接失败 */
    ONENET_ERR_SUBSCRIBE  = -3,   /* 订阅失败 */
    ONENET_ERR_PUBLISH    = -4,   /* 发布失败 */
    ONENET_ERR_MEMORY     = -5,   /* 内存分配失败 */
    ONENET_ERR_MOSQ       = -6,   /* mosquitto底层错误 */
    ONENET_ERR_JSON       = -7,   /* JSON构造/解析错误 */
} onenet_err_t;

/* ======================== MQTT 客户端状态 ======================== */
typedef enum {
    MQTT_STATE_DISCONNECTED = 0,
    MQTT_STATE_CONNECTING,
    MQTT_STATE_CONNECTED,
} mqtt_state_t;

/* ======================== 属性值类型 ======================== */
typedef enum {
    PROP_TYPE_INT32 = 0,
    PROP_TYPE_INT64,
    PROP_TYPE_FLOAT,
    PROP_TYPE_BOOL,
    PROP_TYPE_ENUM,
} prop_type_t;

/* ======================== 属性值联合体 ======================== */
typedef union {
    int32_t  i32;
    int64_t  i64;
    float    f32;
    bool     boolean;
} prop_value_t;

#endif /* COMMON_TYPE_H */
