/**************************************************************
** 文件名: onenet_property.h
** 说  明: OneNET 物模型属性 JSON 序列化/反序列化接口
**************************************************************/

#ifndef ONENET_PROPERTY_H
#define ONENET_PROPERTY_H

#include "common_type.h"

/* ======================== 属性标识符 ======================== */
#define PROP_KEY_CSQ                "CSQ"
#define PROP_KEY_BATTERY_PCT        "battery_percentage"
#define PROP_KEY_BATTERY_STATE      "battery_state"
#define PROP_KEY_HOT_VALVE          "hot_valve"
#define PROP_KEY_HUMIDITY           "humidity_value"
#define PROP_KEY_MAXHUM_SET         "maxhum_set"
#define PROP_KEY_MAXTEMP_SET        "maxtemp_set"
#define PROP_KEY_MINIHUM_SET        "minihum_set"
#define PROP_KEY_MINITEMP_SET       "minitemp_set"
#define PROP_KEY_TEMP_UNIT_CONVERT  "temp_unit_convert"
#define PROP_KEY_TEMP_VALUE         "temp_value"

/* ======================== 事件标识符 ======================== */
#define EVENT_KEY_LED               "led"

/* ======================== 初始化 ======================== */

/**************************************************************
** 函数名:   property_init
** 函数说明: 初始化属性模块, 设置默认值
** 参数:    无
** 返回: 无
**************************************************************/
void property_init(void);

/* ======================== 属性读写 ======================== */

/**************************************************************
** 函数名:   property_set_value
** 函数说明: 设置属性值(平台下发时调用)
** 参数:    key - 属性标识符, value - 属性值
** 返回: ONENET_OK 成功, ONENET_ERR_PARAM 未知属性或只读
**************************************************************/
onenet_err_t property_set_value(const char *key, prop_value_t value);

/**************************************************************
** 函数名:   property_get_value
** 函数说明: 获取属性值
** 参数:    key - 属性标识符, value - [out]属性值
** 返回: ONENET_OK 成功, ONENET_ERR_PARAM 未知属性
**************************************************************/
onenet_err_t property_get_value(const char *key, prop_value_t *value);

/* ======================== JSON 构造/解析 ======================== */

/**************************************************************
** 函数名:   property_build_post_json
** 函数说明: 构造属性上报JSON(thing/property/post格式)
** 参数:    buf - 输出缓冲区, buf_len - 缓冲区长度
** 返回: 实际JSON长度, <0 表示错误
**************************************************************/
int property_build_post_json(char *buf, int buf_len);

/**************************************************************
** 函数名:   property_has_changes
** 函数说明: 检查可写属性是否有变化
** 参数:    无
** 返回: true 有变化, false 无变化
**************************************************************/
bool property_has_changes(void);

/**************************************************************
** 函数名:   property_build_event_json
** 函数说明: 构造事件上报JSON, 仅包含变化的的可写属性
** 参数:    buf - 输出缓冲区, buf_len - 缓冲区长度
** 返回: 实际JSON长度, <0 表示错误, 0 表示无变化
**************************************************************/
int property_build_event_json(char *buf, int buf_len);

/**************************************************************
** 函数名:   property_build_led_event_json
** 函数说明: 构造led事件上报JSON(使用内部存储值)
** 参数:    buf - 输出缓冲区, buf_len - 缓冲区长度
** 返回: 实际JSON长度, <0 表示错误, 0 表示无变化
**************************************************************/
int property_build_led_event_json(char *buf, int buf_len);

/**************************************************************
** 函数名:   property_set_led
** 函数说明: 设置led事件值, 检测到变化时自动标记上报
** 参数:    led1 - led1值(bool), led2 - led2值(bool)
** 返回: 无
**************************************************************/
void property_set_led(bool led1, bool led2);

/**************************************************************
** 函数名:   property_led_has_changes
** 函数说明: 检查led事件值是否有变化
** 参数:    无
** 返回: true 有变化, false 无变化
**************************************************************/
bool property_led_has_changes(void);

/**************************************************************
** 函数名:   property_parse_reply_json
** 函数说明: 解析平台回复JSON, 提取错误码/id并同步计数器
** 参数:    json_str - JSON字符串, err_code - [out]错误码, err_msg - [out]错误描述, msg_len - 缓冲区长度
** 返回: ONENET_OK 解析成功
**************************************************************/
onenet_err_t property_parse_reply_json(const char *json_str,
                                       int *err_code,
                                       char *err_msg, int msg_len);

/**************************************************************
** 函数名:   property_parse_set_json
** 函数说明: 解析平台下发的属性设置命令, 提取并设置属性值
** 参数:    json_str - JSON字符串
** 返回: ONENET_OK 解析成功
**************************************************************/
onenet_err_t property_parse_set_json(const char *json_str);

/**************************************************************
** 函数名:   property_build_set_reply_json
** 函数说明: 构造属性设置回复JSON
** 参数:    buf - 输出缓冲区, buf_len - 缓冲区长度, err_code - 结果码
** 返回: 实际JSON长度, <0 表示错误
**************************************************************/
int property_build_set_reply_json(char *buf, int buf_len, int err_code);

/**************************************************************
** 函数名:   property_build_desired_get_json
** 函数说明: 构造获取期望值请求JSON
** 参数:    buf - 输出缓冲区, buf_len - 缓冲区长度
** 返回: 实际JSON长度, <0 表示错误
**************************************************************/
int property_build_desired_get_json(char *buf, int buf_len);

#endif /* ONENET_PROPERTY_H */
