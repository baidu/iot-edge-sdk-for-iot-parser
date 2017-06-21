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
 * @brief implementation of device management SDK.
 *
 * @authors Zhao Bo
 */
#include "device_management.h"
#include "device_management_conf.h"
#include "device_management_util.h"

#define _GNU_SOURCE
#define __USE_GNU

#include <uuid/uuid.h>
#include <MQTTAsync.h>
#include <log4c.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <zconf.h>
#include <stdio.h>

#define SUB_TOPIC_COUNT 7

#define MAX_UUID_LENGTH (36 + 1) /* According to RFC4122 it has 32 hex digits + 4 dashes. */

#define URI_SSL "ssl://"

static const char *LO4C_CATEGORY_NAME = "device-management";

static const char *REQUEST_ID_KEY = "requestId";

static const char *CODE_KEY = "code";

static const char *MESSAGE_KEY = "message";

static const char *REPORTED = "reported";

static const char *DESIRED = "desired";

static const char *TOPIC_PREFIX = "$baidu/iot/shadow";

static volatile bool inited = false;

static log4c_category_t *category = NULL;

/* Memorize all topics device management uses so that we don't compose them each time. */
typedef struct {
    char *update; /* publish */
    char *updateAccepted; /* subscribe */
    char *updateRejected; /* subscribe */
    char *get; /* publish */
    char *getAccepted; /* subscribe */
    char *getRejected; /* subscribe */
    char *delete; /* publish */
    char *deleteAccepted; /* subscribe */
    char *deleteRejected; /* subscribe */
    char *delta; /* subscribe */
    char *deltaRejected; /* publish */
    char *subTopics[SUB_TOPIC_COUNT];
} TopicContract;

typedef struct {
    char *key; // key 可以为NULL，表示匹配根。
    ShadowPropertyDeltaCallback cb; // 收到更新之后，会调用这个回调。
} ShadowPropertyDeltaHandler;

/* Manages shadow property handlers. It's a add-only collection. */
typedef struct {
    ShadowPropertyDeltaHandler vault[MAX_SHADOW_PROPERTY_HANDLER];
    int index;
    /* This data is accessed from MQTT client's callback. */
    pthread_mutex_t mutex;
} PropertyHandlerTable;

typedef struct {
    char requestId[MAX_UUID_LENGTH];
    ShadowAction action;
    ShadowActionCallback callback;
    void *callbackContext;
    time_t timestamp;
    uint8_t timeout;
    bool free;
} InFlightMessage;

typedef struct {
    InFlightMessage vault[MAX_IN_FLIGHT_MESSAGE];
    /* This data is also accessed from MQTT client's callback. */
    pthread_mutex_t mutex;
} InFlightMessageList;

typedef struct device_management_client_t {
    MQTTAsync mqttClient;
    int errorCode;
    char *errorMessage;
    volatile bool hasSubscribed;
    char *username;
    char *password;
    char *deviceName;
    char *trustStore;
    TopicContract *topicContract;
    PropertyHandlerTable properties;
    InFlightMessageList messages;
    pthread_mutex_t mutex;
} device_management_client_t;

typedef struct {
    device_management_client_t *members[MAX_CLIENT];
    pthread_mutex_t mutex;
} ClientGroup;

static ClientGroup allClients;

static pthread_t inFlightMessageKeeper;

static TopicContract *topic_contract_create(const char *deviceName);

static void topic_contract_destroy(TopicContract *topics);

static bool client_group_add(ClientGroup *group, device_management_client_t *client);

static bool client_group_remove(ClientGroup *group, device_management_client_t *client);

static void client_group_iterate(ClientGroup *clients, void (*fp)(device_management_client_t *c));

static void in_flight_message_house_keep(device_management_client_t *c);

static void *in_flight_message_house_keep_proc(void *ignore);

static const char *message_get_request_id(const cJSON *payload);

static bool device_management_is_connected(DeviceManagementClient client);

static bool device_management_is_connected2(device_management_client_t *c);

static DmReturnCode device_management_shadow_send_json(device_management_client_t *c, const char *topic,
                                                       const char *requestId, cJSON *payload);

static DmReturnCode device_management_shadow_send(DeviceManagementClient client, ShadowAction action, cJSON *payload,
                                                  ShadowActionCallback callback,
                                                  void *context, uint8_t timeout);

static int
device_management_shadow_handle_response(device_management_client_t *c, const char *requestId, ShadowAction action,
                                         ShadowAckStatus status,
                                         cJSON *payload);

static DmReturnCode device_management_delta_arrived(device_management_client_t *c, cJSON *payload);

MQTTAsync_failureData UNKNOWN_FAILURE = {0, 0, "Unknown MQTT failure"};

