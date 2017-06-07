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
#include "device_management.h"
#include "device_management_conf.h"

#define _GNU_SOURCE

#include <uuid/uuid.h>
#include <MQTTAsync.h>
#include <log4c.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <zconf.h>
#include <stdio.h>

static const char *log4c_category_name = "device_management";

static const char *REQUEST_ID_KEY = "requestId";

static const char *CODE_KEY = "code";

static const char *MESSAGE_KEY = "message";

static const char *REPORTED = "reported";

static log4c_category_t *category = NULL;

#define SUB_TOPIC_COUNT 5

typedef struct {
    char *update;
    char *updateAccepted;
    char *updateRejected;
    char *get;
    char *getAccepted;
    char *getRejected;
    char *delta;
    char *deltaRejected;
    char *subTopics[SUB_TOPIC_COUNT];
} TopicContract;

typedef struct {
    ShadowProperty property;
} PropertyTableElement;

typedef struct {
    PropertyTableElement vault[MAX_SHADOW_PROPERTY];
    int index;
    // This data ia accessed from MQTT client's callback.
    pthread_mutex_t mutex;
} PropertyTable;

#define REQUEST_ID_LENGTH 64

typedef struct {
    char requestId[REQUEST_ID_LENGTH];
    ShadowAction action;
    ShadowActionCallback callback;
    void *callbackContext;
    time_t timestamp;
    uint8_t timeout;
    bool free;
} InFlightRequest;

typedef struct {
    InFlightRequest vault[MAX_IN_FLIGHT_REQUEST];
    // This data ia accessed from MQTT client's callback.
    pthread_mutex_t mutex;
} InFlightRequestTable;

typedef struct device_management_client_t {
    MQTTAsync mqttClient;
    int errorCode;
    char *errorMessage;
    bool hasSubscribed;
    char *username;
    char *password;
    char *deviceName;
    TopicContract *topicContract;
    PropertyTable properties;
    InFlightRequestTable requests;
    timer_t timer;
    pthread_mutex_t mutex;
} device_management_client_t;

typedef struct {
    device_management_client_t *members[MAX_CLIENT];
    pthread_mutex_t mutex;
} ClientGroup;

static ClientGroup allClients;

static pthread_t requestKeeper;

static void
exit_null_pointer();

static void
check_malloc_result(void *address);

static void
safeFree(char **pointer);

static TopicContract *
topic_contract_create(const char *deviceName);

static void
topic_contract_destroy(TopicContract *topics);

static bool
client_group_add(ClientGroup *group, device_management_client_t *client);

static bool
client_group_remove(ClientGroup *group, device_management_client_t *client);

static void
in_flight_request_house_keep_handle(device_management_client_t *c);

static void *
in_flight_request_house_keep(void *ignore);

static const char *
request_get_request_id(const cJSON *root);

static bool
device_management_is_connected(DeviceManagementClient client);

static bool
device_management_is_connected2(device_management_client_t *c);

static DmReturnCode
device_management_shadow_send_json(device_management_client_t *c, const char *topic,
                                   const char *requestId, cJSON *payload);

static DmReturnCode
device_management_shadow_send_request(DeviceManagementClient client, ShadowAction action, cJSON *payload,
                                      ShadowActionCallback callback,
                                      void *context, uint8_t timeout);

static int
device_management_shadow_handle_response(device_management_client_t *c, const char *requestId, ShadowAction action,
                                         ShadowAckStatus status,
                                         cJSON *root);

static DmReturnCode
device_management_delta_arrived(device_management_client_t *c, cJSON *root);

static void
mqtt_on_connected(void *context, char *cause);

static void
mqtt_on_connection_lost(void *context, char *cause);

static void
mqtt_on_connect_success(void *context, MQTTAsync_successData *response);

static void
mqtt_on_connect_failure(void *context, MQTTAsync_failureData *response);

static void
mqtt_on_publish_success(void *context, MQTTAsync_successData *response);

static void
mqtt_on_publish_failure(void *context, MQTTAsync_failureData *response);

static int
mqtt_on_message_arrived(void *context, char *topicName, int topicLen, MQTTAsync_message *message);

static void
mqtt_on_delivery_complete(void *context, MQTTAsync_token token);

