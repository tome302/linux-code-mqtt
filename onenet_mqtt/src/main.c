/**************************************************************
** 文件名: main.c
** 说  明: OneNET MQTT 客户端主程序
**************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>

#include "onenet_config.h"
#include "onenet_mqtt.h"
#include "onenet_property.h"
#include "common_type.h"

/* ======================== 全局控制 ======================== */

static volatile bool g_running = true;

/**************************************************************
** 函数名:   signal_handler
** 函数说明: 信号处理函数, 捕获SIGINT/SIGTERM并设置退出标志
** 参数:    sig - 信号编号
** 返回: 无
**************************************************************/
static void signal_handler(int sig)
{
    (void)sig;
    printf("\n[MAIN] 收到退出信号, 正在停止...\n");
    g_running = false;
}

/* ======================== 消息接收回调 ======================== */

/**************************************************************
** 函数名:   on_platform_message
** 函数说明: 处理平台下发的消息, 解析属性上报回复
** 参数:    topic - 主题字符串, payload - 消息内容, payload_len - 消息长度
** 返回: 无
**************************************************************/
static void on_platform_message(const char *topic, const void *payload, int payload_len)
{
    /* 构造以 '\0' 结尾的字符串 */
    char msg_buf[2048];
    int copy_len = (payload_len < (int)sizeof(msg_buf) - 1) ? payload_len : (int)sizeof(msg_buf) - 1;
    memcpy(msg_buf, payload, copy_len);
    msg_buf[copy_len] = '\0';

    printf("[MAIN] 收到平台消息:\n  topic: %s\n  payload: %s\n", topic, msg_buf);

    char cmp_topic[TOPIC_MAX_LEN];

    /* ---- 属性上报回复 ---- */
    snprintf(cmp_topic, sizeof(cmp_topic),
             TOPIC_PROPERTY_POST_REPLY, PRODUCT_ID, DEVICE_NAME);
    if (strcmp(topic, cmp_topic) == 0) {
        int err_code = -1;
        char err_msg[256] = {0};
        if (property_parse_reply_json(msg_buf, &err_code, err_msg, sizeof(err_msg)) == ONENET_OK) {
            if (err_code == 0 || err_code == 200) {
                printf("[MAIN] 属性上报成功, 平台回复: %s\n", err_msg);
            } else {
                printf("[MAIN] 属性上报失败, errcode=%d, errmsg=%s\n", err_code, err_msg);
            }
        }
        return;
    }

    /* ---- 事件上报回复 ---- */
    snprintf(cmp_topic, sizeof(cmp_topic),
             TOPIC_EVENT_POST_REPLY, PRODUCT_ID, DEVICE_NAME);
    if (strcmp(topic, cmp_topic) == 0) {
        int err_code = -1;
        char err_msg[256] = {0};
        if (property_parse_reply_json(msg_buf, &err_code, err_msg, sizeof(err_msg)) == ONENET_OK) {
            if (err_code == 0 || err_code == 200) {
                printf("[MAIN] 事件上报成功, 平台回复: %s\n", err_msg);
            } else {
                printf("[MAIN] 事件上报失败, errcode=%d, errmsg=%s\n", err_code, err_msg);
            }
        }
        return;
    }

    /* ---- 平台设置属性(设备订阅topic收到) ---- */
    snprintf(cmp_topic, sizeof(cmp_topic),
             TOPIC_PROPERTY_SET, PRODUCT_ID, DEVICE_NAME);
    if (strcmp(topic, cmp_topic) == 0) {
        printf("[MAIN] 收到平台属性设置命令\n");
        onenet_err_t ret = property_parse_set_json(msg_buf);

        /* 构廻set_reply并发布 */
        char reply_buf[512];
        int rlen = property_build_set_reply_json(reply_buf, sizeof(reply_buf),
                                                  (ret == ONENET_OK) ? 200 : 400);
        if (rlen > 0) {
            char set_reply_topic[TOPIC_MAX_LEN];
            snprintf(set_reply_topic, sizeof(set_reply_topic),
                     TOPIC_PROPERTY_SET_REPLY, PRODUCT_ID, DEVICE_NAME);
            printf("[MAIN] 发送属性设置回复: %s\n", reply_buf);
            onenet_mqtt_publish(set_reply_topic, reply_buf, rlen, 0);
        }
        return;
    }

    /* ---- 期望值获取回复 ---- */
    snprintf(cmp_topic, sizeof(cmp_topic),
             TOPIC_PROPERTY_DESIRED_GET_REPLY, PRODUCT_ID, DEVICE_NAME);
    if (strcmp(topic, cmp_topic) == 0) {
        int err_code = -1;
        char err_msg[256] = {0};
        if (property_parse_reply_json(msg_buf, &err_code, err_msg, sizeof(err_msg)) == ONENET_OK) {
            if (err_code == 0 || err_code == 200) {
                printf("[MAIN] 获取期望值成功, 平台回复: %s\n", err_msg);
            } else {
                printf("[MAIN] 获取期望值失败, errcode=%d, errmsg=%s\n", err_code, err_msg);
            }
        }
        return;
    }

    printf("[MAIN] 未匹配的topic: %s\n", topic);
}

