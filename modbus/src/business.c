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

#include "business.h"
#include "data.h"
#include "common.h"
#include "thread.h"
#include "mqttsender.h"

#include <string.h>
#include <stdlib.h>

const char* const PEM_FILE = "root_cert.pem";
const char* const CONFIG_FILE = "gwconfig.txt";
const char* const POLICY_CACHE = "policyCache.txt";
const char* const DATA_CACHE = "data_cache.dat";

// when worker is running, it should require this lock first
// when policy loader is going to change policy, it also need to 
// acquire this lock first
mutex_type g_policy_lock;
SlavePolicy g_slave_header;    // the pure header node for slave polices

int g_policy_updated = 1;
mutex_type g_policy_update_lock;

int g_gateway_connected = 0;
mutex_type g_gateway_mutex;

GatewayConfig g_gateway_conf;
cJSON* g_misc = NULL;   // extra info need to pub to cloud in every message, eg. imei
char g_buff[BUFF_LEN];
int g_stop_worker = 0;
int g_worker_is_running = 0;
static int g_mqttsender = -1;
static int g_cache_size = 1024 * 1024 * 500;    // 500 MB,

MQTTClient_SSLOptions g_sslopts = MQTTClient_SSLOptions_initializer;

void set_ssl_option(MQTTClient_connectOptions* conn_opts, char* host)
{
    char* ssl = "ssl://";
    char* ssl_upper = "SSL://";
    if (strncmp(ssl, host, strlen(ssl)) == 0
        || strncmp(ssl_upper, host, strlen(ssl_upper)) == 0)
    {
        g_sslopts.trustStore = PEM_FILE;
        g_sslopts.enableServerCertAuth = 1;
        conn_opts->ssl = &g_sslopts;
    }
}

int load_channel(Channel* conf)
{
    if (conf == NULL)
    {
        printf("the parameter conf is NULL\n");
        return 0;
    }
    char* content = NULL;
    long size = read_file_as_string(CONFIG_FILE, &content);
    if (size <= 0)
    {
        printf("failed to open config file %s, while trying to load gateway configuration\n",
             CONFIG_FILE);
        return 0;
    }
    
    cJSON* root = cJSON_Parse(content);
    if (root == NULL)
    {
        printf("the config file is not a valid json object, file=%s\n", CONFIG_FILE);
        return 0;
    }
    mystrncpy(conf->endpoint, cJSON_GetObjectItem(root, "endpoint")->valuestring, MAX_LEN);
    mystrncpy(conf->topic, cJSON_GetObjectItem(root, "topic")->valuestring, MAX_LEN);
    mystrncpy(conf->user, cJSON_GetObjectItem(root, "user")->valuestring, MAX_LEN);
    mystrncpy(conf->password, cJSON_GetObjectItem(root, "password")->valuestring, MAX_LEN);

    free(content);
    cJSON_Delete(root);
    return 1;
}

int load_gateway_config(GatewayConfig* conf)
{
    if (conf == NULL)
    {
        printf("the parameter conf is NULL\n");
        return 0;
    }
    char* content = NULL;
    long size = read_file_as_string(CONFIG_FILE, &content);
    if (size <= 0)
    {
        printf("failed to open config file %s, while trying to load gateway configuration\n",
             CONFIG_FILE);
        return 0;
    }
    
    cJSON* root = cJSON_Parse(content);
    if (root == NULL)
    {
        printf("the config file is not a valid json object, file=%s\n", CONFIG_FILE);
        return 0;
    }
    mystrncpy(conf->endpoint, cJSON_GetObjectItem(root, "endpoint")->valuestring, MAX_LEN);
    mystrncpy(conf->topic, cJSON_GetObjectItem(root, "topic")->valuestring, MAX_LEN);
    mystrncpy(conf->user, cJSON_GetObjectItem(root, "user")->valuestring, MAX_LEN);
    mystrncpy(conf->password, cJSON_GetObjectItem(root, "password")->valuestring, MAX_LEN);
    // backControlTopic may be missing, or may be null
    conf->backControlTopic[0] = 0;
    if (cJSON_HasObjectItem(root, "backControlTopic")) {
        cJSON* backControlTopicObj = cJSON_GetObjectItem(root, "backControlTopic");
        if (! cJSON_IsNull(backControlTopicObj)) {
            mystrncpy(conf->backControlTopic, backControlTopicObj->valuestring, MAX_LEN);
        }
    }

    if (cJSON_HasObjectItem(root, "misc")) {
        cJSON* misc = cJSON_GetObjectItem(root, "misc");
        if (misc != NULL) {
            g_misc = cJSON_Duplicate(misc, 1);
        }
    }

    // g_cache_size
    if (cJSON_HasObjectItem(root, "cacheSize")) {
        cJSON* cacheSize = cJSON_GetObjectItem(root, "cacheSize");
        if (cacheSize != NULL) {
            g_cache_size = cacheSize->valueint;
        }
    }

    free(content);
    cJSON_Delete(root);
    return 1;
}