DmReturnCode
device_management_init() {
    if (log4c_init()) {
        fprintf(stderr, "log4c init failed.\n");
        return FAILURE;
    }

    category = log4c_category_new(log4c_category_name);

    log4c_category_log(category, LOG4C_PRIORITY_INFO, "initialized.");
    // Dump current log4c configuration in case you can't find log output anywhere.
//    FILE *fd = fopen("log4c.txt", "w");
//    log4c_dump_all_instances(fd);
//    fclose(fd);

    pthread_mutex_init(&(allClients.mutex), NULL);
    pthread_create(&requestKeeper, NULL, in_flight_request_house_keep, NULL);

    return SUCCESS;
}

DmReturnCode
device_management_fini() {
    // Destroy log4c.
    if (category != NULL) {
        log4c_category_delete(category);
    }
    log4c_fini();
    return SUCCESS;
}

DmReturnCode
device_management_create(DeviceManagementClient *client, const char *broker, const char *deviceName,
                         const char *username, const char *password) {
    int rc;
    int i;

    device_management_client_t *c = malloc(sizeof(device_management_client_t));

    // TODO: validate arguments.

    rc = MQTTAsync_create(&(c->mqttClient), broker, deviceName, MQTTCLIENT_PERSISTENCE_NONE, NULL);

    if (rc == EXIT_FAILURE) {
        log4c_category_log(category, LOG4C_PRIORITY_ERROR, "Failed to create. rc=%d.", rc);
        free(c);
        return FAILURE;
    }

    MQTTAsync_setCallbacks(c->mqttClient, c, mqtt_on_connection_lost, mqtt_on_message_arrived,
                           mqtt_on_delivery_complete);
    MQTTAsync_setConnected(c->mqttClient, c, mqtt_on_connected);

    c->errorMessage = NULL;
    c->username = strdup(username);
    c->password = strdup(password);
    c->deviceName = strdup(deviceName);
    // TODO: check rc for below statements.
    c->topicContract = topic_contract_create(deviceName);
    // TODO: set magic to c
    // TODO: set context.
    c->properties.index = 0;
    pthread_mutex_init(&(c->properties.mutex), NULL);
    for (i = 0; i < MAX_IN_FLIGHT_REQUEST; ++i) {
        c->requests.vault[i].free = true;
    }
    pthread_mutex_init(&(c->requests.mutex), NULL);
    pthread_mutex_init(&(c->mutex), NULL);
    *client = c;
    client_group_add(&allClients, c);

    log4c_category_log(category, LOG4C_PRIORITY_INFO, "created. broker=%s, deviceName=%s.",
                       broker, deviceName);
    return SUCCESS;
}

DmReturnCode
device_management_connect(DeviceManagementClient client) {
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

    log4c_category_log(category, LOG4C_PRIORITY_INFO, "connecting to server.");
    rc = MQTTAsync_connect(c->mqttClient, &connectOptions);
    if (rc != MQTTASYNC_SUCCESS) {
        log4c_category_log(category, LOG4C_PRIORITY_ERROR, "Failed to start connecting. rc=%d.", rc);
        return FAILURE;
    }

    while (!MQTTAsync_isConnected(c->mqttClient) && c->errorMessage == NULL) {
        sleep(1);
    }

    if (MQTTAsync_isConnected(c->mqttClient)) {
        log4c_category_log(category, LOG4C_PRIORITY_INFO, "MQTT connected.");
        return SUCCESS;
    } else if (c->errorMessage != NULL) {
        log4c_category_log(category, LOG4C_PRIORITY_ERROR, "MQTT connect failed. code=%d, message=%s.", c->errorCode,
                           c->errorMessage);
        return FAILURE;
    }
}

DmReturnCode
device_management_shadow_update(DeviceManagementClient client, cJSON *reported, ShadowActionCallback callback,
                                void *context, uint8_t timeout) {
    DmReturnCode rc;

    cJSON *root = cJSON_CreateObject();

    cJSON_AddItemToObject(root, REPORTED, reported);

    rc = device_management_shadow_send_request(client, SHADOW_UPDATE, root, callback, context, timeout);

    cJSON_DetachItemViaPointer(root, reported);
    cJSON_Delete(root);

    if (rc != SUCCESS) {
        log4c_category_log(category, LOG4C_PRIORITY_ERROR, "device_management_shadow_update rc=%d", rc);
    }
    return rc;
}