static void device_management_set_error(device_management_client_t *client, MQTTAsync_failureData *failureData);

static void mqtt_on_connected(void *context, char *cause);

static void mqtt_on_connection_lost(void *context, char *cause);

static void mqtt_on_connect_success(void *context, MQTTAsync_successData *response);

static void mqtt_on_connect_failure(void *context, MQTTAsync_failureData *response);

static void mqtt_on_subscribe_success(void *context, MQTTAsync_successData *response);

static void mqtt_on_subscribe_failure(void *context, MQTTAsync_failureData *response);

static void mqtt_on_publish_success(void *context, MQTTAsync_successData *response);

static void mqtt_on_publish_failure(void *context, MQTTAsync_failureData *response);

static int mqtt_on_message_arrived(void *context, char *topicName, int topicLen, MQTTAsync_message *message);

static void mqtt_on_delivery_complete(void *context, MQTTAsync_token token);

static const char *shadowActionStrings[5] = {"SHADOW_GET", "SHADOW_UPDATE", "SHADOW_DELETE", "SHADOW_INVALID"};

static const char *shadowAckStatusStrings[3] = {"SHADOW_ACK_ACCEPTED", "SHADOW_ACK_REJECTED", "SHADOW_ACK_TIMEOUT"};

/**
 * @brief Global initialization of device management client SDK. Not thread safe.
 *
 */
DmReturnCode device_management_init() {
    if (inited) {
        log4c_category_log(category, LOG4C_PRIORITY_WARN, "already initialized.");
        return SUCCESS;
    }

    if (log4c_init()) {
        fprintf(stderr, "log4c init failed.\n");
        return FAILURE;
    }

    category = log4c_category_new(LO4C_CATEGORY_NAME);

    log4c_category_log(category, LOG4C_PRIORITY_INFO, "initialized.");

    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&(allClients.mutex), NULL);
    pthread_create(&inFlightMessageKeeper, NULL, in_flight_message_house_keep_proc, NULL);
    pthread_mutexattr_destroy(&attr);
    inited = true;
    return SUCCESS;
}

DmReturnCode device_management_fini() {
    if (!inited) {
        printf("not initialized. no clean up needed.");
        return SUCCESS;
    }
    inited = false;
    // Destroy
    pthread_cancel(inFlightMessageKeeper);
    pthread_join(inFlightMessageKeeper, NULL);

    log4c_category_log(category, LOG4C_PRIORITY_INFO, "cleaned up.");

    // Destroy log4c.
    if (category != NULL) {
        log4c_category_delete(category);
    }
    return SUCCESS;
}

DmReturnCode device_management_create(DeviceManagementClient *client, const char *broker, const char *deviceName,
                                      const char *username, const char *password, const char *clientId, const char *trustStore) {
    int rc;
    int i;

    if (client == NULL || broker == NULL || deviceName == NULL || username == NULL || password == NULL) {
        return NULL_POINTER;
    }

    if (strncmp(URI_SSL, broker, strlen(URI_SSL)) == 0) {
        if (trustStore == NULL) {
            return NULL_POINTER;
        }
    }

    device_management_client_t *c = malloc(sizeof(device_management_client_t));
    check_malloc_result(c);

    /* Create MQTT client. Use device name as client ID. */
    rc = MQTTAsync_create(&(c->mqttClient), broker, clientId != NULL ? clientId : deviceName, MQTTCLIENT_PERSISTENCE_NONE, NULL);

    if (rc == EXIT_FAILURE) {
        log4c_category_log(category, LOG4C_PRIORITY_ERROR, "Failed to create. MQTT rc=%d.", rc);
        free(c);
        return FAILURE;
    }

    MQTTAsync_setCallbacks(c->mqttClient, c, mqtt_on_connection_lost, mqtt_on_message_arrived,
                           mqtt_on_delivery_complete);
    MQTTAsync_setConnected(c->mqttClient, c, mqtt_on_connected);

    /* Set up device_management_client_t. */
    c->errorMessage = NULL;
    c->username = strdup(username);
    c->password = strdup(password);
    c->deviceName = strdup(deviceName);
    c->trustStore = trustStore == NULL ? NULL : strdup(trustStore);
    c->topicContract = topic_contract_create(deviceName);
    c->hasSubscribed = false;

    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);

    c->properties.index = 0;
    pthread_mutex_init(&(c->properties.mutex), &attr);
    for (i = 0; i < MAX_IN_FLIGHT_MESSAGE; ++i) {
        c->messages.vault[i].free = true;
    }

    pthread_mutex_init(&(c->messages.mutex), &attr);
    pthread_mutex_init(&(c->mutex), &attr);
    pthread_mutexattr_destroy(&attr);
    client_group_add(&allClients, c);
    *client = c;

    log4c_category_log(category, LOG4C_PRIORITY_INFO, "created. broker=%s, deviceName=%s.",
                       broker, deviceName);
    return SUCCESS;
}

