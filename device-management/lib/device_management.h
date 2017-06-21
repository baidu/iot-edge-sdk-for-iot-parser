/*
* Copyright (c) 2016 Baidu, Inc. All Rights Reserved.
*
* Licensed to the Apache Software Foundation (ASF) under one or more
* contributor license agreements.  See the NOTICE file distributed with
* this work for additional information regarding copyright ownership.
* The ASF licenses this file to You under the Apache License, Version 2.0
* (the "License"); you may not use this file except in compliance with
* the License.  You may obtain a copy of the License at
*
*    http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

/**
 * @brief interface of device management SDK.
 *
 * @authors Zhao Bo
 */
#ifndef DEVICE_MANAGEMENT_H
#define DEVICE_MANAGEMENT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <cjson/cJSON.h>
#include <stdint.h>
#include <stdbool.h>

/*
 * 涉及到的MQTT主题：
 * 1. PUB $baidu/iot/shadow/${deviceName}/update 将设备状态更新到设备影子
 * 1.1 SUB $baidu/iot/shadow/${deviceName}/update/accepted 获取设备影子更新成功后的结果
 * 1.2 SUB sub $baidu/iot/shadow/${deviceName}/update/rejected 获取设备影子更新失败后的结果
 * 2. PUB $baidu/iot/shadow/${deviceName}/get 获取该设备在设备影子中的所有状态信息
 * 2.1 SUB sub $baidu/iot/shadow/${deviceName}/get/accepted 获取设备影子
 * 2.2 SUB $baidu/iot/shadow/${deviceName}/get/rejected 获取设备影子失败的相关消息
 * 3. SUB $baidu/iot/shadow/{deviceName}/delta 设备影子的变化
 * 3.1 PUB $baidu/iot/shadow/${deviceName}/delta/rejected 若设备更新状态失败，可将相关错误信息发送到物管理
 */

typedef enum {
    SUCCESS = 0,
    FAILURE = -1,
    NULL_POINTER = -2,
    /* 物管理客户端未连接 */
    NOT_CONNECTED = -3,
    /* 尝试注册超过上限（由 MAX_SHADOW_PROPERTY_HANDLER 定义）的 handler */
    TOO_MANY_SHADOW_PROPERTY_HANDLER = -4,
    BAD_ARGUMENT = -5,
    TOO_MANY_IN_FLIGHT_MESSAGE = -6,
    NO_MATCHING_IN_FLIGHT_MESSAGE = -7,
} DmReturnCode;

typedef enum {
    SHADOW_GET = 0,
    SHADOW_UPDATE,
    SHADOW_DELETE,
    /* 表示一个不合法的 shadow action */
    SHADOW_INVALID,
} ShadowAction;

typedef enum {
    SHADOW_ACK_ACCEPTED = 0,
    SHADOW_ACK_REJECTED,
    /* 在一定时间内没有收到服务器端的 accepted/rejected */
    SHADOW_ACK_TIMEOUT,
} ShadowAckStatus;

/* 如果 GET/UPDATE 被服务器端接受，就会返回一个ShadowResponse */
typedef struct {
    cJSON *reported;
    cJSON *desired;
    struct {
        cJSON *reported;
        cJSON *desired;
    } lastUpdatedTime;
    int profileVersion;
} ShadowResponse;

/* 如果 GET/UPDATE 被服务器端接受，就会返回一个ShadowResponse */
typedef struct {
    ShadowResponse response;
} ShadowActionAccepted;

typedef struct {
    const char *code;
    const char *message;
} ShadowActionRejected;

typedef union {
    ShadowActionAccepted accepted;
    ShadowActionRejected rejected;
} ShadowActionAck;

typedef struct {
    const char *code;
    const char *message;

    /* 如何释放这个 error */
    void (*destroyer)(void *error);
} UserDefinedError;

/**
 * @brief 影子 GET/UPDATE/DELETE 收到 ACK 或者超时后的回调
 */
typedef void (*ShadowActionCallback)(ShadowAction action, ShadowAckStatus status, ShadowActionAck *ack, void *context);

/**
 * @brief 影子发生变化时的回调
 *
 * @param name 发生改变属性的名称。如果为 NULL 则表示根。
 * @param desired 发生改变属性的期待值
 * @return 返回一个错误，表示无法按期待值改变属性。否则返回 NULL。
 */
typedef UserDefinedError *(*ShadowPropertyDeltaCallback)(const char *name, struct cJSON *desired);

struct device_management_client_t;

typedef struct device_management_client_t *DeviceManagementClient;

DmReturnCode device_management_init();

DmReturnCode device_management_fini();

/**
 * @brief 创建一个物管理客户端
 *
 * @param client 返回所创建的客户端
 * @param broker 服务器地址，形式如 "tcp://server:1883"
 * @param deviceName 设备名字
 * @param username MQTT 用户名
 * @param password MQTT 密码
 * @param clientId MQTT 客户端ID。可以传NULL，将以deviceName作为clientId。
 * @param trustStore 文件名。不打开SSL的时候可以传NULL。
 * @return
 */
DmReturnCode device_management_create(DeviceManagementClient *client, const char *broker, const char *deviceName,
                                      const char *username, const char *password, const char *clientId, const char *trustStore);

/**
 * @brief 连接客户端至服务器
 *
 * @param client 物管理客户端
 * @return
 */
DmReturnCode device_management_connect(DeviceManagementClient client);

/**
 * @brief 断开并销毁客户端
 *
 * @param client 物管理客户端
 * @return
 */
DmReturnCode device_management_destroy(DeviceManagementClient client);

/**
 * @brief 更新设备影子
 *
 * @param client 物管理客户端
 * @param callback 完成之后的回调
 * @param context 传递给回调的上下文
 * @param timeout 为这个请求指定一个超时时间。单位为秒。
 * @param reported 要上报的内容，设备端会上报。
 * @param desired 期待值，控制端会发送。设备端调用update的时候，该参数一般为NULL。
 * @return 代码
 */
DmReturnCode
device_management_shadow_update(DeviceManagementClient client, ShadowActionCallback callback, void *context,
                                uint8_t timeout, cJSON *reported, cJSON *desired);

/**
 * @brief 获取设备影子
 *
 * @param client 物管理客户端
 * @param callback 完成之后的回调
 * @param context 传递给回调的上下文
 * @param timeout 为这个请求指定一个超时时间。单位为秒。
 * @return 代码
 */
DmReturnCode device_management_shadow_get(DeviceManagementClient client, ShadowActionCallback callback, void *context,
                                          uint8_t timeout);

/**
 * @brief 删除设备影子
 *
 * @param client 物管理客户端
 * @param callback 完成之后的回调
 * @param context 传递给回调的上下文
 * @param timeout 为这个请求指定一个超时时间。单位为秒。
 * @return 代码
 */
DmReturnCode
device_management_shadow_delete(DeviceManagementClient client, ShadowActionCallback callback, void *context,
                                uint8_t timeout);

/**
 * @brief 为某一个属性注册一个回调，在这个属性的 desired 值变化时，得到通知。
 *
 * @param client 物管理客户端
 * @param key 属性名字。传NULL则表示为整个shadow注册一个回调。
 * @param cb 收到delta之后的回调
 * @return 代码
 */
DmReturnCode
device_management_shadow_register_delta(DeviceManagementClient client, const char *key, ShadowPropertyDeltaCallback cb);

#ifdef __cplusplus
}
#endif
#endif