DmReturnCode
device_management_shadow_get(DeviceManagementClient client, ShadowActionCallback callback, void *context,
                             uint8_t timeout) {
    DmReturnCode rc;
    cJSON *root = cJSON_CreateObject();

    rc = device_management_shadow_send_request(client, SHADOW_GET, root, callback, context, timeout);

    cJSON_Delete(root);

    if (rc != SUCCESS) {
        log4c_category_log(category, LOG4C_PRIORITY_ERROR, "device_management_shadow_get rc=%d", rc);
    }
    return rc;
}

DmReturnCode
device_management_shadow_register_delta(DeviceManagementClient client, ShadowProperty *shadowProperty) {
    DmReturnCode rc = SUCCESS;

    if (client == NULL || shadowProperty == NULL || shadowProperty->cb == NULL) {
        exit_null_pointer();
    }

    if (!device_management_is_connected(client)) {
        return NOT_CONNECTED;
    }

    device_management_client_t *c = client;

    pthread_mutex_lock(&(c->properties.mutex));
    if (c->properties.index >= MAX_SHADOW_PROPERTY) {
        rc = TOO_MANY_PROPERTY;
    } else {
        c->properties.vault[c->properties.index].property.key = shadowProperty->key;
        c->properties.vault[c->properties.index].property.cb = shadowProperty->cb;
        c->properties.index++;
    }
    pthread_mutex_unlock(&(c->properties.mutex));

    if (rc != SUCCESS) {
        log4c_category_log(category, LOG4C_PRIORITY_ERROR, "device_management_shadow_register_delta rc=%d", rc);
    }
    return rc;
}

DmReturnCode device_management_destroy(DeviceManagementClient client) {
    device_management_client_t *c = client;
    if (c == NULL) {
        log4c_category_log(category, LOG4C_PRIORITY_ERROR, "bad client.");
    } else {
        client_group_remove(&allClients, c);
        safeFree(&(c->username));
        safeFree(&(c->password));
        safeFree(&(c->deviceName));
        safeFree(&(c->errorMessage));
        free(c->errorMessage);
        MQTTAsync_disconnect(c->mqttClient, NULL);
        MQTTAsync_destroy(&(c->mqttClient));
        topic_contract_destroy(c->topicContract);
        free(client);
    }

    return SUCCESS;
}

void
check_malloc_result(void *address) {
    if (address == NULL) {
        log4c_category_log(category, LOG4C_PRIORITY_FATAL, "malloc failure");
        exit(EXIT_FAILURE);
    }
}

void safeFree(char **pointer) {
    if (pointer != NULL && *pointer != NULL) {
        free(*pointer);
        *pointer = NULL;
    }
}

TopicContract *
topic_contract_create(const char *deviceName) {
    int rc;
    TopicContract *t = malloc(sizeof(TopicContract));
    check_malloc_result(t);

    rc = asprintf(&(t->update), "baidu/iot/shadow/%s/update", deviceName);
    rc = asprintf(&(t->updateAccepted), "baidu/iot/shadow/%s/update/accepted", deviceName);
    rc = asprintf(&(t->updateRejected), "baidu/iot/shadow/%s/update/rejected", deviceName);
    t->subTopics[0] = t->updateAccepted;
    t->subTopics[1] = t->updateRejected;

    rc = asprintf(&(t->get), "baidu/iot/shadow/%s/get", deviceName);
    rc = asprintf(&(t->getAccepted), "baidu/iot/shadow/%s/get/accepted", deviceName);
    rc = asprintf(&(t->getRejected), "baidu/iot/shadow/%s/get/rejected", deviceName);
    t->subTopics[2] = t->getAccepted;
    t->subTopics[3] = t->getRejected;

    rc = asprintf(&(t->delta), "baidu/iot/shadow/%s/delta", deviceName);
    rc = asprintf(&(t->deltaRejected), "baidu/iot/shadow/%s/delta/rejected", deviceName);
    t->subTopics[4] = t->delta;
}