DmReturnCode device_management_connect(DeviceManagementClient client) {
    int rc;
    device_management_client_t *c = client;

    MQTTAsync_connectOptions connectOptions = MQTTAsync_connectOptions_initializer;
    connectOptions.keepAliveInterval = KEEP_ALIVE;
    connectOptions.cleansession = 1;
    connectOptions.username = c->username;
    connectOptions.password = c->password;
    connectOptions.automaticReconnect = true;
    connectOptions.onSuccess = mqtt_on_connect_success;
    connectOptions.onFailure = mqtt_on_connect_failure;
    connectOptions.context = c;
    connectOptions.connectTimeout = CONNECT_TIMEOUT;

    MQTTAsync_SSLOptions sslOptions = MQTTAsync_SSLOptions_initializer;
    if (c->trustStore != NULL) {
        sslOptions.trustStore = c->trustStore;
        sslOptions.enableServerCertAuth = false;
        connectOptions.ssl = &sslOptions;
    }

    if (MQTTAsync_isConnected(c->mqttClient)) {
        log4c_category_log(category, LOG4C_PRIORITY_INFO, "already connected.");
        return SUCCESS;
    }

    log4c_category_log(category, LOG4C_PRIORITY_INFO, "connecting to server.");
    rc = MQTTAsync_connect(c->mqttClient, &connectOptions);
    if (rc != MQTTASYNC_SUCCESS) {
        log4c_category_log(category, LOG4C_PRIORITY_ERROR, "failed to start connecting. MQTT rc=%d.", rc);
        return FAILURE;
    }

    while (!MQTTAsync_isConnected(c->mqttClient) && c->errorMessage == NULL) {
        sleep(1);
    }

    if (MQTTAsync_isConnected(c->mqttClient)) {
        // Wait sub complete.
        while (!device_management_is_connected2(c) && c->errorMessage == NULL) {
            sleep(1);
        }
        return SUCCESS;
    } else if (c->errorMessage != NULL) {
        log4c_category_log(category, LOG4C_PRIORITY_ERROR, "MQTT connect failed. code=%d, message=%s.", c->errorCode,
                           c->errorMessage);
        return FAILURE;
    }

    return SUCCESS;
}

DmReturnCode
device_management_shadow_update(DeviceManagementClient client, ShadowActionCallback callback, void *context,
                                uint8_t timeout, cJSON *reported, cJSON *desired) {
    DmReturnCode rc;

    cJSON *payload;

    if (reported == NULL && desired == NULL) {
        return NULL_POINTER;
    }
    if (reported == desired) {
        return BAD_ARGUMENT;
    }

    payload = cJSON_CreateObject();

    if (reported != NULL) {
        cJSON_AddItemToObject(payload, REPORTED, reported);
    }
    if (desired != NULL) {
        cJSON_AddItemToObject(payload, DESIRED, desired);
    }

    rc = device_management_shadow_send(client, SHADOW_UPDATE, payload, callback, context, timeout);

    cJSON_DetachItemViaPointer(payload, reported);
    cJSON_DetachItemViaPointer(payload, desired);
    cJSON_Delete(payload);

    if (rc != SUCCESS) {
        log4c_category_log(category, LOG4C_PRIORITY_ERROR, "device_management_shadow_update rc=%d", rc);
    }
    return rc;
}

DmReturnCode device_management_shadow_get(DeviceManagementClient client, ShadowActionCallback callback, void *context,
                                          uint8_t timeout) {
    DmReturnCode rc;
    cJSON *payload = cJSON_CreateObject();

    rc = device_management_shadow_send(client, SHADOW_GET, payload, callback, context, timeout);

    cJSON_Delete(payload);

    if (rc != SUCCESS) {
        log4c_category_log(category, LOG4C_PRIORITY_ERROR, "device_management_shadow_get rc=%d", rc);
    }
    return rc;
}

DmReturnCode
device_management_shadow_delete(DeviceManagementClient client, ShadowActionCallback callback, void *context,
                                uint8_t timeout) {
    DmReturnCode rc;
    cJSON *payload = cJSON_CreateObject();

    rc = device_management_shadow_send(client, SHADOW_DELETE, payload, callback, context, timeout);

    cJSON_Delete(payload);

    if (rc != SUCCESS) {
        log4c_category_log(category, LOG4C_PRIORITY_ERROR, "device_management_shadow_delete rc=%d", rc);
    }
    return rc;
}

