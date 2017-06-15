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
 * @authors Zhao Bo zhaobo03@baidu.com
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <device_management.h>
#include "device_management_stub.h"
#include "test_conf.h"
#include "test_util.h"

typedef std::function<void(ShadowAction, ShadowAckStatus, ShadowActionAck *, void *)> ActionCallback;

TEST(InitTest, DoubleInit) {
    device_management_init();
    device_management_init();
    device_management_fini();
}

TEST(InitTest, DoubleFini) {
    device_management_init();
    device_management_fini();
    device_management_fini();
}

TEST(ConnectTest, DoubleConnect) {
    DeviceManagementClient client;
    device_management_init();

    device_management_create(&client, TestConf::testMqttBroker.data(), "DoubleConnect",
                             TestConf::testMqttUsername.data(),
                             TestConf::testMqttPassword.data());
    device_management_connect(client);
    device_management_connect(client);
    device_management_fini();
}

class UpdateTest : public ::testing::Test {
protected:
    virtual void SetUp() override;

    virtual void TearDown() override;

    std::shared_ptr<DeviceManagementStub> stub;
};

void UpdateTest::SetUp() {
    stub = DeviceManagementStub::create();
    stub->start();
}

void UpdateTest::TearDown() {
    stub->clearListeners();
}

class Listener {
public:
    virtual void ServerCallBack(const std::string &deviceName, const std::string action) = 0;

    virtual void ClientCallback(ShadowAction action, ShadowAckStatus status, ShadowActionAck *ack, void *context) = 0;
};

class MockListener : public virtual Listener {
public:
    MOCK_METHOD2(ServerCallBack, void(
            const std::string &deviceName,
            const std::string action));

    MOCK_METHOD4(ClientCallback, void(ShadowAction
            action, ShadowAckStatus
            status, ShadowActionAck * ack, void * context));

    int called = 0;
};

TEST_F(UpdateTest, UpdateHappy) {
    device_management_init();
    DeviceManagementClient client;
    std::string seed = TestUtil::uuid();
    std::string deviceName = "UpdateHappy-" + seed;
    device_management_create(&client, TestConf::testMqttBroker.data(), deviceName.data(),
                             TestConf::testMqttUsername.data(), TestConf::testMqttPassword.data());
    device_management_connect(client);

    MockListener listener;

    stub->addListener([&listener](const std::string &deviceName, const std::string action) {
        listener.ServerCallBack(deviceName, action);
        listener.called++;
    });
    EXPECT_CALL(listener, ServerCallBack(deviceName, "update")).Times(1);

    ShadowActionCallback cb{[](ShadowAction action, ShadowAckStatus status, ShadowActionAck *ack, void *context) {
        MockListener *pListener = static_cast<MockListener *>(context);
        pListener->ClientCallback(action, status, ack, context);
        pListener->called++;
    }};
    EXPECT_CALL(listener, ClientCallback(SHADOW_UPDATE, SHADOW_ACK_ACCEPTED, testing::_, &listener)).Times(1);

    cJSON *reported = cJSON_CreateObject();
    cJSON_AddStringToObject(reported, "color", "green");
    device_management_shadow_update(client, cb, &listener, 10, reported, NULL);
    cJSON_Delete(reported);

    for (int i = 0; i < 60; ++i) {
        if (listener.called == 2) {
            break;
        }
        sleep(1);
    }
    ASSERT_EQ(2, listener.called);
}