void
topic_contract_destroy(TopicContract *topics) {
    if (topics != NULL) {
        safeFree(&(topics->update));
        safeFree(&(topics->updateAccepted));
        safeFree(&(topics->updateRejected));
        safeFree(&(topics->get));
        safeFree(&(topics->getAccepted));
        safeFree(&(topics->getRejected));
        safeFree(&(topics->delta));
        safeFree(&(topics->deltaRejected));
        free(topics);
    }
}

static void
client_group_iterate(ClientGroup *clients, void (*fp)(device_management_client_t *c)) {
    int i;
    pthread_mutex_lock(&(clients->mutex));
    for (i = 0; i < MAX_CLIENT; ++i) {
        if (clients->members[i] != NULL) {
            fp(clients->members[i]);
        }
    }
    pthread_mutex_unlock(&(clients->mutex));
}

bool
client_group_add(ClientGroup *group, device_management_client_t *client) {
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

bool
client_group_remove(ClientGroup *group, device_management_client_t *client) {
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


void
in_flight_request_house_keep_handle(device_management_client_t *c) {
    int i;
    time_t now;
    time(&now);
    pthread_mutex_lock(&(c->requests.mutex));
    for (i = 0; i < MAX_IN_FLIGHT_REQUEST; ++i) {
        if (!c->requests.vault[i].free) {
            long elipse = difftime(now, c->requests.vault[i].timestamp);
            if (elipse > c->requests.vault[i].timeout) {
                if (c->requests.vault[i].callback != NULL) {
                    c->requests.vault[i].callback(c->requests.vault[i].action, SHADOW_ACK_TIMEOUT, NULL,
                                                  c->requests.vault[i].callbackContext);
                }
                c->requests.vault[i].free = true;
            }
        }
    }

    pthread_mutex_unlock(&(c->requests.mutex));

}

void *
in_flight_request_house_keep(void *ignore) {
    while (1) {
        client_group_iterate(&allClients, in_flight_request_house_keep_handle);
        sleep(1);
    }
}

const char *
request_get_request_id(const cJSON *root) {
    cJSON *requestId = cJSON_GetObjectItemCaseSensitive(root, "requestId");
    return requestId->valuestring;
}

void
exit_null_pointer() {
    log4c_category_log(category, LOG4C_PRIORITY_ERROR, "NULL POINTER");
    exit(NULL_POINTER);
}

DmReturnCode
in_flight_request_add(InFlightRequestTable *table, const char *requestId, ShadowAction action,
                      ShadowActionCallback callback,
                      void *context, uint8_t timeout) {
    int rc = TOO_MANY_REQUEST;
    int i;

    pthread_mutex_lock(&(table->mutex));
    for (i = 0; i < MAX_IN_FLIGHT_REQUEST; ++i) {
        if (table->vault[i].free) {
            table->vault[i].free = false;
            table->vault[i].action = action;
            table->vault[i].callback = callback;
            table->vault[i].callbackContext = context;
            table->vault[i].timeout = timeout;
            time(&(table->vault[i].timestamp));
            strncpy(table->vault[i].requestId, requestId, REQUEST_ID_LENGTH);
            rc = SUCCESS;
            break;
        }
    }
    pthread_mutex_unlock(&(table->mutex));

    return rc;
}

bool
device_management_is_connected(DeviceManagementClient client) {
    if (client == NULL) {
        exit_null_pointer();
        return false;
    }

    device_management_client_t *c = client;

    return device_management_is_connected2(c);
}

bool
device_management_is_connected2(device_management_client_t *c) {
    if (c->mqttClient == NULL) {
        return false;
    }

    return MQTTAsync_isConnected(c->mqttClient) && c->hasSubscribed ? true : false;
}

DmReturnCode
device_management_shadow_send_json(device_management_client_t *c, const char *topic,
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
        log4c_category_log(category, LOG4C_PRIORITY_ERROR, "failed to send request. rc=%d, requestId=%s.", rc,
                           requestId);
        dmrc = FAILURE;
    } else {
        log4c_category_log(category, LOG4C_PRIORITY_TRACE, "\n[>>>>>>\ntopic:\n%s\npayload:\n%s\n >>>>>>]", topic,
                           string);

    }
    free(string);

    return dmrc;
}

DmReturnCode
device_management_shadow_send_request(DeviceManagementClient client, ShadowAction action, cJSON *payload,
                                      ShadowActionCallback callback,
                                      void *context, uint8_t timeout) {
    const char *topic;

    int rc;
    device_management_client_t *c = client;

    uuid_t uuid;
    char requestId[REQUEST_ID_LENGTH];
    uuid_generate(uuid);
    uuid_unparse(uuid, requestId);

    if (action == SHADOW_UPDATE) {
        topic = client->topicContract->update;
    } else if (action == SHADOW_GET) {
        topic = client->topicContract->get;
    } else if (action == SHADOW_DELETE) {
        topic = client->topicContract->delta;
    } else {
        log4c_category_log(category, LOG4C_PRIORITY_ERROR, "Unsupported action.");
        return BAD_ARGUMENTS;
    }

    rc = in_flight_request_add(&(c->requests), requestId, action, callback, context, timeout);
    if (rc != SUCCESS) {
        return rc;
    }

    device_management_shadow_send_json(c, topic, requestId, payload);

    return SUCCESS;
}

int
device_management_shadow_handle_response(device_management_client_t *c, const char *requestId, ShadowAction action,
                                         ShadowAckStatus status,
                                         cJSON *root) {
    int rc = NO_MATCHING_IN_FLIGHT_REQUEST;
    int i;
    ShadowActionAck ack;

    pthread_mutex_lock(&(c->requests.mutex));
    for (i = 0; i < MAX_IN_FLIGHT_REQUEST; ++i) {
        if (!c->requests.vault[i].free &&
            strncasecmp(c->requests.vault[i].requestId, requestId, REQUEST_ID_LENGTH) == 0) {
            if (status == SHADOW_ACK_ACCEPTED) {
                ack.accepted.document = root;
            } else if (status == SHADOW_ACK_REJECTED) {
                ack.rejected.code = cJSON_GetObjectItem(root, CODE_KEY)->valuestring;
                ack.rejected.message = cJSON_GetObjectItem(root, MESSAGE_KEY)->valuestring;
            }
            c->requests.vault[i].callback(action, status, &ack, c->requests.vault[i].callbackContext);
            c->requests.vault[i].free = true;
            rc = SUCCESS;
            break;
        }
    }
    pthread_mutex_unlock(&(c->requests.mutex));

    if (rc == NO_MATCHING_IN_FLIGHT_REQUEST) {
        log4c_category_log(category, LOG4C_PRIORITY_WARN, "no in flight request matching %s.", requestId);
    }
    return rc;
}

DmReturnCode
device_management_delta_arrived(device_management_client_t *c, cJSON *root) {
    uint32_t i = 0;
    UserDefinedError *error = NULL;
    cJSON *desired;
    cJSON *property;

    const char *requestId = request_get_request_id(root);
    log4c_category_log(category, LOG4C_PRIORITY_DEBUG, "received delta. requestId=%s.", requestId);
    desired = cJSON_GetObjectItemCaseSensitive(root, "desired");

    pthread_mutex_lock(&(c->properties.mutex));
    for (i = 0; i < c->properties.index; ++i) {
        if (c->properties.vault[i].property.key == NULL) {
            error = c->properties.vault[i].property.cb(NULL, desired);
        } else {
            property = cJSON_GetObjectItemCaseSensitive(desired, c->properties.vault[i].property.key);
            if (property != NULL) {
                error = c->properties.vault[i].property.cb(c->properties.vault[i].property.key, property);
            }
        }

        if (error != NULL) {
            break;
        }
    }

    pthread_mutex_unlock(&(c->properties.mutex));

    if (error != NULL) {
        cJSON *request = cJSON_CreateObject();
        cJSON_AddStringToObject(request, CODE_KEY, error->code);
        cJSON_AddStringToObject(request, MESSAGE_KEY, error->message);
        device_management_shadow_send_json(c, c->topicContract->deltaRejected, requestId, request);
        if (error->destroyer != NULL) {
            error->destroyer(error);
        }
        cJSON_Delete(request);
    }

    return SUCCESS;
}

void
mqtt_on_connected(void *context, char *cause) {
    int rc;
    MQTTAsync_responseOptions responseOptions;
    responseOptions.onSuccess = NULL;
    responseOptions.onFailure = NULL;
    device_management_client_t *c = context;
    // TODO: on-demand subscribe
    int qos[SUB_TOPIC_COUNT];
    for (int i = 0; i < SUB_TOPIC_COUNT; ++i) {
        qos[i] = 1;
    }
    rc = MQTTAsync_subscribeMany(c->mqttClient, SUB_TOPIC_COUNT, c->topicContract->subTopics, qos, &responseOptions);
    if (rc != MQTTASYNC_SUCCESS) {
        log4c_category_log(category, LOG4C_PRIORITY_ERROR, "Failed to subscribe. rc=%d.", rc);
        return;
    }
    MQTTAsync_waitForCompletion(c->mqttClient, responseOptions.token, SUBSCRIBE_TIMEOUT * 1000);
    pthread_mutex_lock(&(c->mutex));
    c->hasSubscribed = true;
    pthread_mutex_unlock(&(c->mutex));
    log4c_category_log(category, LOG4C_PRIORITY_DEBUG, "MQTT subscribed.");
}

void mqtt_on_connection_lost(void *context, char *cause) {
    log4c_category_log(category, LOG4C_PRIORITY_ERROR, "connection lost. cause=%s.", cause);
    device_management_client_t *c = context;
    pthread_mutex_lock(&(c->mutex));
    c->hasSubscribed = false;
    pthread_mutex_unlock(&(c->mutex));
    // TODO: I've set auto-connect to true. Should I manually re-connect here?
}

void mqtt_on_connect_success(void *context, MQTTAsync_successData *response) {
    device_management_client_t *c = context;
    if (c->errorMessage != NULL) {
        free(c->errorMessage);
        c->errorMessage = NULL;
    }
}

void mqtt_on_connect_failure(void *context, MQTTAsync_failureData *response) {
    device_management_client_t *c = context;
    if (c->errorMessage != NULL) {
        free(c->errorMessage);
    }
    c->errorCode = response->code;
    c->errorMessage = strdup(response->message);
}

void
mqtt_on_publish_success(void *context, MQTTAsync_successData *response) {
    free(context);
}

void
mqtt_on_publish_failure(void *context, MQTTAsync_failureData *response) {
    log4c_category_log(category, LOG4C_PRIORITY_ERROR, "failed to send json. code=%d, message=%s.",
                       response->code, response->message);
    free(context);
}

void
mqtt_on_delivery_complete(void *context, MQTTAsync_token dt) {
    // TODO: anything to do?
    device_management_client_t *c = context;
}

int
mqtt_on_message_arrived(void *context, char *topicName, int topicLen, MQTTAsync_message *message) {
    device_management_client_t *c = context;
    char *json = message->payload;
    if (message->payloadlen < 3) {
        return -1;
    }
    if (json[message->payloadlen - 1] != '\0') {
        // Make a copy
        json = malloc(message->payloadlen + 1);
        strncpy(json, message->payload, message->payloadlen);
        json[message->payloadlen] = '\0';
    }
    log4c_category_log(category, LOG4C_PRIORITY_TRACE, "\n[<<<<<<\ntopic:\n%s\npayload:\n%s\n <<<<<<]",
                       topicName, json);
    cJSON *root = cJSON_Parse(json);
    if (json != message->payload) {
        free(json);
    }
    ShadowAckStatus status = SHADOW_ACK_ACCEPTED;
    ShadowAction action = SHADOW_NULL;

    if (strncasecmp(c->topicContract->delta, topicName, strlen(c->topicContract->delta)) == 0) {
        device_management_delta_arrived(c, root);
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
        } else {
            log4c_category_log(category, LOG4C_PRIORITY_ERROR, "Unexpected topic %s.", topicName);
        }

        if (action != SHADOW_NULL) {
            cJSON *requestId = cJSON_GetObjectItem(root, REQUEST_ID_KEY);

            if (requestId == NULL) {
                log4c_category_log(category, LOG4C_PRIORITY_ERROR, "cannot find request id.");
            } else {

                device_management_shadow_handle_response(c, requestId->valuestring, action, status, root);
            }
        }
    }

    cJSON_Delete(root);
    MQTTAsync_freeMessage(&message);
    MQTTAsync_free(topicName);

    return true;
}