SlavePolicy* new_slave_policy()
{
    SlavePolicy* sp = (SlavePolicy*) malloc(sizeof(SlavePolicy));
    sp->nextRun = time(NULL);
    sp->next = NULL;
    sp->mqttClient = -1;

    return sp;
}

void cleanup_shared_data()
{
    cleanup_modbus_ctxs();
}    

void destroy_slave_policy(SlavePolicy* sp)
{
    if (sp == NULL)
    {
        return;
    }
    // modbus context is cleaned up in a centralized place (cleanup_shared_data())

    free(sp);
}

SlavePolicy* json_to_slave_poilicy(cJSON* root)
{
    SlavePolicy* policy = new_slave_policy();
    mystrncpy(policy->gatewayid, json_string(root, "gatewayid"), UUID_LEN);
    policy->slaveid = json_int(root, "slaveid");
    int int_mode = json_int(root, "mode");
    policy->mode = (ModbusMode)int_mode;
    mystrncpy(policy->ip_com_addr, json_string(root, "ip_com_addr"), ADDR_LEN);
    policy->functioncode = (char)json_int(root, "functioncode");
    policy->start_addr = json_int(root, "start_addr");
    policy->length = json_int(root, "length");
    policy->interval = json_int(root, "interval");
    mystrncpy(policy->trantable, json_string(root, "trantable"), UUID_LEN);
        
    cJSON* cjch = cJSON_GetObjectItem(root, "pubChannel");
    mystrncpy(policy->pubChannel.endpoint, json_string(cjch, "endpoint"), MAX_LEN);
    mystrncpy(policy->pubChannel.topic, json_string(cjch, "topic"), MAX_LEN);
    mystrncpy(policy->pubChannel.user, json_string(cjch, "user"), MAX_LEN);
    mystrncpy(policy->pubChannel.password, json_string(cjch, "password"), MAX_LEN);
    policy->nextRun = time(NULL) + policy->interval;

    if (policy->mode == RTU)
    {
        policy->baud = json_int(root, "baud");
        policy->databits = json_int(root, "databits");
        policy->parity = json_string(root, "parity")[0];
        policy->stopbits = json_int(root, "stopbits");
    }
    return policy;
}

void cleanup_data()
{
    SlavePolicy* sp = g_slave_header.next;
    g_slave_header.next = NULL;
    SlavePolicy* next_policy = NULL;
    while (sp != NULL)
    {
        next_policy = sp->next;
        destroy_slave_policy(sp);
        sp = next_policy;
    }
    cleanup_shared_data();
}

int load_slave_policy_from_cache(SlavePolicy* header)
{
    // in case gateway can't retrieve SlavePolicy from cloud immediately,
    // we should cache the polices in a local file. Whenever gateway startup,
    // it should load polices form this local cache first, and in the mean time
    // listen any policy change pushed from cloud.
    log_debug("enter loadSlavePolicy");
    int rc = -1;
    // anyway we will clear the flag that need reload policy
    rc = Thread_lock_mutex(g_policy_update_lock);

    g_policy_updated = 0;

    char* content = NULL;

    long filesize = read_file_as_string(POLICY_CACHE, &content);
    if (filesize <= 0)
    {
        printf("failed to open policy cache file %s, skipping policy cache loading\n",
                 POLICY_CACHE);
        rc = Thread_unlock_mutex(g_policy_update_lock);
        return 0;
    }
    if (content == NULL)
    {
        return 0;
    }

    rc = Thread_unlock_mutex(g_policy_update_lock);
    cJSON* fileroot = cJSON_Parse(content);
    if (fileroot == NULL)
    {
        printf("invalid config detected from cache file %s, skipping policy cache loading\r\n", 
                POLICY_CACHE);
        free(content);
        return 0;
    }
    int num = cJSON_GetArraySize(fileroot);
    if (num < 0)
    {
        printf("no slave policy is loaded from cache file %s\r\n", POLICY_CACHE);
        free(content);
        return 1;
    }

    rc = Thread_lock_mutex(g_policy_lock);

    // clear all the existing data 
    cleanup_data();

    int i = 0;
    for(i = 0; i < num; i++)
    {
        cJSON* root = cJSON_GetArrayItem(fileroot, i);
        SlavePolicy* policy = json_to_slave_poilicy(root);
        init_modbus_context(policy);

        // add the policy into list
        policy->next = g_slave_header.next;
        g_slave_header.next = policy;
    }
    rc = Thread_unlock_mutex(g_policy_lock);

    cJSON_Delete(fileroot);
    free(content);
}

