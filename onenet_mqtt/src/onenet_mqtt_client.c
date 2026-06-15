/**************************************************************
** 文件名: onenet_mqtt_client.c
** 说  明: OneNET MQTT 客户端实现 (mosquitto 封装)
**************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mosquitto.h>

#include "onenet_config.h"
#include "onenet_mqtt.h"
#include "common_type.h"

/* ======================== 内部状态 ======================== */

static struct mosquitto *g_mosq = NULL;
static mqtt_state_t      g_state = MQTT_STATE_DISCONNECTED;
static onenet_msg_cb_t   g_msg_cb = NULL;

/* ======================== 内部日志 ======================== */

#define LOG_INFO(fmt, ...)  printf("[MQTT INFO]  " fmt "\n", ##__VA_ARGS__)
#define LOG_WARN(fmt, ...)  printf("[MQTT WARN]  " fmt "\n", ##__VA_ARGS__)
#define LOG_ERR(fmt, ...)   fprintf(stderr, "[MQTT ERROR] " fmt "\n", ##__VA_ARGS__)

/* ======================== Mosquitto 回调 ======================== */

/**************************************************************
** 函数名:   on_connect
** 函数说明: 连接回调, 连接成功后设置状态为已连接
** 参数:    mosq - mosquitto实例, obj - 用户数据, rc - 返回码
** 返回: 无
**************************************************************/
static void on_connect(struct mosquitto *mosq, void *obj, int rc)
{
    (void)mosq;
    (void)obj;

    if (rc == 0) {
        g_state = MQTT_STATE_CONNECTED;
        LOG_INFO("连接成功: %s:%d", MQTT_BROKER_HOST, MQTT_BROKER_PORT);
    } else {
        g_state = MQTT_STATE_DISCONNECTED;
        LOG_ERR("连接失败, mosquitto rc=%d", rc);
    }
}

/**************************************************************
** 函数名:   on_disconnect
** 函数说明: 断开回调, 标记断连状态
** 参数:    mosq - mosquitto实例, obj - 用户数据, rc - 返回码
** 返回: 无
**************************************************************/
static void on_disconnect(struct mosquitto *mosq, void *obj, int rc)
{
    (void)mosq;
    (void)obj;

    g_state = MQTT_STATE_DISCONNECTED;
    if (rc == 0) {
        LOG_INFO("正常断开连接");
    } else {
        LOG_WARN("异常断开, mosquitto rc=%d", rc);
    }
}

/**************************************************************
** 函数名:   on_message
** 函数说明: 消息回调, 将收到的消息转发给上层处理
** 参数:    mosq - mosquitto实例, obj - 用户数据, msg - 消息结构体
** 返回: 无
**************************************************************/
static void on_message(struct mosquitto *mosq, void *obj,
                       const struct mosquitto_message *msg)
{
    (void)mosq;
    (void)obj;

    LOG_INFO("收到消息: topic=%s, payloadlen=%d", msg->topic, msg->payloadlen);

    if (g_msg_cb && msg->topic) {
        g_msg_cb(msg->topic, msg->payload, msg->payloadlen);
    }
}

/**************************************************************
** 函数名:   on_log
** 函数说明: 日志回调, mosquitto内部日志输出
** 参数:    mosq - mosquitto实例, obj - 用户数据, level - 日志级别, str - 日志内容
** 返回: 无
**************************************************************/
static void on_log(struct mosquitto *mosq, void *obj, int level, const char *str)
{
    (void)mosq;
    (void)obj;
    (void)level;
    /* 仅在 DEBUG 编译时输出底层日志 */
#ifdef DEBUG
    printf("[MOSQ] %s\n", str);
#else
    (void)str;
#endif
}

/* ======================== 错误码转换 ======================== */

/**************************************************************
** 函数名:   mosq_to_onenet
** 函数说明: 将mosquitto错误码转换为项目错误码
** 参数:    mosq_rc - mosquitto返回码
** 返回: 对应的 onenet_err_t 错误码
**************************************************************/
static onenet_err_t mosq_to_onenet(int mosq_rc)
{
    if (mosq_rc == MOSQ_ERR_SUCCESS) return ONENET_OK;
    if (mosq_rc == MOSQ_ERR_NOMEM)   return ONENET_ERR_MEMORY;
    return ONENET_ERR_MOSQ;
}

/* ======================== 公开接口实现 ======================== */

/**************************************************************
** 函数名:   onenet_mqtt_init
** 函数说明: MQTT客户端初始化, 创建实例/设置凭证/注册回调
** 参数:    无
** 返回: ONENET_OK 成功, 其他为错误码
**************************************************************/
onenet_err_t onenet_mqtt_init(void)
{
    int rc;

    /* 初始化 mosquitto 库 */
    rc = mosquitto_lib_init();
    if (rc != MOSQ_ERR_SUCCESS) {
        LOG_ERR("mosquitto_lib_init 失败: rc=%d", rc);
        return ONENET_ERR_MOSQ;
    }

    /* 创建客户端实例 (clean_session=true) */
    g_mosq = mosquitto_new(MQTT_CLIENT_ID, true, NULL);
    if (!g_mosq) {
        LOG_ERR("mosquitto_new 失败");
        return ONENET_ERR_MEMORY;
    }

    /* 设置用户名密码 */
    rc = mosquitto_username_pw_set(g_mosq, MQTT_USERNAME, MQTT_PASSWORD);
    if (rc != MOSQ_ERR_SUCCESS) {
        LOG_ERR("mosquitto_username_pw_set 失败: rc=%d", rc);
        mosquitto_destroy(g_mosq);
        g_mosq = NULL;
        return mosq_to_onenet(rc);
    }

    /* 禁用 mosquitto 内置自动重连, 由上层控制 */
    mosquitto_reconnect_delay_set(g_mosq, 0, 0, false);

    /* 注册回调 */
    mosquitto_connect_callback_set(g_mosq, on_connect);
    mosquitto_disconnect_callback_set(g_mosq, on_disconnect);
    mosquitto_message_callback_set(g_mosq, on_message);
    mosquitto_log_callback_set(g_mosq, on_log);

    g_state = MQTT_STATE_DISCONNECTED;
    LOG_INFO("MQTT 客户端初始化完成 (client_id=%s)", MQTT_CLIENT_ID);

    return ONENET_OK;
}

