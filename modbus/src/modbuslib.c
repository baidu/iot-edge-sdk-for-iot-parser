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

#include "modbuslib.h"
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <modbus/modbus.h>

modbus_t* g_modbus_ctxs[MODBUS_DATA_COUNT];

void init_modbus_context(SlavePolicy* policy)
{
    if (policy == NULL)
    {
        return;
    }

    if (g_modbus_ctxs[policy->slaveid] != NULL)
    {
        return; // already initialized
    }

    modbus_t* ctx = NULL;
    if (policy->mode == TCP)
    {
        char ip[256];
        mystrncpy(ip, policy->ip_com_addr, ADDR_LEN);
        int len = strlen(ip);
        int i = 0;
        while (i < len && ip[i] != ':')
        {
            i++;
        }

        int port = 502;
        if (i < len)
        {
            ip[i] = '\0';
            i++;
            if (i < len)
            {
                port = atoi(ip + i);
            }
        }
        ctx = modbus_new_tcp(ip, port);
        if (modbus_connect(ctx) == -1) 
        {
            fprintf(stderr, "Failed to connect modbus slave: %s, ip=%s, port=%d\n",
                    modbus_strerror(errno), ip, port);
            modbus_free(ctx);
            ctx = NULL ;
        }
    }
    else if (policy->mode == RTU)
    {
        ctx = modbus_new_rtu(policy->ip_com_addr, policy->baud, policy->parity, 
                policy->databits, policy->stopbits);
        if (modbus_connect(ctx) == -1) 
        {
            fprintf(stderr, "Failed to connect modbus slave: %s, serial port=%s, baud=%d"
                    " parity=%c, databits=%d, stopbits=%d\n",
                    modbus_strerror(errno), policy->ip_com_addr, policy->baud, policy->parity,
                    policy->databits, policy->stopbits);
            modbus_free(ctx);
            ctx = NULL ;
        }
    }
    else
    {
        fprintf(stderr, "Not supported modbus mode %d, only support modbus TCP and RTU now\n",
                 (int)policy->mode);
    }
    
    if (ctx != NULL)
    {
        modbus_set_slave(ctx, policy->slaveid);
    }
    g_modbus_ctxs[policy->slaveid] = ctx;
}

int read_modbus(SlavePolicy* policy, char* payload)
{
    if (policy == NULL)
    {
        fprintf(stderr, "NULL policy in read_modbus\n");
        return -1;
    }

    // 1 query modbus data
    // parse the host and port first
    modbus_t* ctx = g_modbus_ctxs[policy->slaveid];
    if (ctx == NULL)
    {
        // slave could be offline when we initialize the modbus context,
        // we need to recover this, by trying to re-connect
        fprintf(stderr, 
            "modbus context is NULL in execution phase, trying to reconnect slaveid=%d\n", 
            policy->slaveid);
        init_modbus_context(policy);
    
        // Ask: slave may disconnect, leaving a non-NULL ctx, how should we recover?    
        // Answer: reconnect on communication error
    }
    ctx = g_modbus_ctxs[policy->slaveid];
    if (ctx == NULL)
    {
        fprintf(stderr, "can't make connection to modbus slave#%d\n", policy->slaveid);
        return -1;
    }

    int start_addr = policy->start_addr;
    int nb = policy->length;

    uint16_t* tab_rq_registers = NULL;
    uint8_t* tab_rq_bits = NULL;

    int rc = -1;
    payload[0] = 0;    // empty the payload first
    int need_reconnect_modbus = 0;
    switch(policy->functioncode)
    {
        case MODBUS_FC_READ_COILS:
            // just store every bit as a byte, for easy of use
            tab_rq_bits = (uint8_t*) malloc(nb * sizeof(uint8_t));
            memset(tab_rq_bits, 0, nb * sizeof(uint8_t));
            rc = modbus_read_bits(ctx, start_addr, nb, tab_rq_bits);
            if (rc != nb) 
            {
                printf("ERROR modbus_read_bits (%d) slaveid=%d, will reconnect\n",
                     rc, policy->slaveid);
                need_reconnect_modbus = 1;
                break;
            }
            byte_arr_to_hex(payload, (char*)tab_rq_bits, nb * sizeof(uint8_t));
            break;

        case MODBUS_FC_READ_DISCRETE_INPUTS:
            tab_rq_bits = (uint8_t*) malloc(nb * sizeof(uint8_t));
            memset(tab_rq_bits, 0, nb * sizeof(uint8_t));
            rc = modbus_read_input_bits(ctx, start_addr, nb, tab_rq_bits);
            if (rc != nb)
            {
                printf("ERROR modbus_read_input_bits (%d) slaveid=%d, will reconnect\n",
                     rc, policy->slaveid);
                need_reconnect_modbus = 1;
                break;
            }
            byte_arr_to_hex(payload, (char*)tab_rq_bits, nb * sizeof(uint8_t));
            break;
    
        case MODBUS_FC_READ_HOLDING_REGISTERS:
            tab_rq_registers = (uint16_t*) malloc(nb * sizeof(uint16_t));
            memset(tab_rq_registers, 0, nb * sizeof(uint16_t));
            rc = modbus_read_registers(ctx, start_addr, nb, tab_rq_registers);
            if (rc != nb)
            {
                printf("ERROR modbus_read_registers (%d) slaveid=%d, will reconnect\n",
                     rc, policy->slaveid);
                need_reconnect_modbus = 1;
                break;
            }
            short_arr_to_array(payload, tab_rq_registers, nb);
            break;

        case MODBUS_FC_READ_INPUT_REGISTERS:
            tab_rq_registers = (uint16_t*) malloc(nb * sizeof(uint16_t));
            memset(tab_rq_registers, 0, nb * sizeof(uint16_t));
            rc = modbus_read_input_registers(ctx, start_addr, nb, tab_rq_registers);
            if (rc != nb)
            {
                printf("ERROR modbus_read_input_registers (%d) slaveid=%d, will reconnect\n",
                     rc, policy->slaveid);
                need_reconnect_modbus = 1;
                break;
            }
            short_arr_to_array(payload, tab_rq_registers, nb);
            break;

        default:
            fprintf(stderr, "not supported function code:%d\n", policy->functioncode);
            break;
    }
    if (tab_rq_bits != NULL)
    {
        free(tab_rq_bits);
    }

    if (tab_rq_registers != NULL)
    {
        free(tab_rq_registers);
    }
    
    if (need_reconnect_modbus == 1)
    {
        if (g_modbus_ctxs[policy->slaveid] != NULL)
        {
            // in case there is error when read modbus, we need
            // to close the current context, otherwise, the 
            // port/serial port is still be occupied.
            modbus_close(g_modbus_ctxs[policy->slaveid]);
            modbus_free(g_modbus_ctxs[policy->slaveid]);
        }
        g_modbus_ctxs[policy->slaveid] = NULL;
        init_modbus_context(policy);
    }
}