void insert_slave_policy(SlavePolicy* policy)
{
    // insert a new slave policy into the list, ordered by the nextRun asc
    // so that the head of the list is always the next slave need to execute
    // note, this function could be involked in a different thread, make sure 
    // add lock before modifying the list

    if (policy == NULL)
    {
        return;
    }

    // we already have the lock here.
    SlavePolicy* itr = &g_slave_header;
    while (itr->next != NULL && itr->next->nextRun < policy->nextRun)
    {
        itr = itr->next;
    }
    policy->next = itr->next;
    itr->next = policy;
}

void delivered(void* context, MQTTClient_deliveryToken dt)
{
    printf("Message with token value %d delivery confirmed\r\n", dt);
}

int msg_arrived(void* context, char* topicName, int topicLen, MQTTClient_message* message)
{
    // sometime we receive strange message with topic name like "\300\005@\267"
    // let's filter those message that with topic other than expected
    if (message == NULL)
    {
        return 1;
    }
    if (strcmp(g_gateway_conf.topic, topicName) == 0) {
        return handle_config_msg(context, topicName, topicLen, message);
    } else if (strlen(g_gateway_conf.backControlTopic) > 0
        && strcmp(g_gateway_conf.backControlTopic, topicName) == 0) {
        return handle_back_control_msg(context, topicName, topicLen, message);
    } else {
        snprintf(g_buff, BUFF_LEN, 
                "received unrelevant message in command topic, skipping it. topic=%s, payload=", 
                topicName);
        log_debug(g_buff);
    
        MQTTClient_freeMessage(&message);
        MQTTClient_free(topicName);

        return 1;
    }
}

int handle_back_control_msg(void* context, char* topicName, int topicLen, MQTTClient_message* message) {
    int i = 1;
    char* payloadptr = NULL;

    payloadptr = message->payload;
    int buflen = message->payloadlen + 1;
    char* buf = (char*)malloc(buflen);
    buf[buflen - 1] = 0;
    for(i = 0; i<message->payloadlen; i++)
    {
        buf[i] = payloadptr[i];
    }

    MQTTClient_freeMessage(&message);
    MQTTClient_free(topicName);
    cJSON* root = cJSON_Parse(buf);
    if (root == NULL)
    {
        free(buf);
        printf("received invalid json config for writing modbus:%s\r\n", buf);
        return 1;
    }
    
    // the control config looks like
    // {
    //     "request1": {
    //         "slaveid": 1,
    //         "address": 40001,
    //         "data": "00ff1234"
    //     },
    //     "request2": {
    //         "slaveid": 2,
    //         "address": 10020,
    //         "data": "00ff1234"
    //     }
    // }
    char key[11];
    // lets limit the max data point to write to 100
    for (i = 1; i <= 100; i++) {
        sprintf(key, "request%d", i);
        if (cJSON_HasObjectItem(root, key)) {
            cJSON* req = cJSON_GetObjectItem(root, key);
            int slaveid = json_int(req, "slaveid");
            int address = json_int(req, "address");
            char* data = json_string(req, "data");
            write_modbus(slaveid, address, data);
        } else {
            break;
        }
    }

    cJSON_Delete(root);
    return 1;
}

