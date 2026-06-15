/**************************************************************
** 文件名: onenet_property.c
** 说  明: OneNET 物模型属性 JSON 序列化/反序列化
**************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <time.h>
#include <json-c/json.h>

#include "onenet_property.h"
#include "common_type.h"

/* ======================== 消息ID计数器 ======================== */

static int g_msg_id = 1;  /* 从 1 开始递增 */
static char g_platform_cmd_id[32] = {0};  /* 平台下发命令的id, 用于回复 */
static bool g_prop_dirty = false;  /* 可写属性变化标志 */
static bool g_led1 = false, g_led2 = false;      /* led事件当前值 */
static bool g_led1_prev = false, g_led2_prev = false;  /* led上次上报值 */
static bool g_led_dirty = false;  /* led变化标志 */

/* ======================== 属性表定义 ======================== */

typedef struct {
    const char   *key;
    prop_type_t   type;
    bool          writable;
    prop_value_t  value;
    prop_value_t  prev_value;  /* 上次上报时的值, 用于变化检测 */
} property_item_t;

/* 11 个物模型属性, 按标识符排列 */
static property_item_t g_props[] = {
    { PROP_KEY_CSQ,               PROP_TYPE_INT64, false, {.i64 = 20}     },
    { PROP_KEY_BATTERY_PCT,       PROP_TYPE_INT64, false, {.i64 = 90}     },
    { PROP_KEY_BATTERY_STATE,     PROP_TYPE_ENUM,  false, {.i32 = 2}      },
    { PROP_KEY_HOT_VALVE,         PROP_TYPE_INT32, true,  {.i32 = 10}     },
    { PROP_KEY_HUMIDITY,          PROP_TYPE_INT64, false, {.i64 = 45}     },
    { PROP_KEY_MAXHUM_SET,        PROP_TYPE_INT64, true,  {.i64 = 80}     },
    { PROP_KEY_MAXTEMP_SET,       PROP_TYPE_FLOAT, true,  {.f32 = 60.0f}  },
    { PROP_KEY_MINIHUM_SET,       PROP_TYPE_INT64, true,  {.i64 = 20}     },
    { PROP_KEY_MINITEMP_SET,      PROP_TYPE_FLOAT, true,  {.f32 = -10.0f} },
    { PROP_KEY_TEMP_UNIT_CONVERT, PROP_TYPE_BOOL,  true,  {.boolean = false} },
    { PROP_KEY_TEMP_VALUE,        PROP_TYPE_FLOAT, false, {.f32 = 25.5f}  },
};

#define PROP_COUNT  (sizeof(g_props) / sizeof(g_props[0]))

/* ======================== 内部辅助 ======================== */

/**************************************************************
** 函数名:   find_prop_index
** 函数说明: 根据key查找属性索引
** 参数:    key - 属性标识符
** 返回: 索引 >= 0 成功, -1 未找到
**************************************************************/
static int find_prop_index(const char *key)
{
    for (int i = 0; i < (int)PROP_COUNT; i++) {
        if (strcmp(g_props[i].key, key) == 0) {
            return i;
        }
    }
    return -1;
}

/**************************************************************
** 函数名:   get_timestamp_ms
** 函数说明: 获取当前毫秒时间戳
** 参数:    无
** 返回: 当前时间的毫秒时间戳
**************************************************************/
static int64_t get_timestamp_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

/**************************************************************
** 函数名:   add_prop_to_params
** 函数说明: 向params对象添加一个属性节点, 格式: "key": {"value": xxx, "time": ts}
** 参数:    params - JSON对象, idx - 属性索引, ts - 时间戳
** 返回: 无
**************************************************************/
static void add_prop_to_params(json_object *params, int idx, int64_t ts)
{
    json_object *prop_obj = json_object_new_object();
    if (!prop_obj) return;

    /* 根据类型添加 value */
    switch (g_props[idx].type) {
    case PROP_TYPE_INT32:
    case PROP_TYPE_ENUM:
        json_object_object_add(prop_obj, "value",
            json_object_new_int(g_props[idx].value.i32));
        break;
    case PROP_TYPE_INT64:
        json_object_object_add(prop_obj, "value",
            json_object_new_int64(g_props[idx].value.i64));
        break;
    case PROP_TYPE_FLOAT:
        json_object_object_add(prop_obj, "value",
            json_object_new_double((double)g_props[idx].value.f32));
        break;
    case PROP_TYPE_BOOL:
        json_object_object_add(prop_obj, "value",
            json_object_new_boolean(g_props[idx].value.boolean));
        break;
    }

    /* 添加时间戳 */
    json_object_object_add(prop_obj, "time",
        json_object_new_int64(ts));

    json_object_object_add(params, g_props[idx].key, prop_obj);
}

