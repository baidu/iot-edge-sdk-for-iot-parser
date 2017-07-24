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

#include "common.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>

// common function section
long read_file_as_string(char const* path, char** buf)
{
    FILE* fp = NULL;
    size_t fsz = 0;
    long   off_end = 0;
    int    rc = 0;

    fp = fopen(path, "rb");
    if(NULL == fp)
    {
        return -1L;
    }

    rc = fseek(fp, 0L, SEEK_END);
    if(0 != rc)
    {
        return -1L;
    }

    if(0 > (off_end = ftell(fp)))
    {
        return -1L;
    }
    fsz = (size_t)off_end;

    *buf = malloc(fsz + 1);
    if(NULL == *buf)
    {
        return -1L;
    }

    rewind(fp);

    if(fsz != fread(*buf, 1, fsz, fp))
    {
        free(*buf);
        return -1L;
    }

    if(EOF == fclose(fp))
    {
        free(*buf);
        return -1L;
    }

    return (long)fsz;
}

int g_debug = 0;
void toggle_debug()
{
    g_debug = g_debug == 0 ? 1 : 0;
    if (g_debug == 1)
    {
        printf("debug info is on\n");
    }
    else
    {
        printf("debug info is off\n");
    }
}

void log_debug(char* msg)
{
    if (g_debug == 1)
    {
        time_t now = time(NULL);
        printf("%s %s\n", ctime(&now), msg);
    } 
}

void mystrncpy(char* desc, const char* src, int len)
{
    snprintf(desc, len, "%s", src);
}

int json_int(cJSON* root, char* item)
{
    return cJSON_GetObjectItem(root, item)->valueint;
}

char* json_string(cJSON* root, char* item)
{
    return cJSON_GetObjectItem(root, item)->valuestring;
}

// convert byte 0x01 to char '0' and '1'
void char2hex(char c, char* hex1, char* hex2)
{
    char high = (c & 0XF0) >> 4;
    char low = c & 0X0F;
    *hex1 = high < 10 ? high + '0' : high - 10 + 'A';
    *hex2 = low < 10 ? low + '0' : low - 10 + 'A';
}

// convert 'A' to 10, or '2' to 2
char char2dec(char data)
{
    if (data >= '0' && data <= '9')
    {
        return data - '0';
    } 
    else if (data >= 'a' && data <= 'f')
    {
        return data - 'a' + 0xa;
    }
    else if (data >= 'A' && data <= 'F')
    {
        return data - 'A' + 0xa;
    }
    return 0;
}

// convert char* to hex, like 0A126F...
void byte_arr_to_hex(char* dest, char* src, int len)
{
    int i = 0; 
    for (; i < len; i++)
    {
        char2hex(src[i], &dest[i << 1], &dest[(i << 1) + 1]);
    }
    dest[len << 1] = '\0';
}

void short_arr_to_array(char* dest, uint16_t* src, int len)
{
    int i = 0; 
    for (; i < len; i++)
    {
        char high = (src[i] & 0XFF00) >> 8;
        char low = src[i] & 0XFF;
        char2hex(high, &dest[i << 2], &dest[(i << 2) +1]);
        char2hex(low, &dest[(i << 2) + 2], &dest[(i << 2) +3]);
    }
    dest[len << 2] = '\0';
}

void channel_to_json(Channel* ch, int maxlen, char* dest)
{
    if (dest == NULL)
    {
        return;
    }
    if (ch == NULL)
    {
        snprintf(dest, maxlen, "{}");
        return;
    }

    snprintf(dest, maxlen, 
        "{\"endpoint\":\"%s\", \"topic\":\"%s\", \"user\":\"%s\", \"password\":\"%s\"}",
         ch->endpoint, ch->topic, ch->user, ch->password);
};

// convert "00ff1234" to 0x00ff, 0x1234, ...
// return the number of data converted
int char2uint16(uint16_t* dest, const char* src)
{
    int len = strlen(src);
    int i = 0 ;
    int cnt = 0;
    for (i = 0; i < len / 4; i++)
    {
        uint16_t data = 0;
        data |= char2dec(src[4 * i]) << 12;
        data |= char2dec(src[4 * i + 1]) << 8;
        data |= char2dec(src[4 * i + 2]) << 4;
        data |= char2dec(src[4 * i + 3]);
        dest[cnt++] = data;
    }

    return cnt;
}

// convert "00ff" to 0x00, 0xff, ....
// return the number of data converted
int char2uint8(uint8_t* dest, const char* src)
{
    int len = strlen(src);
    int i = 0 ;
    int cnt = 0;
    for (i = 0; i < len / 2; i++)
    {
        uint8_t data = 0;
        data |= char2dec(src[2 * i]) << 4;
        data |= char2dec(src[2 * i + 1]);
        dest[cnt++] = data;
    }

    return cnt;
}