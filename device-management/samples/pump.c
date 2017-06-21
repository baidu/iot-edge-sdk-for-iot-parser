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
 * @brief 一个模拟智能茶壶的例子。在这个例子中将用到update shadow，以及按照shadow delta的设定，来控制茶壶的温度。
 * 茶壶的温度可以每秒钟升高/降低1度。
 *
 * @authors Zhao Bo
 */
#include <stdlib.h>
#include <device_management.h>
#include <stdio.h>
#include <zconf.h>
#include <log4c.h>

static log4c_category_t *category;

void check_return_code(DmReturnCode rc) {
    if (rc != SUCCESS) {
        exit(rc);
    }
}

static int acceptedAck = 0;

static int rejectedAck = 0;

static int timeoutAck = 0;

void shadow_action_callback(ShadowAction action, ShadowAckStatus status, ShadowActionAck *ack, void *context) {
    log4c_category_log(category, LOG4C_PRIORITY_INFO, "action callback. action=%d, status=%d.", action, status);
    if (status == SHADOW_ACK_ACCEPTED) {
        acceptedAck++;
    } else if (status == SHADOW_ACK_REJECTED) {
        rejectedAck++;
    } else if (status == SHADOW_ACK_TIMEOUT) {
        timeoutAck++;
    }
}

int main() {
    DmReturnCode rc;
    const char *broker = "ssl://samples.mqtt.iot.gz.baidubce.com:1884";
    const char *username = "test/test"; /* 设置为您设备的用户名 */
    const char *password = "test"; /* 设置为您设备的密码 */
    const char *deviceName = "pump1"; /* 设置为您设备的名字 */

    device_management_init();

    category = log4c_category_new("pump");

    DeviceManagementClient client;
    /* 设置为正确的root_cert.pem路径. git repository根目录有一份该文件。*/
    rc = device_management_create(&client, broker, deviceName, username, password, NULL, "./root_cert.pem");
    check_return_code(rc);
    rc = device_management_connect(client);
    check_return_code(rc);

    cJSON *reported = cJSON_CreateObject();

    cJSON *frequencyIn = cJSON_CreateNumber(20);
    cJSON *current = cJSON_CreateNumber(111.0);
    cJSON *speed = cJSON_CreateNumber(1033);
    cJSON *torque = cJSON_CreateNumber(41.5);
    cJSON *power = cJSON_CreateNumber(31.9);
    cJSON *dcBusVoltage = cJSON_CreateNumber(543);
    cJSON *outputVoltage = cJSON_CreateNumber(440);
    cJSON *driveTemp = cJSON_CreateNumber(40);

    cJSON_AddItemToObject(reported, "FrequencyIn", frequencyIn);
    cJSON_AddItemToObject(reported, "Current", current);
    cJSON_AddItemToObject(reported, "Speed", speed);
    cJSON_AddItemToObject(reported, "Torque", torque);
    cJSON_AddItemToObject(reported, "Power", power);
    cJSON_AddItemToObject(reported, "DC_bus_voltage", dcBusVoltage);
    cJSON_AddItemToObject(reported, "Output_voltage", outputVoltage);
    cJSON_AddItemToObject(reported, "Drive-temp", driveTemp);

    rc = device_management_shadow_update(client, shadow_action_callback, NULL, 10, reported, NULL);
    check_return_code(rc);
    // 设备影子的更新是异步的,device_management_shadow_update返回并不等于已经收到服务器端的响应.
    // 在这个示例中,我们在一个loop中等待收到ACK.
    while (acceptedAck + rejectedAck + timeoutAck < 1) {
        sleep(1);
    }

    cJSON_Delete(reported);

    rc = device_management_destroy(client);
    check_return_code(rc);

    rc = device_management_fini();
    check_return_code(rc);
}