DmReturnCode device_management_shadow_register_delta(DeviceManagementClient client, const char *key,
                                                     ShadowPropertyDeltaCallback cb) {
    DmReturnCode rc = SUCCESS;

    if (client == NULL || cb == NULL) {
        exit_null_pointer();
    }

    if (!device_management_is_connected(client)) {
        return NOT_CONNECTED;
    }

    device_management_client_t *c = client;

    pthread_mutex_lock(&(c->properties.mutex));
    if (c->properties.index >= MAX_SHADOW_PROPERTY_HANDLER) {
        rc = TOO_MANY_SHADOW_PROPERTY_HANDLER;
    } else {
        c->properties.vault[c->properties.index].key = key == NULL ? NULL : strdup(key);
        c->properties.vault[c->properties.index].cb = cb;
        c->properties.index++;
    }
    pthread_mutex_unlock(&(c->properties.mutex));

    if (rc != SUCCESS) {
        log4c_category_log(category, LOG4C_PRIORITY_ERROR, "device_management_shadow_register_delta rc=%d", rc);
    }
    return rc;
}

DmReturnCode device_management_destroy(DeviceManagementClient client) {
    uint32_t i;
    device_management_client_t *c = client;
    if (c == NULL) {
        log4c_category_log(category, LOG4C_PRIORITY_ERROR, "bad client.");
    } else {
        pthread_mutex_lock(&(c->properties.mutex));
        for (i = 0; i < c->properties.index; ++i) {
            safe_free(&(c->properties.vault[i].key));
        }
        c->properties.index = 0;
        pthread_mutex_unlock(&(c->properties.mutex));

        client_group_remove(&allClients, c);
        safe_free(&(c->username));
        safe_free(&(c->password));
        safe_free(&(c->deviceName));
        safe_free(&(c->trustStore));
        safe_free(&(c->errorMessage));
        MQTTAsync_disconnect(c->mqttClient, NULL);
        MQTTAsync_destroy(&(c->mqttClient));
        topic_contract_destroy(c->topicContract);
        pthread_mutex_destroy(&(c->mutex));
        free(client);
    }

    return SUCCESS;
}

TopicContract *topic_contract_create(const char *deviceName) {
    int rc;
    TopicContract *t = malloc(sizeof(TopicContract));
    check_malloc_result(t);

    rc = asprintf(&(t->update), "%s/%s/update", TOPIC_PREFIX, deviceName);
    rc = asprintf(&(t->updateAccepted), "%s/%s/update/accepted", TOPIC_PREFIX, deviceName);
    rc = asprintf(&(t->updateRejected), "%s/%s/update/rejected", TOPIC_PREFIX, deviceName);
    t->subTopics[0] = t->updateAccepted;
    t->subTopics[1] = t->updateRejected;

    rc = asprintf(&(t->get), "%s/%s/get", TOPIC_PREFIX, deviceName);
    rc = asprintf(&(t->getAccepted), "%s/%s/get/accepted", TOPIC_PREFIX, deviceName);
    rc = asprintf(&(t->getRejected), "%s/%s/get/rejected", TOPIC_PREFIX, deviceName);
    t->subTopics[2] = t->getAccepted;
    t->subTopics[3] = t->getRejected;

    rc = asprintf(&(t->delete), "%s/%s/delete", TOPIC_PREFIX, deviceName);
    rc = asprintf(&(t->deleteAccepted), "%s/%s/delete/accepted", TOPIC_PREFIX, deviceName);
    rc = asprintf(&(t->deleteRejected), "%s/%s/delete/rejected", TOPIC_PREFIX, deviceName);
    t->subTopics[4] = t->deleteAccepted;
    t->subTopics[5] = t->deleteRejected;

    rc = asprintf(&(t->delta), "%s/%s/delta", TOPIC_PREFIX, deviceName);
    rc = asprintf(&(t->deltaRejected), "%s/%s/delta/rejected", TOPIC_PREFIX, deviceName);
    t->subTopics[6] = t->delta;

    return t;
}

void topic_contract_destroy(TopicContract *topics) {
    if (topics != NULL) {
        safe_free(&(topics->update));
        safe_free(&(topics->updateAccepted));
        safe_free(&(topics->updateRejected));
        safe_free(&(topics->get));
        safe_free(&(topics->getAccepted));
        safe_free(&(topics->getRejected));
        safe_free(&(topics->delete));
        safe_free(&(topics->deleteAccepted));
        safe_free(&(topics->deleteRejected));
        safe_free(&(topics->delta));
        safe_free(&(topics->deltaRejected));
        free(topics);
    }
}