int handle_config_msg(void* context, char* topicName, int topicLen, MQTTClient_message* message) {
    int i = 0;
    char* payloadptr = NULL;

    payloadptr = message->payload;
    int buflen = message->payloadlen + 1;
    char* buf = (char*)malloc(buflen);
    buf[buflen - 1] = 0;
    for(i = 0; i<message->payloadlen; i++)
    {
        buf[i] = payloadptr[i];
    }

    MQTTClient_freeMessage(&message);
    MQTTClient_free(topicName);
    cJSON* root = cJSON_Parse(buf);
    if (root == NULL)
    {
        free(buf);
        printf("received invalid json config:%s\r\n", buf);
        return 1;
    }
    cJSON_Delete(root);
    Thread_lock_mutex(g_policy_update_lock);
    FILE* fp = fopen(POLICY_CACHE, "w");
    if (! fp)
    {
        free(buf);
        snprintf(g_buff, BUFF_LEN, "failed to open %s for write", POLICY_CACHE);
        log_debug(g_buff);
        Thread_unlock_mutex(g_policy_update_lock);
        return 0;
    }
    fprintf(fp, "%s", buf);
    fclose(fp);
    printf("received gateway config from cloud\r\n");
    free(buf);

    g_policy_updated = 1;
    Thread_unlock_mutex(g_policy_update_lock);
    return 1;
}

void connection_lost(void* context, char* cause)
{
    printf("\nConnection lost, caused by %s, will reconnect later\r\n", cause);
    MQTTClient client = (MQTTClient) context;
    MQTTClient_destroy(&client);
    Thread_lock_mutex(g_gateway_mutex);
    g_gateway_connected = 0;
    Thread_unlock_mutex(g_gateway_mutex);
}

void start_listen_command()
{
    printf("connecting gateway to cloud...\r\n");
    Thread_lock_mutex(g_gateway_mutex);
    // sub to config mqtt topic, to receive slave policy from cloud
    MQTTClient client;
    MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
    int rc = 0;
    int ch = 0;

    char clientid[MAX_LEN];
    snprintf(clientid, MAX_LEN, "modbusGW%lld", (long long)time(NULL));
    MQTTClient_create(&client, g_gateway_conf.endpoint, clientid,
        MQTTCLIENT_PERSISTENCE_NONE, NULL);

    conn_opts.keepAliveInterval = 20;
    conn_opts.cleansession = 1;
    conn_opts.username = g_gateway_conf.user;
    conn_opts.password = g_gateway_conf.password;
    conn_opts.connectTimeout = 5;
    set_ssl_option(&conn_opts, g_gateway_conf.endpoint);

    MQTTClient_setCallbacks(client, client, connection_lost, msg_arrived, delivered);

    if ((rc = MQTTClient_connect(client, &conn_opts)) != MQTTCLIENT_SUCCESS)
    {
        MQTTClient_destroy(&client);
        printf("Failed to connect, return code %d\r\n", rc);
        Thread_unlock_mutex(g_gateway_mutex);

        return;
    }
    if (strlen(g_gateway_conf.backControlTopic) > 0) {
        char* topics[2];
        topics[0] = g_gateway_conf.topic;
        topics[1] = g_gateway_conf.backControlTopic;
        int qoss[2];
        qoss[0] = 0;
        qoss[1] = 0;
        MQTTClient_subscribeMany(client, 2, topics, qoss);
    } else {
        MQTTClient_subscribe(client, g_gateway_conf.topic, 0);
    }

    g_gateway_connected = 1;
    Thread_unlock_mutex(g_gateway_mutex);
}

void pack_pub_msg(SlavePolicy* policy, char* raw, char* dest)
{
    cJSON* root = cJSON_CreateObject(); 
    cJSON_AddNumberToObject(root, "bdModbusVer", 1);
    cJSON_AddStringToObject(root, "gatewayid", policy->gatewayid);
    cJSON_AddStringToObject(root, "trantable", policy->trantable);
    cJSON* modbus = NULL;
    cJSON_AddItemToObject(root, "modbus", modbus = cJSON_CreateObject());
    
    cJSON* request = NULL;
    cJSON_AddItemToObject(modbus, "request", request = cJSON_CreateObject());
    cJSON_AddNumberToObject(request, "functioncode", policy->functioncode);
    cJSON_AddNumberToObject(request, "slaveid", policy->slaveid);
    cJSON_AddNumberToObject(request, "startAddr", policy->start_addr);
    cJSON_AddNumberToObject(request, "length", policy->length);
    
    cJSON_AddStringToObject(modbus, "response", raw);
    
    time_t now = time(NULL);
    char timestamp[40];
    snprintf(timestamp, 39, "%lld", (long long) now);
    cJSON_AddStringToObject(root, "timestamp", timestamp);

    // misc
    if (g_misc != NULL) {
        cJSON* misc = cJSON_Duplicate(g_misc, 1);
        cJSON_AddItemToObject(root, "misc", misc);
    }
    char* text = cJSON_PrintUnformatted(root);
    mystrncpy(dest, text, BUFF_LEN);
    free(text);
    cJSON_Delete(root);
}