/* ======================== 公开接口实现 ======================== */

/**************************************************************
** 函数名:   property_init
** 函数说明: 初始化属性模块, 设置默认值
** 参数:    无
** 返回: 无
**************************************************************/
void property_init(void)
{
    /* 初始化 prev_value 为当前值, 避免启动时误报变化 */
    for (int i = 0; i < (int)PROP_COUNT; i++) {
        g_props[i].prev_value = g_props[i].value;
    }
    g_prop_dirty = false;
    printf("[PROP] 属性模块初始化完成, 共 %zu 个属性\n", PROP_COUNT);
}

/**************************************************************
** 函数名:   property_set_value
** 函数说明: 设置属性值(平台下发时调用)
** 参数:    key - 属性标识符, value - 属性值
** 返回: ONENET_OK 成功, ONENET_ERR_PARAM 未知属性或只读
**************************************************************/
onenet_err_t property_set_value(const char *key, prop_value_t value)
{
    int idx = find_prop_index(key);
    if (idx < 0) {
        printf("[PROP] 未知属性: %s\n", key);
        return ONENET_ERR_PARAM;
    }

    if (!g_props[idx].writable) {
        printf("[PROP] 属性 %s 为只读, 忽略写入\n", key);
        return ONENET_ERR_PARAM;
    }

    /* 检测值是否变化, 变化则置脏标志 */
    bool changed = false;
    switch (g_props[idx].type) {
    case PROP_TYPE_INT32:
    case PROP_TYPE_ENUM:
        changed = (g_props[idx].value.i32 != value.i32);
        break;
    case PROP_TYPE_INT64:
        changed = (g_props[idx].value.i64 != value.i64);
        break;
    case PROP_TYPE_FLOAT:
        changed = (g_props[idx].value.f32 != value.f32);
        break;
    case PROP_TYPE_BOOL:
        changed = (g_props[idx].value.boolean != value.boolean);
        break;
    }

    g_props[idx].value = value;
    if (changed) {
        g_prop_dirty = true;
        printf("[PROP] 属性变化: %s, 标记立即上报\n", key);
    } else {
        printf("[PROP] 属性未变: %s\n", key);
    }
    return ONENET_OK;
}

/**************************************************************
** 函数名:   property_get_value
** 函数说明: 获取属性值
** 参数:    key - 属性标识符, value - [out]属性值
** 返回: ONENET_OK 成功, ONENET_ERR_PARAM 未知属性
**************************************************************/
onenet_err_t property_get_value(const char *key, prop_value_t *value)
{
    if (!key || !value) return ONENET_ERR_PARAM;

    int idx = find_prop_index(key);
    if (idx < 0) return ONENET_ERR_PARAM;

    *value = g_props[idx].value;
    return ONENET_OK;
}

/**************************************************************
** 函数名:   property_build_post_json
** 函数说明: 构造属性上报JSON(thing/property/post格式)
** 参数:    buf - 输出缓冲区, buf_len - 缓冲区长度
** 返回: 实际JSON长度, <0 表示错误
**************************************************************/
int property_build_post_json(char *buf, int buf_len)
{
    if (!buf || buf_len <= 0) return -1;

    int64_t ts = get_timestamp_ms();

    /* 构造顶层对象, id 使用自增计数器 */
    json_object *root = json_object_new_object();
    if (!root) return -1;

    char id_buf[32];
    snprintf(id_buf, sizeof(id_buf), "%d", g_msg_id);
    json_object_object_add(root, "id", json_object_new_string(id_buf));
    json_object_object_add(root, "version", json_object_new_string("1.0"));

    /* 构造 params */
    json_object *params = json_object_new_object();
    if (!params) {
        json_object_put(root);
        return -1;
    }

    /* 添加所有属性 */
    for (int i = 0; i < (int)PROP_COUNT; i++) {
        add_prop_to_params(params, i, ts);
    }

    json_object_object_add(root, "params", params);

    /* 序列化为 JSON 字符串 */
    const char *json_str = json_object_to_json_string_ext(root,
        JSON_C_TO_STRING_PLAIN);
    if (!json_str) {
        json_object_put(root);
        return -1;
    }

    int len = (int)strlen(json_str);
    if (len >= buf_len) {
        json_object_put(root);
        return -1;
    }

    memcpy(buf, json_str, len + 1);  /* 包含 '\0' */
    json_object_put(root);  /* 释放 JSON 对象 */

    /* 上报成功, id 自增 */
    g_msg_id++;

    return len;
}

