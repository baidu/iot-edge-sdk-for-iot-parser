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

typedef struct {
    int value;
} MyContext;

static log4c_category_t *category;

static volatile int desiredTemperature = 0;

static volatile int actualTemperature = 0;

static bool alwaysUpdate = false; // 当期待值和当前值相同的时候，是否update。

UserDefinedError *
temperature_on_delta(const char *name, struct cJSON *desired) {
    printf("received delta of %s -> %d.\n", name, desired->valueint);
    desiredTemperature = desired->valueint;
    if (desiredTemperature < 0) {
        UserDefinedError *error = malloc(sizeof(UserDefinedError));
        error->message = "Can't set to that temperature.";
        error->code = "InvalidTemperature";
        error->destroyer = free;
        return error;
    } else {
        return NULL;
    }
}

void on_get_ack(ShadowResponse response) {
    cJSON *temperature = cJSON_GetObjectItemCaseSensitive(response.desired, "temperature");
    if (temperature != NULL) {
        printf("received get of temperature -> %d.\n", temperature->valueint);
        desiredTemperature = temperature->valueint;
    }
}

void on_update_ack(ShadowResponse response) {
}

void shadow_action_callback(ShadowAction action, ShadowAckStatus status, ShadowActionAck *ack, void *context) {
    if (status == SHADOW_ACK_ACCEPTED) {
        if (action == SHADOW_GET) {
            on_get_ack(ack->accepted.response);
        } else if (action == SHADOW_UPDATE) {
            on_update_ack(ack->accepted.response);
        }
    } else if (status == SHADOW_ACK_REJECTED) {
        log4c_category_log(category, LOG4C_PRIORITY_ERROR, "REJECTED: code=%s, message=%s.", ack->rejected.code,
                           ack->rejected.message);
    } else if (status == SHADOW_ACK_TIMEOUT) {
    }
}

void check_return_code(DmReturnCode rc) {
    if (rc != SUCCESS) {
        exit(rc);
    }
}

void change_temperature() {
    if (desiredTemperature >= 0 && desiredTemperature != actualTemperature) {
        actualTemperature += (desiredTemperature > actualTemperature ? 1 : -1);
    }
}

int main() {
    DmReturnCode rc;
    const char *broker = "tcp://sandbox.mqtt.iot.gz.baidubce.com:8883"; /* Change to correct address before run. */
    const char *username = "05feeb0897064d7fa203660ad53df4e8/test-tea-pot";
    const char *password = "NtsgATi5GIe5p7cq7KtBYz3DZT2TQZXkQkXn9lt4FIE=";
    const char *deviceName = "test-tea-pot";
    MyContext context;
    bool shouldUpdate;

    device_management_init();

    category = log4c_category_new("tea-pot");

    DeviceManagementClient client;
    rc = device_management_create(&client, broker, deviceName, username, password, NULL, NULL);
    check_return_code(rc);
    rc = device_management_connect(client);
    check_return_code(rc);

    rc = device_management_shadow_register_delta(client, "temperature", temperature_on_delta);
    check_return_code(rc);

    /* 发起一个GET，获取当前desired的值。desired变化的时候我们可以通过delta得到通知，但是初始值只能用GET获取。 */
    device_management_shadow_get(client, shadow_action_callback, NULL, 10);

    cJSON *reported = cJSON_CreateObject();

    cJSON *temp = cJSON_CreateNumber(-1);

    cJSON_AddItemToObject(reported, "temperature", temp);

    int i = 0;
    for (i = 0; i < 60; ++i) {
        sleep(1);
        if (desiredTemperature < 0) {
            continue;
        }
        change_temperature();
        shouldUpdate = alwaysUpdate || temp->valueint != actualTemperature;
        if (shouldUpdate) {
            cJSON_SetIntValue(temp, actualTemperature);
            rc = device_management_shadow_update(client, shadow_action_callback, &context, 10, reported, NULL);
            check_return_code(rc);
        }
    }

    sleep(15);

    cJSON_Delete(reported);

    rc = device_management_destroy(client);
    check_return_code(rc);

    rc = device_management_fini();
    check_return_code(rc);
}