void cleanup_modbus_ctxs()
{
    // clean modbus context
    int i = 0;
    for(i = 0; i < MODBUS_DATA_COUNT; i++)
    {
        modbus_t* ctx = g_modbus_ctxs[i];
        if (ctx != NULL)
        {
            modbus_close(ctx);
            modbus_free(ctx);
            g_modbus_ctxs[i] = NULL;
        }
    }
}

void init_modbus_ctxs()
{
    int i = 0;
    for (i = 0; i < MODBUS_DATA_COUNT; i++)
    {
        g_modbus_ctxs[i] = NULL;
    }
}

/**
*   write data into the specified slaveid, the slaveid must 
*   within the slaveis that the gateway is pulling data from
*
*   slaveid: the target modbus slave
*   startAddress: the address of the first register/bit to be written, e.g 40003, 00005
*   data: the data to write, e.g "00ff12cd", presented as string
*   
*   the number of data to write depends on the lenght of data, and the start address.
*   e.g data="00ff00fe" and startAddress=40001, then 40001,40002 will be written.
*   e.g data="00ff00fe" and startAddress=00001, the 00001,00002,00003,00004 will be written
*
*   return: 0 on sucess, -1 otherwise
**/
int write_modbus(int slaveid, int startAddress, char* data)
{
    // check if slave is connected or not
    if (slaveid < 1 || slaveid >= MODBUS_DATA_COUNT)
    {
        return -1;
    }
    modbus_t* ctx = g_modbus_ctxs[slaveid];
    if (ctx == NULL)
    {
        return -1;
    }

    // check data
    if (data == NULL || strlen(data) < 2)
    {
        return -1;  // expects at least 2 chars, like "01"
    }

    int rc = 0;

    // check start address
    if (startAddress >= 1 && startAddress < 9999)
    {        
        uint8_t data8[MAX_MODBUS_DATA_TO_WRITE];
        int num = char2uint8(data8, data);
        int startOffset = startAddress - 1;
        if (num > 0)
        {
            rc = modbus_write_bits(ctx, startOffset, num, data8);
            if (rc == -1)
            {
                printf("write bits failed, slaveid=%d, address=%d, data=%s\n", slaveid, startAddress, data);
                return -1;
            }
        }
        return 0;
    } 
    else if (startAddress >= 40001 && startAddress < 49999)
    {
        uint16_t data16[MAX_MODBUS_DATA_TO_WRITE];
        int num = char2uint16(data16, data);
        int startOffset = startAddress - 40001;
        if (num > 0)
        {
            rc = modbus_write_registers(ctx, startOffset, num, data16);
            if (rc == -1)
            {
                printf("write registers failed, slaveid=%d, address=%d, data=%s\n", slaveid, startAddress, data);
                return -1;
            }
        }
        return 0;
    } 
    else {
        // invalid start address
        printf("unsupported address for write:%d\n", startAddress);
    }

    return -1;
}