/**************************************************************
** 函数名:   property_has_changes
** 函数说明: 检查可写属性是否有变化
** 参数:    无
** 返回: true 有变化, false 无变化
**************************************************************/
bool property_has_changes(void)
{
    return g_prop_dirty;
}

/**************************************************************
** 函数名:   property_set_led
** 函数说明: 设置led事件值, 检测到变化时置脏标志
** 参数:    led1 - led1值(bool), led2 - led2值(bool)
** 返回: 无
**************************************************************/
void property_set_led(bool led1, bool led2)
{
    if (led1 != g_led1 || led2 != g_led2) {
        g_led1 = led1;
        g_led2 = led2;
        g_led_dirty = true;
        printf("[PROP] led状态变化: led1=%d, led2=%d, 标记上报\n", led1, led2);
    }
}

/**************************************************************
** 函数名:   property_led_has_changes
** 函数说明: 检查led事件值是否有变化
** 参数:    无
** 返回: true 有变化, false 无变化
**************************************************************/
bool property_led_has_changes(void)
{
    return g_led_dirty;
}

/**************************************************************
** 函数名:   property_build_event_json
** 函数说明: 构造事件上报JSON, 仅包含发生变化的可写属性
** 参数:    buf - 输出缓冲区, buf_len - 缓冲区长度
** 返回: 实际JSON长度, <0 表示错误, 0 表示无变化
**************************************************************/
int property_build_event_json(char *buf, int buf_len)
{
    if (!buf || buf_len <= 0) return -1;
    if (!g_prop_dirty) return 0;

    int64_t ts = get_timestamp_ms();

    json_object *root = json_object_new_object();
    if (!root) return -1;

    char id_buf[32];
    snprintf(id_buf, sizeof(id_buf), "%d", g_msg_id);
    json_object_object_add(root, "id", json_object_new_string(id_buf));
    json_object_object_add(root, "version", json_object_new_string("1.0"));

    json_object *params = json_object_new_object();
    if (!params) {
        json_object_put(root);
        return -1;
    }

    int changed_count = 0;
    for (int i = 0; i < (int)PROP_COUNT; i++) {
        if (!g_props[i].writable) continue;

        bool changed = false;
        switch (g_props[i].type) {
        case PROP_TYPE_INT32:
        case PROP_TYPE_ENUM:
            changed = (g_props[i].value.i32 != g_props[i].prev_value.i32);
            break;
        case PROP_TYPE_INT64:
            changed = (g_props[i].value.i64 != g_props[i].prev_value.i64);
            break;
        case PROP_TYPE_FLOAT:
            changed = (g_props[i].value.f32 != g_props[i].prev_value.f32);
            break;
        case PROP_TYPE_BOOL:
            changed = (g_props[i].value.boolean != g_props[i].prev_value.boolean);
            break;
        }

        if (changed) {
            add_prop_to_params(params, i, ts);
            g_props[i].prev_value = g_props[i].value;
            changed_count++;
            printf("[PROP] 事件包含: %s\n", g_props[i].key);
        }
    }

    if (changed_count == 0) {
        json_object_put(params);
        json_object_put(root);
        g_prop_dirty = false;
        return 0;
    }

    json_object_object_add(root, "params", params);

    const char *json_str = json_object_to_json_string_ext(root,
        JSON_C_TO_STRING_PLAIN);
    if (!json_str) {
        json_object_put(root);
        return -1;
    }

    int len = (int)strlen(json_str);
    if (len >= buf_len) {
        json_object_put(root);
        return -1;
    }

    memcpy(buf, json_str, len + 1);
    json_object_put(root);

    g_msg_id++;
    g_prop_dirty = false;  /* 清除脏标志 */
    printf("[PROP] 事件上报: %d 个变化属性\n", changed_count);
    return len;
}

