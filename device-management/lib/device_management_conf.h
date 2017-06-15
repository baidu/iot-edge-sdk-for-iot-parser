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
 * @brief compiling time configuration of device management SDK.
 *
 * @authors Zhao Bo
 */
#ifndef DEVICE_MANAGEMENT_CONF_H
#define DEVICE_MANAGEMENT_CONF_H

#ifdef __cplusplus
extern "C" {
#endif

/* MQTT 发送/订阅 时指定的服务质量 */
#define QOS 1

/* MQTT 保活时间。单位是秒。 */
#define KEEP_ALIVE 60

/* MQTT 连接超时时间。单位是秒。 */
#define CONNECT_TIMEOUT 10

/* MQTT 订阅超时时间。单位是秒。 */
#define SUBSCRIBE_TIMEOUT 10

/* 最多能创建的客户端 */
#define MAX_CLIENT 10

/* 已发送，但还未收到服务器端 accepted/rejected 的消息，被认为是 in flight message。
 * 有超过 MAX_IN_FLIGHT_MESSAGE 之后，再尝试发送将会收到 TOO_MANY_IN_FLIGHT_MESSAGE 错误。*/
#define MAX_IN_FLIGHT_MESSAGE 100

/* 每个客户端可注册不多于此的handler */
#define MAX_SHADOW_PROPERTY_HANDLER 100

#ifdef __cplusplus
}
#endif

#endif