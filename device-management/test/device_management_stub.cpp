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
 * @brief Stub of device management server.
 *
 * @authors Zhao Bo
 */
#include "device_management_stub.h"
#include <regex>
#include <MQTTClient.h>

#include <cjson/cJSON.h>
#include <boost/format.hpp>
#include <mutex>
#include <list>

#include "test_conf.h"
#include "test_util.h"

class DeviceManagementStubImpl : public DeviceManagementStub {
public:
    DeviceManagementStubImpl(const std::string &broker, const std::string &username, const std::string &password,
                             const std::string &clientId);

    virtual ~DeviceManagementStubImpl();

    void start();

    virtual void setAutoResponse(bool value) override;

    virtual void addListener(CallBack f) override;

    virtual void clearListeners() override;

private:
    std::string username;
    std::string password;
    MQTTClient client;

    bool autoRespond;
    std::list<CallBack> callbacks;
    std::mutex callbackMutex;

    static const std::regex topicRegex;
    static const std::string update;
    static const std::string requestIdKey;
    static const std::string acceptedFormat;
    static const std::string rejectedFormat;

    // MQTT callbacks
    static void connection_lost(void *context, char *cause);

    static int message_arrived(void *context, char *topicName, int topicLen, MQTTClient_message *message);

    static void delivery_complete(void *context, MQTTClient_deliveryToken dt);

    // Business logic
    void processUpdate(const std::string &device, const std::string requestId, cJSON *document);
};

// First group matches device name and second group matches action.
const std::regex DeviceManagementStubImpl::topicRegex = std::regex(std::string("\\$baidu/iot/shadow/") + "([^/]*)/([^/]*)");

const std::string DeviceManagementStubImpl::update = "update";

const std::string DeviceManagementStubImpl::requestIdKey = "requestId";

const std::string DeviceManagementStubImpl::acceptedFormat = TestConf::getTopicPrefix() + "%s/%s/accepted";

const std::string DeviceManagementStubImpl::rejectedFormat = TestConf::getTopicPrefix() + "%s/%s/rejected";

DeviceManagementStubImpl::DeviceManagementStubImpl(const std::string &broker, const std::string &username,
                                                   const std::string &password, const std::string &clientId) :
        username(username), password(password) {
    MQTTClient_create(&client, broker.data(), clientId.data(), MQTTCLIENT_PERSISTENCE_NONE, NULL);
    autoRespond = true;
}

DeviceManagementStubImpl::~DeviceManagementStubImpl() {
    if (client != NULL) {
        MQTTClient_disconnect(client, 10);
        MQTTClient_destroy(&client);
    }
}

void DeviceManagementStubImpl::start() {
    MQTTClient_connectOptions options = MQTTClient_connectOptions_initializer;
    options.username = username.data();
    options.password = password.data();
    MQTTClient_setCallbacks(client, this, connection_lost, message_arrived, delivery_complete);
    MQTTClient_connect(client, &options);
    std::string topicFilter = TestConf::getTopicPrefix() + "+/update";
    MQTTClient_subscribe(client, topicFilter.data(), 1);
}

void DeviceManagementStubImpl::setAutoResponse(bool value) {
    autoRespond = value;
}

void DeviceManagementStubImpl::addListener(CallBack f) {
    std::lock_guard<std::mutex> lock(callbackMutex);
    callbacks.push_back(f);
}

void DeviceManagementStubImpl::clearListeners() {
    std::lock_guard<std::mutex> lock(callbackMutex);
    callbacks.clear();
}

int DeviceManagementStubImpl::message_arrived(void *context, char *topicName, int topicLen,
                                              MQTTClient_message *message) {
    DeviceManagementStubImpl *impl = static_cast<DeviceManagementStubImpl *>(context);
    std::cmatch results;
    bool matched = std::regex_match(topicName, results, topicRegex, std::regex_constants::match_default);
    if (matched) {
        const std::string &device = results[1];
        const std::string &action = results[2];
        char *payload = static_cast<char *>(message->payload);
        char *jsonString = payload;
        if (jsonString[message->payloadlen - 1] != '\0') {
            // Make a copy
            jsonString = static_cast<char *>(malloc(message->payloadlen + 1));
            strncpy(jsonString, payload, message->payloadlen);
            jsonString[message->payloadlen] = '\0';
        }

        cJSON *document = cJSON_Parse(jsonString);
        if (jsonString != payload) {
            free(jsonString);
        }
        const std::string requestId(cJSON_GetObjectItem(document, requestIdKey.data())->valuestring);
        if (action == DeviceManagementStubImpl::update) {
            impl->processUpdate(device, requestId, document);
        }

        for (CallBack &callback : impl->callbacks) {
            callback.operator()(device, action);
        }

        cJSON_Delete(document);

    }

    MQTTClient_free(topicName);
    MQTTClient_freeMessage(&message);
    return 1;
}

void DeviceManagementStubImpl::connection_lost(void *context, char *cause) {
    DeviceManagementStubImpl *impl = static_cast<DeviceManagementStubImpl *>(context);
}

void DeviceManagementStubImpl::delivery_complete(void *context, MQTTClient_deliveryToken tok) {

}

void DeviceManagementStubImpl::processUpdate(const std::string &device, const std::string requestId, cJSON *document) {
    if (autoRespond) {
        boost::format format(acceptedFormat);
        std::string topic = boost::str(format % device % update);
        cJSON *json = cJSON_CreateObject();
        cJSON_AddStringToObject(json, "requestId", requestId.data());
        cJSON_AddNumberToObject(json, "profileVersion", 1);
        char *payload = cJSON_Print(json);
        // Send ack
        MQTTClient_publish(client, topic.data(), strlen(payload) + 1, payload, 1, 0, NULL);
        free(payload);
    }
}

std::shared_ptr<DeviceManagementStub> DeviceManagementStub::create() {
    return std::shared_ptr<DeviceManagementStub>(
            new DeviceManagementStubImpl(TestConf::getTestMqttBroker(),
                                         TestConf::getTestMqttUsername(),
                                         TestConf::getTestMqttPassword(),
                                         TestUtil::uuid()));
}
