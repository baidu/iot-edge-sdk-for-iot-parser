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

#ifndef INF_BCE_IOT_MODBUS_SDK_C_MODBUSLIB_H
#define INF_BCE_IOT_MODBUS_SDK_C_MODBUSLIB_H

#include "data.h"

// make modbus connection to modbus slave, and 
// store the context in g_modbus_ctxs
void init_modbus_context(SlavePolicy* policy);

// issue a modbus request to modbus slave, and receive
// the data into payload.
// detect is modbus connection is established or not, 
// and auto reconnect if necessary
int read_modbus(SlavePolicy* policy, char* payload);

void cleanup_modbus_ctxs();

void init_modbus_ctxs();

#endif