static void on_modbus_read() {
    FILE* fp = fopen("on_modbus_read", "w");
    if (fp != NULL) {
        fprintf(fp, "%lld", (long long)time(NULL));
        fclose(fp);
    }
}

void execute_policy(SlavePolicy* policy)
{
    // 3 recaculate the next run time
    policy->nextRun = policy->interval + time(NULL);

    // 4 re-insert into the list, in order of nextRun
    insert_slave_policy(policy);
    
    if (policy == NULL)
    {
        return;
    }

    // 1 query modbus data
    char payload[1024];
    int rc = read_modbus(policy, payload);
    // 2 pub modbus data
    if (strlen(payload) > 0)
    {
        char msgcontent[BUFF_LEN];
        pack_pub_msg(policy, payload, msgcontent);
        mqtt_send(g_mqttsender, 
                    policy->pubChannel.endpoint, 
                    policy->pubChannel.user,
                    policy->pubChannel.password,
                    policy->pubChannel.topic,
                    msgcontent,
                    strlen(msgcontent),
                    0,
                    PEM_FILE); 
    }
    if (rc != -1) {
        on_modbus_read();
    }
}

//TODO: fire up a few more workers, and precess in parallel, to speed up.
static thread_return_type worker_func(void* arg)
{
    g_worker_is_running = 1;
    while (g_stop_worker != 1)
    {
        // load slave policy if it's updated
        if (g_policy_updated)
        {
            load_slave_policy_from_cache(&g_slave_header);
        }
        
        if (g_gateway_connected == 0)
        {
            start_listen_command();
        }

        // iterate from the beginning of the policy list
        // and pick those whose nextRun is due, and execute them, 
        // calculate the new next run, and insert into the list
        time_t now = time(NULL);
        // we have something to do, acquire the lock here
        int rc = Thread_lock_mutex(g_policy_lock);
        if (g_slave_header.next != NULL && g_slave_header.next->nextRun <= now)
        {    
            while (g_slave_header.next != NULL && g_slave_header.next->nextRun <= now)
            {
                // detach from the list
                SlavePolicy* policy = g_slave_header.next;
                g_slave_header.next = g_slave_header.next->next;
                execute_policy(policy);
            }
        }
        rc = Thread_unlock_mutex(g_policy_lock);    

        sleep(1);
    }
    log_debug("exiting worker thread...\r\n");
    g_worker_is_running = 0;
}

thread_type g_worker_thread;

void start_worker()
{
    g_worker_thread = Thread_start(worker_func, (void*) NULL);
}

void init_static_data()
{
    g_policy_lock = Thread_create_mutex();
    g_policy_update_lock = Thread_create_mutex();
    g_gateway_mutex = Thread_create_mutex();
    
    init_modbus_ctxs();
}

void init_and_start()
{
    printf("Baidu IoT Modbus SDK v0.3.0\r\n");

    init_static_data();

    // 
    // 1 load gateway configuration from local file
    // 2 receive device(slave) polling config from cloud, or local cache
    // 3 start execution, modbus read and PUB

    // 1 load gateway configuration from local file
    if (load_gateway_config(&g_gateway_conf))
    {
        printf("successfully loaded gateway config from file %s\r\n", CONFIG_FILE);
    } 
    else 
    {
        printf("failed to load gateway configuration from file %s\r\n", CONFIG_FILE);
    }
    
    g_mqttsender = new_mqtt_sender(DATA_CACHE, g_cache_size);         

    // 2 receive device(slave) polling config from cloud, or local cache
    g_slave_header.next = NULL;
    load_slave_policy_from_cache(&g_slave_header);

    start_listen_command();
    start_worker();
}

void wait_user_input()
{
    char ch = '\0';
    printf("Gateway is running, press 'q' to exit, press 'd' to toggle debug\r\n");
    do 
    {
        ch = getchar();
        
        // toggle debug on/off
        if (ch == 'd' || ch == 'D')
        {
            toggle_debug();
        }
    } while(ch!='Q' && ch != 'q'); 
    g_stop_worker = 1;
    printf("exiting...\n");
}

void clean_and_exit()
{
    // pthread_join(g_worker_thread, NULL);
    int count = 0;
    while (g_worker_is_running == 1 && ++count < 10) {
        sleep(1);
    }

    close_mqtt_sender(g_mqttsender);
    cleanup_data();
}