/* ======================== 重连逻辑 ======================== */

/**************************************************************
** 函数名:   do_reconnect
** 函数说明: 指数退避重连(5s->10s->20s->40s->60s上限)
** 参数:    无
** 返回: 无
**************************************************************/
static void do_reconnect(void)
{
    int backoff = RECONNECT_INTERVAL;

    while (g_running && !onenet_mqtt_is_connected()) {
        printf("[MAIN] 连接断开, %d 秒后重连...\n", backoff);
        sleep(backoff);

        if (!g_running) break;

        onenet_err_t ret = onenet_mqtt_connect();
        if (ret == ONENET_OK) {
            /* 等待 connect 回调 */
            for (int i = 0; i < 50 && g_running; i++) {
                onenet_mqtt_loop();
                if (onenet_mqtt_is_connected()) {
                    printf("[MAIN] 重连成功\n");
                    /* 重连后重新订阅所有topic */
                    char sub_topic[TOPIC_MAX_LEN];
                    snprintf(sub_topic, sizeof(sub_topic), TOPIC_PROPERTY_POST_REPLY, PRODUCT_ID, DEVICE_NAME);
                    onenet_mqtt_subscribe(sub_topic, 0);
                    snprintf(sub_topic, sizeof(sub_topic), TOPIC_EVENT_POST_REPLY, PRODUCT_ID, DEVICE_NAME);
                    onenet_mqtt_subscribe(sub_topic, 0);
                    snprintf(sub_topic, sizeof(sub_topic), TOPIC_PROPERTY_SET, PRODUCT_ID, DEVICE_NAME);
                    onenet_mqtt_subscribe(sub_topic, 0);
                    snprintf(sub_topic, sizeof(sub_topic), TOPIC_PROPERTY_DESIRED_GET_REPLY, PRODUCT_ID, DEVICE_NAME);
                    onenet_mqtt_subscribe(sub_topic, 0);
                    return;
                }
            }
        }

        /* 指数退避: 5 -> 10 -> 20 -> 40 -> 60(上限) */
        backoff = backoff * 2;
        if (backoff > RECONNECT_MAX) {
            backoff = RECONNECT_MAX;
        }
    }
}

/* ======================== 主函数 ======================== */