void client_group_iterate(ClientGroup *clients, void (*fp)(device_management_client_t *c)) {
    int i;
    pthread_mutex_lock(&(clients->mutex));
    for (i = 0; i < MAX_CLIENT; ++i) {
        if (clients->members[i] != NULL) {
            fp(clients->members[i]);
        }
    }
    pthread_mutex_unlock(&(clients->mutex));
}

bool client_group_add(ClientGroup *group, device_management_client_t *client) {
    int i;
    bool rc = false;

    pthread_mutex_lock(&(group->mutex));
    for (i = 0; i < MAX_CLIENT; ++i) {
        if (group->members[i] == NULL) {
            group->members[i] = client;
            rc = true;
            break;
        }
    }
    pthread_mutex_unlock(&(group->mutex));

    return rc;
}

bool client_group_remove(ClientGroup *group, device_management_client_t *client) {
    int i;
    bool rc = false;

    pthread_mutex_lock(&(group->mutex));
    for (i = 0; i < MAX_CLIENT; ++i) {
        if (group->members[i] == client) {
            group->members[i] = NULL;
            rc = true;
            break;
        }
    }
    pthread_mutex_unlock(&(group->mutex));

    return rc;
}


void in_flight_message_house_keep(device_management_client_t *c) {
    int i;
    time_t now;
    time(&now);
    pthread_mutex_lock(&(c->messages.mutex));
    for (i = 0; i < MAX_IN_FLIGHT_MESSAGE; ++i) {
        if (!c->messages.vault[i].free) {
            double elipse = difftime(now, c->messages.vault[i].timestamp);
            if (elipse > c->messages.vault[i].timeout) {
                log4c_category_log(category, LOG4C_PRIORITY_ERROR, "%s timed out. requestId=%s.",
                                   shadowActionStrings[c->messages.vault[i].action], c->messages.vault[i].requestId);
                if (c->messages.vault[i].callback != NULL) {
                    c->messages.vault[i].callback(c->messages.vault[i].action, SHADOW_ACK_TIMEOUT, NULL,
                                                  c->messages.vault[i].callbackContext);
                }
                c->messages.vault[i].free = true;
            }
        }
    }

    pthread_mutex_unlock(&(c->messages.mutex));

}

void *in_flight_message_house_keep_proc(void *ignore) {
    while (1) {
        client_group_iterate(&allClients, in_flight_message_house_keep);
        sleep(1);
    }
}

static const char *EMPTY_UUID = "00000000-0000-0000-0000-000000000000";
const char *message_get_request_id(const cJSON *payload) {
    cJSON *requestId = cJSON_GetObjectItemCaseSensitive(payload, "requestId");
    return requestId != NULL ? requestId->valuestring : EMPTY_UUID;
}

void exit_null_pointer() {
    log4c_category_log(category, LOG4C_PRIORITY_ERROR, "NULL POINTER");
    exit(NULL_POINTER);
}

DmReturnCode in_flight_message_add(InFlightMessageList *table, const char *requestId, ShadowAction action,
                                   ShadowActionCallback callback,
                                   void *context, uint8_t timeout) {
    DmReturnCode rc = TOO_MANY_IN_FLIGHT_MESSAGE;
    int i;

    pthread_mutex_lock(&(table->mutex));
    for (i = 0; i < MAX_IN_FLIGHT_MESSAGE; ++i) {
        if (table->vault[i].free) {
            table->vault[i].free = false;
            table->vault[i].action = action;
            table->vault[i].callback = callback;
            table->vault[i].callbackContext = context;
            table->vault[i].timeout = timeout;
            time(&(table->vault[i].timestamp));
            strncpy(table->vault[i].requestId, requestId, MAX_UUID_LENGTH);
            rc = SUCCESS;
            break;
        }
    }
    pthread_mutex_unlock(&(table->mutex));

    return rc;
}

bool device_management_is_connected(DeviceManagementClient client) {
    if (client == NULL) {
        exit_null_pointer();
        return false;
    }

    device_management_client_t *c = client;

    return device_management_is_connected2(c);
}

bool device_management_is_connected2(device_management_client_t *c) {
    if (c->mqttClient == NULL) {
        return false;
    }

    return MQTTAsync_isConnected(c->mqttClient) && c->hasSubscribed ? true : false;
}