/**************************************************************
** 函数名:   onenet_mqtt_connect
** 函数说明: 连接到OneNET MQTT broker
** 参数:    无
** 返回: ONENET_OK 成功, 其他为错误码
**************************************************************/
onenet_err_t onenet_mqtt_connect(void)
{
    int rc;

    if (!g_mosq) {
        LOG_ERR("客户端未初始化");
        return ONENET_ERR_PARAM;
    }

    g_state = MQTT_STATE_CONNECTING;
    LOG_INFO("正在连接 %s:%d ...", MQTT_BROKER_HOST, MQTT_BROKER_PORT);

    rc = mosquitto_connect(g_mosq, MQTT_BROKER_HOST, MQTT_BROKER_PORT,
                           MQTT_KEEPALIVE);
    if (rc != MOSQ_ERR_SUCCESS) {
        g_state = MQTT_STATE_DISCONNECTED;
        LOG_ERR("连接失败: %s (rc=%d)", mosquitto_strerror(rc), rc);
        return mosq_to_onenet(rc);
    }

    return ONENET_OK;
}

/**************************************************************
** 函数名:   onenet_mqtt_disconnect
** 函数说明: 断开MQTT连接
** 参数:    无
** 返回: 无
**************************************************************/
void onenet_mqtt_disconnect(void)
{
    if (g_mosq) {
        mosquitto_disconnect(g_mosq);
        g_state = MQTT_STATE_DISCONNECTED;
        LOG_INFO("已发送断开连接指令");
    }
}

/**************************************************************
** 函数名:   onenet_mqtt_destroy
** 函数说明: 释放MQTT客户端资源
** 参数:    无
** 返回: 无
**************************************************************/
void onenet_mqtt_destroy(void)
{
    if (g_mosq) {
        mosquitto_destroy(g_mosq);
        g_mosq = NULL;
    }
    mosquitto_lib_cleanup();
    g_state = MQTT_STATE_DISCONNECTED;
    LOG_INFO("MQTT 客户端资源已释放");
}

/**************************************************************
** 函数名:   onenet_mqtt_subscribe
** 函数说明: 订阅指定topic
** 参数:    topic - 主题字符串, qos - 服务质量(0/1/2)
** 返回: ONENET_OK 成功, 其他为错误码
**************************************************************/
onenet_err_t onenet_mqtt_subscribe(const char *topic, int qos)
{
    int rc;

    if (!g_mosq || !topic) {
        return ONENET_ERR_PARAM;
    }

    rc = mosquitto_subscribe(g_mosq, NULL, topic, qos);
    if (rc != MOSQ_ERR_SUCCESS) {
        LOG_ERR("订阅失败: topic=%s, rc=%d", topic, rc);
        return mosq_to_onenet(rc);
    }

    LOG_INFO("订阅成功: topic=%s, qos=%d", topic, qos);
    return ONENET_OK;
}

/**************************************************************
** 函数名:   onenet_mqtt_publish
** 函数说明: 发布消息到指定topic
** 参数:    topic - 主题字符串, payload - 消息内容, len - 消息长度, qos - 服务质量
** 返回: ONENET_OK 成功, 其他为错误码
**************************************************************/
onenet_err_t onenet_mqtt_publish(const char *topic, const void *payload,
                                  int len, int qos)
{
    int rc;

    if (!g_mosq || !topic || !payload || len <= 0) {
        return ONENET_ERR_PARAM;
    }

    rc = mosquitto_publish(g_mosq, NULL, topic, len, payload, qos, false);
    if (rc != MOSQ_ERR_SUCCESS) {
        LOG_ERR("发布失败: topic=%s, rc=%d", topic, rc);
        return mosq_to_onenet(rc);
    }

    LOG_INFO("发布成功: topic=%s, len=%d", topic, len);
    return ONENET_OK;
}

/**************************************************************
** 函数名:   onenet_mqtt_loop
** 函数说明: 处理一次网络事件(非阻塞, 超时100ms)
** 参数:    无
** 返回: ONENET_OK 成功, 其他为错误码
**************************************************************/
onenet_err_t onenet_mqtt_loop(void)
{
    int rc;

    if (!g_mosq) {
        return ONENET_ERR_PARAM;
    }

    /* 非阻塞处理, 超时100ms */
    rc = mosquitto_loop(g_mosq, 100, 1);
    if (rc != MOSQ_ERR_SUCCESS) {
        return mosq_to_onenet(rc);
    }

    return ONENET_OK;
}

/**************************************************************
** 函数名:   onenet_mqtt_is_connected
** 函数说明: 查询当前MQTT连接状态
** 参数:    无
** 返回: true-已连接, false-未连接
**************************************************************/
bool onenet_mqtt_is_connected(void)
{
    return (g_state == MQTT_STATE_CONNECTED);
}

/**************************************************************
** 函数名:   onenet_mqtt_set_message_callback
** 函数说明: 注册消息接收回调函数
** 参数:    cb - 回调函数指针
** 返回: 无
**************************************************************/
void onenet_mqtt_set_message_callback(onenet_msg_cb_t cb)
{
    g_msg_cb = cb;
}