/**************************************************************
** 函数名:   main
** 函数说明: 程序入口, 初始化->连接->订阅->主循环(属性上报+重连)
** 参数:    无
** 返回: 0-正常退出, 1-异常退出
**************************************************************/
int main(void)
{
    onenet_err_t ret;
    char json_buf[4096];
    char post_topic[TOPIC_MAX_LEN];
    char event_topic[TOPIC_MAX_LEN];

    /* 注册信号处理 */
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    printf("========================================\n");
    printf("  OneNET MQTT 客户端\n");
    printf("  设备: %s (产品: %s)\n", DEVICE_NAME, PRODUCT_ID);
    printf("  Broker: %s:%d\n", MQTT_BROKER_HOST, MQTT_BROKER_PORT);
    printf("========================================\n\n");

    /* ---- 初始化 ---- */
    property_init();

    ret = onenet_mqtt_init();
    if (ret != ONENET_OK) {
        fprintf(stderr, "[MAIN] MQTT 初始化失败: %d\n", ret);
        return 1;
    }

    /* 注册消息回调 */
    onenet_mqtt_set_message_callback(on_platform_message);

    /* ---- 连接 ---- */
    ret = onenet_mqtt_connect();
    if (ret != ONENET_OK) {
        fprintf(stderr, "[MAIN] 初始连接失败: %d\n", ret);
        onenet_mqtt_destroy();
        return 1;
    }

    /* 等待连接建立 */
    for (int i = 0; i < 100 && g_running; i++) {
        onenet_mqtt_loop();
        if (onenet_mqtt_is_connected()) break;
        usleep(50000);  /* 50ms */
    }

    if (!onenet_mqtt_is_connected()) {
        fprintf(stderr, "[MAIN] 连接超时\n");
        onenet_mqtt_destroy();
        return 1;
    }

    /* ---- 订阅 ---- */
    {
        char sub_topic[TOPIC_MAX_LEN];
        /* 属性上报回复 */
        snprintf(sub_topic, sizeof(sub_topic), TOPIC_PROPERTY_POST_REPLY, PRODUCT_ID, DEVICE_NAME);
        ret = onenet_mqtt_subscribe(sub_topic, 0);
        if (ret != ONENET_OK) fprintf(stderr, "[MAIN] 订阅失败[%s]: %d\n", sub_topic, ret);
        /* 事件上报回复 */
        snprintf(sub_topic, sizeof(sub_topic), TOPIC_EVENT_POST_REPLY, PRODUCT_ID, DEVICE_NAME);
        ret = onenet_mqtt_subscribe(sub_topic, 0);
        if (ret != ONENET_OK) fprintf(stderr, "[MAIN] 订阅失败[%s]: %d\n", sub_topic, ret);
        /* 平台设置属性 */
        snprintf(sub_topic, sizeof(sub_topic), TOPIC_PROPERTY_SET, PRODUCT_ID, DEVICE_NAME);
        ret = onenet_mqtt_subscribe(sub_topic, 0);
        if (ret != ONENET_OK) fprintf(stderr, "[MAIN] 订阅失败[%s]: %d\n", sub_topic, ret);
        /* 期望值获取回复 */
        snprintf(sub_topic, sizeof(sub_topic), TOPIC_PROPERTY_DESIRED_GET_REPLY, PRODUCT_ID, DEVICE_NAME);
        ret = onenet_mqtt_subscribe(sub_topic, 0);
        if (ret != ONENET_OK) fprintf(stderr, "[MAIN] 订阅失败[%s]: %d\n", sub_topic, ret);
    }

    snprintf(post_topic, sizeof(post_topic),
             TOPIC_PROPERTY_POST, PRODUCT_ID, DEVICE_NAME);
    snprintf(event_topic, sizeof(event_topic),
             TOPIC_EVENT_POST, PRODUCT_ID, DEVICE_NAME);

    /* ---- 主循环 ---- */
    printf("[MAIN] 连接成功, 延迟5秒后开始上报...\n");
    sleep(5);
    printf("[MAIN] 进入主循环 (上报间隔: %ds)\n\n", PROPERTY_REPORT_INTERVAL);

    time_t last_report = 0;

    while (g_running) {
        /* 处理网络事件 */
        onenet_mqtt_loop();

        /* 断连检测 + 重连 */
        if (!onenet_mqtt_is_connected()) {
            do_reconnect();
            if (!g_running) break;
            last_report = 0;  /* 重连后立即可上报 */
        }

        /* 定时属性上报 */
        time_t now = time(NULL);
        if (now - last_report >= PROPERTY_REPORT_INTERVAL) {
            int len = property_build_post_json(json_buf, sizeof(json_buf));
            if (len > 0) {
                printf("[MAIN] 属性上报 JSON (%d bytes):\n  %s\n", len, json_buf);
                ret = onenet_mqtt_publish(post_topic, json_buf, len, 0);
                if (ret != ONENET_OK) {
                    fprintf(stderr, "[MAIN] 发布失败: %d\n", ret);
                }
            }

            last_report = now;
        }

        /* led事件变化时上报, 走 event/post */
        if (property_led_has_changes()) {
            char led_buf[1024];
            int llen = property_build_led_event_json(led_buf, sizeof(led_buf));
            if (llen > 0) {
                printf("[MAIN] led事件上报 JSON (%d bytes):\n  %s\n", llen, led_buf);
                ret = onenet_mqtt_publish(event_topic, led_buf, llen, 0);
                if (ret != ONENET_OK) {
                    fprintf(stderr, "[MAIN] led事件发布失败: %d\n", ret);
                }
            }
        }

        /* 可写属性变化时立即上报变化的属性, 走 property/post */
        if (property_has_changes()) {
            char event_buf[4096];
            int elen = property_build_event_json(event_buf, sizeof(event_buf));
            if (elen > 0) {
                printf("[MAIN] 属性变化立即上报 JSON (%d bytes):\n  %s\n", elen, event_buf);
                ret = onenet_mqtt_publish(post_topic, event_buf, elen, 0);
                if (ret != ONENET_OK) {
                    fprintf(stderr, "[MAIN] 发布失败: %d\n", ret);
                }
                last_report = time(NULL);  /* 重置定时器 */
            }
        }
    }

    /* ---- 清理退出 ---- */
    printf("[MAIN] 正在清理退出...\n");
    onenet_mqtt_disconnect();
    onenet_mqtt_destroy();
    printf("[MAIN] 程序退出\n");

    return 0;
}