/**************************************************************
** 函数名:   property_build_led_event_json
** 函数说明: 构造led事件上报JSON(使用内部存储值), 清除脏标志
** 参数:    buf - 输出缓冲区, buf_len - 缓冲区长度
** 返回: 实际JSON长度, <0 表示错误, 0 表示无变化
**************************************************************/
int property_build_led_event_json(char *buf, int buf_len)
{
    if (!buf || buf_len <= 0) return -1;
    if (!g_led_dirty) return 0;

    int64_t ts = get_timestamp_ms();

    json_object *root = json_object_new_object();
    if (!root) return -1;

    char id_buf[32];
    snprintf(id_buf, sizeof(id_buf), "%d", g_msg_id);
    json_object_object_add(root, "id", json_object_new_string(id_buf));
    json_object_object_add(root, "version", json_object_new_string("1.0"));

    /* 构造 params -> led -> {value: {led1, led2}, time: ts} */
    json_object *params = json_object_new_object();
    if (!params) {
        json_object_put(root);
        return -1;
    }

    json_object *led_obj = json_object_new_object();
    if (!led_obj) {
        json_object_put(params);
        json_object_put(root);
        return -1;
    }

    /* value 为结构体对象 */
    json_object *value_obj = json_object_new_object();
    if (!value_obj) {
        json_object_put(led_obj);
        json_object_put(params);
        json_object_put(root);
        return -1;
    }
    json_object_object_add(value_obj, "led1", json_object_new_boolean(g_led1));
    json_object_object_add(value_obj, "led2", json_object_new_boolean(g_led2));

    json_object_object_add(led_obj, "value", value_obj);
    json_object_object_add(led_obj, "time", json_object_new_int64(ts));

    json_object_object_add(params, EVENT_KEY_LED, led_obj);
    json_object_object_add(root, "params", params);

    const char *json_str = json_object_to_json_string_ext(root,
        JSON_C_TO_STRING_PLAIN);
    if (!json_str) {
        json_object_put(root);
        return -1;
    }

    int len = (int)strlen(json_str);
    if (len >= buf_len) {
        json_object_put(root);
        return -1;
    }

    memcpy(buf, json_str, len + 1);
    json_object_put(root);

    g_msg_id++;
    g_led_dirty = false;  /* 清除脏标志 */
    g_led1_prev = g_led1;  /* 更新历史值 */
    g_led2_prev = g_led2;
    return len;
}

/**************************************************************
** 函数名:   property_parse_reply_json
** 函数说明: 解析平台回复JSON, 提取错误码/id并同步计数器
** 参数:    json_str - JSON字符串, err_code - [out]错误码, err_msg - [out]错误描述, msg_len - 缓冲区长度
** 返回: ONENET_OK 解析成功
**************************************************************/
onenet_err_t property_parse_reply_json(const char *json_str,
                                       int *err_code,
                                       char *err_msg, int msg_len)
{
    if (!json_str || !err_code || !err_msg || msg_len <= 0) {
        return ONENET_ERR_PARAM;
    }

    *err_code = -1;
    err_msg[0] = '\0';

    json_object *root = json_tokener_parse(json_str);
    if (!root) {
        snprintf(err_msg, msg_len, "JSON 解析失败");
        return ONENET_ERR_JSON;
    }

    /* 提取 "id" 并同步计数器: 平台下发 id 后, 下次上报从该值累加 */
    json_object *id_obj = NULL;
    if (json_object_object_get_ex(root, "id", &id_obj)) {
        const char *id_str = json_object_get_string(id_obj);
        if (id_str) {
            int platform_id = atoi(id_str);
            if (platform_id >= g_msg_id) {
                g_msg_id = platform_id + 1;
                printf("[PROP] id 同步: 平台id=%d, 下次上报id=%d\n",
                       platform_id, g_msg_id);
            }
        }
    }

    /* 提取 "errcode" 或 "code" */
    json_object *code_obj = NULL;
    if (json_object_object_get_ex(root, "errcode", &code_obj) ||
        json_object_object_get_ex(root, "code", &code_obj)) {
        *err_code = json_object_get_int(code_obj);
    }

    /* 提取 "errmsg" 或 "msg" */
    json_object *msg_obj = NULL;
    if (json_object_object_get_ex(root, "errmsg", &msg_obj) ||
        json_object_object_get_ex(root, "msg", &msg_obj)) {
        const char *msg = json_object_get_string(msg_obj);
        if (msg) {
            snprintf(err_msg, msg_len, "%s", msg);
        }
    }

    json_object_put(root);
    return ONENET_OK;
}

