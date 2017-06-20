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
 * @authors Zhao Bo
 */

#include "test_conf.h"

const std::string &TestConf::getTestMqttBroker() {
    static std::string broker = "tcp://localhost";
    return broker;
}

const std::string &TestConf::getTestMqttUsername() {
    static std::string username = "test/test";
    return username;
}

const std::string &TestConf::getTestMqttPassword() {
    static std::string password = "test";
    return password;
}

const std::string &TestConf::getTopicPrefix() {
    static std::string prefix = "$baidu/iot/shadow/";
    return prefix;
}

//const std::string TestConf::topicPrefix;