DmReturnCode device_management_shadow_send_json(device_management_client_t *c, const char *topic,
                                                const char *requestId, cJSON *payload) {
    DmReturnCode dmrc = SUCCESS;
    MQTTAsync_message message = MQTTAsync_message_initializer;
    MQTTAsync_responseOptions *responseOptions;
    char *string;
    int rc;

    cJSON_AddStringToObject(payload, REQUEST_ID_KEY, requestId);
    string = cJSON_PrintUnformatted(payload);

    message.payload = string;
    message.payloadlen = strlen(string) + 1;
    message.qos = QOS;
    message.retained = 0;

    responseOptions = malloc(sizeof(MQTTAsync_responseOptions));
    responseOptions->onSuccess = mqtt_on_publish_success;
    responseOptions->onFailure = mqtt_on_publish_failure;
    responseOptions->context = responseOptions;

    rc = MQTTAsync_sendMessage(c->mqttClient, topic, &message, responseOptions);
    if (rc != MQTTASYNC_SUCCESS) {
        log4c_category_log(category, LOG4C_PRIORITY_ERROR, "failed to send message. MQTT rc=%d, requestId=%s.", rc,
                           requestId);
        dmrc = FAILURE;
    } else {
        log4c_category_log(category, LOG4C_PRIORITY_TRACE, "sending message \n>>topic:\n%s\n>>payload:\n%s",
                           topic,
                           string);

    }
    free(string);

    return dmrc;
}

DmReturnCode device_management_shadow_send(DeviceManagementClient client, ShadowAction action, cJSON *payload,
                                           ShadowActionCallback callback,
                                           void *context, uint8_t timeout) {
    const char *topic;

    DmReturnCode rc;
    device_management_client_t *c = client;

    uuid_t uuid;
    char requestId[MAX_UUID_LENGTH];
    uuid_generate(uuid);
    uuid_unparse(uuid, requestId);

    if (!device_management_is_connected2(c)) {
        log4c_category_log(category, LOG4C_PRIORITY_WARN, "can't send message to server when client is not connected.");
        return NOT_CONNECTED;
    }

    if (action == SHADOW_UPDATE) {
        topic = client->topicContract->update;
    } else if (action == SHADOW_GET) {
        topic = client->topicContract->get;
    } else if (action == SHADOW_DELETE) {
        topic = client->topicContract->delete;
    } else {
        log4c_category_log(category, LOG4C_PRIORITY_ERROR, "Unsupported action.");
        return BAD_ARGUMENT;
    }

    rc = in_flight_message_add(&(c->messages), requestId, action, callback, context, timeout);
    if (rc == SUCCESS) {
        device_management_shadow_send_json(c, topic, requestId, payload);
    }

    return rc;
}

int device_management_shadow_handle_response(device_management_client_t *c, const char *requestId, ShadowAction action,
                                             ShadowAckStatus status,
                                             cJSON *payload) {
    int rc = NO_MATCHING_IN_FLIGHT_MESSAGE;
    int i;
    ShadowActionAck ack;

    pthread_mutex_lock(&(c->messages.mutex));
    for (i = 0; i < MAX_IN_FLIGHT_MESSAGE; ++i) {
        if (!c->messages.vault[i].free &&
            strncasecmp(c->messages.vault[i].requestId, requestId, MAX_UUID_LENGTH) == 0) {
            if (status == SHADOW_ACK_ACCEPTED) {
                ack.accepted.response.reported = cJSON_GetObjectItemCaseSensitive(payload, "reported");
                ack.accepted.response.desired = cJSON_GetObjectItemCaseSensitive(payload, "desired");
                cJSON *lastUpdatedTime = cJSON_GetObjectItemCaseSensitive(payload, "lastUpdatedTime");
                if (lastUpdatedTime != NULL) {
                    ack.accepted.response.lastUpdatedTime.reported = cJSON_GetObjectItemCaseSensitive(lastUpdatedTime,
                                                                                                      "reported");
                    ack.accepted.response.lastUpdatedTime.desired = cJSON_GetObjectItemCaseSensitive(lastUpdatedTime,
                                                                                                     "desired");
                }
                cJSON *profileVersion = cJSON_GetObjectItemCaseSensitive(payload, "profileVersion");
                if (profileVersion != NULL) {
                    ack.accepted.response.profileVersion = cJSON_GetObjectItemCaseSensitive(payload,
                                                                                            "profileVersion")->valueint;
                }
            } else if (status == SHADOW_ACK_REJECTED) {
                cJSON *code = cJSON_GetObjectItem(payload, CODE_KEY);
                cJSON *message = cJSON_GetObjectItem(payload, MESSAGE_KEY);
                if (code == NULL || message == NULL) {
                    log4c_category_log(category, LOG4C_PRIORITY_WARN, "bad rejected message.");
                    ack.rejected.code = NULL;
                    ack.rejected.message = NULL;
                } else {
                    ack.rejected.code = code->valuestring;
                    ack.rejected.message = message->valuestring;
                }
            }
            c->messages.vault[i].callback(action, status, &ack, c->messages.vault[i].callbackContext);
            c->messages.vault[i].free = true;
            rc = SUCCESS;
            break;
        }
    }
    pthread_mutex_unlock(&(c->messages.mutex));

    if (rc == NO_MATCHING_IN_FLIGHT_MESSAGE) {
        log4c_category_log(category, LOG4C_PRIORITY_WARN, "no in flight payload matching %s.", requestId);
    }
    return rc;
}

