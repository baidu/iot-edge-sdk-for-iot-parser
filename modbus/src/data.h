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

#ifndef INF_BCE_IOT_MODBUS_SDK_C_DATA_H
#define INF_BCE_IOT_MODBUS_SDK_C_DATA_H

#include <time.h>
#include <MQTTClient.h>

// constants
enum {
    UUID_LEN = 38,
    MAX_SLAVE_ID = 247,
    MODBUS_DATA_COUNT = 248,
    MAX_LEN = 512,
    BUFF_LEN = 2018,
    ADDR_LEN = 64,
    MAX_MODBUS_DATA_TO_WRITE = 123
};

// types
typedef enum
{
    TCP = 0,
    RTU,
    ASCII
} ModbusMode;

typedef struct
{
    char endpoint[MAX_LEN];
    char topic[MAX_LEN];
    char user[MAX_LEN];
    char password[MAX_LEN];
} Channel;

typedef struct
{
    char endpoint[MAX_LEN];
    char topic[MAX_LEN];
    char user[MAX_LEN];
    char password[MAX_LEN];
    char backControlTopic[MAX_LEN];
} GatewayConfig;

typedef struct SlavePolicy_t
{
    char gatewayid[UUID_LEN]; 		// the cloud logic gateway id, used to distinguish slaves
    int slaveid;
    ModbusMode mode;    			// tcp, rtu, ascii
    char ip_com_addr[ADDR_LEN];
    char functioncode;
    int start_addr;
    int length;
    int interval;    				// in seconds
    char trantable[UUID_LEN];
    Channel pubChannel;    			// which channel to upload(pub) data
    time_t nextRun;    				//  time for next execution of this policy
    struct SlavePolicy_t* next;    	// the next salve policy that need to execute
    int mqttClient;
    int baud;
    int databits;
    char parity;
    int stopbits;
} SlavePolicy;

#endif 