/**************************************************************
** 函数名:   property_parse_set_json
** 函数说明: 解析平台下发的属性设置命令, 提取params中的属性并设置
** 参数:    json_str - JSON字符串
** 返回: ONENET_OK 解析成功
**************************************************************/
onenet_err_t property_parse_set_json(const char *json_str)
{
    if (!json_str) return ONENET_ERR_PARAM;

    json_object *root = json_tokener_parse(json_str);
    if (!root) {
        printf("[PROP] 属性设置JSON解析失败\n");
        return ONENET_ERR_JSON;
    }

    /* 提取平台下发的id, 回复时使用相同的id */
    json_object *id_obj = NULL;
    if (json_object_object_get_ex(root, "id", &id_obj)) {
        const char *id_str = json_object_get_string(id_obj);
        if (id_str) {
            snprintf(g_platform_cmd_id, sizeof(g_platform_cmd_id), "%s", id_str);
            printf("[PROP] 平台命令id: %s\n", g_platform_cmd_id);
        }
    }

    /* 提取 params 对象 */
    json_object *params = NULL;
    if (!json_object_object_get_ex(root, "params", &params)) {
        printf("[PROP] 属性设置命令中未找到params\n");
        json_object_put(root);
        return ONENET_ERR_JSON;
    }

    /* 遍历 params 中的每个属性 */
    /* 平台格式: "params": {"hot_valve": 12}  (直接值, 非 {"value":12}) */
    int set_ok = 0, set_fail = 0;
    json_object_object_foreach(params, key, val_obj) {
        int idx = find_prop_index(key);
        if (idx < 0) {
            printf("[PROP] 未知属性: %s\n", key);
            set_fail++;
            continue;
        }

        /* 平台下发的是直接值, 如 "hot_valve": 12 */
        prop_value_t pv;
        switch (g_props[idx].type) {
        case PROP_TYPE_INT32:
        case PROP_TYPE_ENUM:
            pv.i32 = json_object_get_int(val_obj);
            break;
        case PROP_TYPE_INT64:
            pv.i64 = json_object_get_int64(val_obj);
            break;
        case PROP_TYPE_FLOAT:
            pv.f32 = (float)json_object_get_double(val_obj);
            break;
        case PROP_TYPE_BOOL:
            pv.boolean = json_object_get_boolean(val_obj);
            break;
        }

        if (property_set_value(key, pv) == ONENET_OK) {
            set_ok++;
        } else {
            set_fail++;
        }
    }

    printf("[PROP] 属性设置完成: 成功=%d, 失败=%d\n", set_ok, set_fail);
    json_object_put(root);
    return ONENET_OK;
}

/**************************************************************
** 函数名:   property_build_set_reply_json
** 函数说明: 构造属性设置回复JSON, 使用平台下发的id
** 参数:    buf - 输出缓冲区, buf_len - 缓冲区长度, err_code - 结果码(200成功)
** 返回: 实际JSON长度, <0 表示错误
**************************************************************/
int property_build_set_reply_json(char *buf, int buf_len, int err_code)
{
    if (!buf || buf_len <= 0) return -1;

    json_object *root = json_object_new_object();
    if (!root) return -1;

    /* 使用平台下发的id, 如果没有则使用自增计数器 */
    const char *reply_id = (g_platform_cmd_id[0] != '\0') ? g_platform_cmd_id : "0";
    json_object_object_add(root, "id", json_object_new_string(reply_id));
    json_object_object_add(root, "code", json_object_new_int(err_code));
    json_object_object_add(root, "msg",
        json_object_new_string(err_code == 200 ? "success" : "failed"));

    const char *json_str = json_object_to_json_string_ext(root,
        JSON_C_TO_STRING_PLAIN);
    if (!json_str) {
        json_object_put(root);
        return -1;
    }

    int len = (int)strlen(json_str);
    if (len >= buf_len) {
        json_object_put(root);
        return -1;
    }

    memcpy(buf, json_str, len + 1);
    json_object_put(root);
    return len;
}

/**************************************************************
** 函数名:   property_build_desired_get_json
** 函数说明: 构造获取期望值请求JSON
** 参数:    buf - 输出缓冲区, buf_len - 缓冲区长度
** 返回: 实际JSON长度, <0 表示错误
**************************************************************/
int property_build_desired_get_json(char *buf, int buf_len)
{
    if (!buf || buf_len <= 0) return -1;

    json_object *root = json_object_new_object();
    if (!root) return -1;

    char id_buf[32];
    snprintf(id_buf, sizeof(id_buf), "%d", g_msg_id);
    json_object_object_add(root, "id", json_object_new_string(id_buf));
    json_object_object_add(root, "version", json_object_new_string("1.0"));

    const char *json_str = json_object_to_json_string_ext(root,
        JSON_C_TO_STRING_PLAIN);
    if (!json_str) {
        json_object_put(root);
        return -1;
    }

    int len = (int)strlen(json_str);
    if (len >= buf_len) {
        json_object_put(root);
        return -1;
    }

    memcpy(buf, json_str, len + 1);
    json_object_put(root);
    return len;
}