DmReturnCode device_management_delta_arrived(device_management_client_t *c, cJSON *payload) {
    uint32_t i = 0;
    UserDefinedError *error = NULL;
    cJSON *desired;
    cJSON *property;

    const char *requestId = message_get_request_id(payload);
    log4c_category_log(category, LOG4C_PRIORITY_DEBUG, "received delta. requestId=%s.", requestId);
    desired = cJSON_GetObjectItemCaseSensitive(payload, "desired");

    pthread_mutex_lock(&(c->properties.mutex));
    for (i = 0; i < c->properties.index; ++i) {
        if (c->properties.vault[i].key == NULL) {
            error = c->properties.vault[i].cb(NULL, desired);
        } else {
            property = cJSON_GetObjectItemCaseSensitive(desired, c->properties.vault[i].key);
            if (property != NULL) {
                error = c->properties.vault[i].cb(c->properties.vault[i].key, property);
            }
        }

        if (error != NULL) {
            break;
        }
    }

    pthread_mutex_unlock(&(c->properties.mutex));

    if (error != NULL) {
        cJSON *responsePayload = cJSON_CreateObject();
        cJSON_AddStringToObject(responsePayload, CODE_KEY, error->code);
        cJSON_AddStringToObject(responsePayload, MESSAGE_KEY, error->message);
        device_management_shadow_send_json(c, c->topicContract->deltaRejected, requestId, responsePayload);
        if (error->destroyer != NULL) {
            error->destroyer(error);
        }
        cJSON_Delete(responsePayload);
    }

    return SUCCESS;
}

void device_management_set_error(device_management_client_t *c, MQTTAsync_failureData *response) {
    pthread_mutex_lock(&(c->mutex));
    safe_free(&(c->errorMessage));
    if (response != NULL) {
        c->errorCode = response->code;
        c->errorMessage = strdup(response->message);
        log4c_category_log(category, LOG4C_PRIORITY_ERROR, "MQTT async failure. code=%d, message=%s.", response->code,
                           response->message);
    } else {
        c->errorCode = 0;
    }
    pthread_mutex_unlock(&(c->mutex));
}

void mqtt_on_connected(void *context, char *cause) {
    int i;
    int rc;
    MQTTAsync_responseOptions responseOptions;
    responseOptions.onSuccess = mqtt_on_subscribe_success;
    responseOptions.onFailure = mqtt_on_subscribe_failure;
    responseOptions.context = context;
    device_management_client_t *c = context;

    log4c_category_log(category, LOG4C_PRIORITY_INFO, "MQTT connected.");

    // TODO: on-demand subscribe
    int qos[SUB_TOPIC_COUNT];
    for (i = 0; i < SUB_TOPIC_COUNT; ++i) {
        qos[i] = 1;
    }
    rc = MQTTAsync_subscribeMany(c->mqttClient, SUB_TOPIC_COUNT, c->topicContract->subTopics, qos, &responseOptions);
    if (rc != MQTTASYNC_SUCCESS) {
        log4c_category_log(category, LOG4C_PRIORITY_ERROR, "Failed to subscribe. MQTT rc=%d.", rc);
        return;
    }
//    rc = MQTTAsync_waitForCompletion(c->mqttClient, responseOptions.token, SUBSCRIBE_TIMEOUT * 1000);
//    if (rc != MQTTASYNC_SUCCESS) {
//        log4c_category_log(category, LOG4C_PRIORITY_ERROR, "subscribe failed. MQTT rc=%d.", rc);
//    } else {
//        pthread_mutex_lock(&(c->mutex));
//        c->hasSubscribed = true;
//        pthread_mutex_unlock(&(c->mutex));
//        log4c_category_log(category, LOG4C_PRIORITY_DEBUG, "MQTT subscribed.");
//    }
}

void mqtt_on_connection_lost(void *context, char *cause) {
    log4c_category_log(category, LOG4C_PRIORITY_ERROR, "connection lost. cause=%s.", cause);
    device_management_client_t *c = context;
    pthread_mutex_lock(&(c->mutex));
    c->hasSubscribed = false;
    pthread_mutex_unlock(&(c->mutex));
}

