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

#ifndef INF_BCE_IOT_MODBUS_SDK_C_COMMON_H
#define INF_BCE_IOT_MODBUS_SDK_C_COMMON_H

#include "data.h"

#include <cjson/cJSON.h>
#include <modbus/modbus.h>

// common function section
long read_file_as_string(char const* path, char** buf);

void toggle_debug();

void log_debug(char* msg);

void mystrncpy(char* desc, const char* src, int len);

int json_int(cJSON* root, char* item);

char* json_string(cJSON* root, char* item);

// convert byte 0x01 to char '0' and '1'
void char2hex(char c, char* hex1, char* hex2);

// convert char* to hex, like 0A126F...
void byte_arr_to_hex(char* dest, char* src, int len);

void short_arr_to_array(char* dest, uint16_t* src, int len);

void channel_to_json(Channel* ch, int maxlen, char* dest);

// convert "00ff1234" to 0x00ff, 0x1234 ...
int char2uint16(uint16_t* dest, const char* src);
// convert "00ff" to 0x00, 0xff, ....
int char2uint8(uint8_t* dest, const char* src);
#endif