void mqtt_on_connect_success(void *context, MQTTAsync_successData *response) {
    device_management_client_t *c = context;
    device_management_set_error(c, NULL);
}

void mqtt_on_connect_failure(void *context, MQTTAsync_failureData *response) {
    device_management_client_t *c = context;
    device_management_set_error(c, response != NULL ? response : &UNKNOWN_FAILURE);
}

void mqtt_on_subscribe_success(void* context, MQTTAsync_successData* response) {
    device_management_client_t *c = context;
    pthread_mutex_lock(&(c->mutex));
    c->hasSubscribed = true;
    pthread_mutex_unlock(&(c->mutex));
    device_management_set_error(c, NULL);
    log4c_category_log(category, LOG4C_PRIORITY_DEBUG, "MQTT subscribed.");
}

void mqtt_on_subscribe_failure(void* context,  MQTTAsync_failureData* response) {
    device_management_client_t *c = context;
    device_management_set_error(c, response != NULL ? response : &UNKNOWN_FAILURE);
}

void mqtt_on_publish_success(void *context, MQTTAsync_successData *response) {
    free(context);
}

void mqtt_on_publish_failure(void *context, MQTTAsync_failureData *response) {
    log4c_category_log(category, LOG4C_PRIORITY_ERROR, "failed to send json. code=%d, message=%s.",
                       response->code, response->message);
    free(context);
}

void mqtt_on_delivery_complete(void *context, MQTTAsync_token dt) {
    // Nothing to do.
}

int mqtt_on_message_arrived(void *context, char *topicName, int topicLen, MQTTAsync_message *message) {
    device_management_client_t *c = context;
    char *jsonString = message->payload;
    if (message->payloadlen < 3) {
        return -1;
    }
    if (jsonString[message->payloadlen - 1] != '\0') {
        // Make a copy
        jsonString = malloc(message->payloadlen + 1);
        strncpy(jsonString, message->payload, message->payloadlen);
        jsonString[message->payloadlen] = '\0';
    }
    log4c_category_log(category, LOG4C_PRIORITY_TRACE, "received message \n<<topic:\n%s\n<<payload:\n%s",
                       topicName, jsonString);
    cJSON *payload = cJSON_Parse(jsonString);
    if (jsonString != message->payload) {
        free(jsonString);
    }

    if (payload == NULL) {
        /* json parsing failed. */
        log4c_category_log(category, LOG4C_PRIORITY_WARN, "failed to parse mqtt message as json. string is:\n%s",
                           jsonString);
    } else {
        ShadowAction action = SHADOW_INVALID;
        ShadowAckStatus status = SHADOW_ACK_ACCEPTED;

        if (strncasecmp(c->topicContract->delta, topicName, strlen(c->topicContract->delta)) == 0) {
            device_management_delta_arrived(c, payload);
        } else {
            if (strncasecmp(c->topicContract->updateAccepted, topicName, strlen(c->topicContract->updateAccepted)) ==
                0) {
                action = SHADOW_UPDATE;
            } else if (strncasecmp(c->topicContract->updateRejected, topicName,
                                   strlen(c->topicContract->updateRejected)) == 0) {
                action = SHADOW_UPDATE;
                status = SHADOW_ACK_REJECTED;
            } else if (strncasecmp(c->topicContract->getAccepted, topicName, strlen(c->topicContract->getAccepted)) ==
                       0) {
                action = SHADOW_GET;
            } else if (strncasecmp(c->topicContract->getRejected, topicName, strlen(c->topicContract->getRejected)) ==
                       0) {
                action = SHADOW_GET;
                status = SHADOW_ACK_REJECTED;
            } else if (strncasecmp(c->topicContract->deleteAccepted, topicName, strlen(c->topicContract->deleteAccepted)) ==
                       0) {
                action = SHADOW_DELETE;
            } else if (strncasecmp(c->topicContract->deleteRejected, topicName, strlen(c->topicContract->deleteRejected)) ==
                       0) {
                action = SHADOW_DELETE;
                status = SHADOW_ACK_REJECTED;
            } else {
                log4c_category_log(category, LOG4C_PRIORITY_ERROR, "Unexpected topic %s.", topicName);
            }

            if (action != SHADOW_INVALID) {
                cJSON *requestId = cJSON_GetObjectItem(payload, REQUEST_ID_KEY);

                if (requestId == NULL) {
                    log4c_category_log(category, LOG4C_PRIORITY_ERROR, "cannot find request id.");
                } else {

                    device_management_shadow_handle_response(c, requestId->valuestring, action, status, payload);
                }
            }
        }
    }

    cJSON_Delete(payload);
    MQTTAsync_freeMessage(&message);
    MQTTAsync_free(topicName);

    return true;
}

