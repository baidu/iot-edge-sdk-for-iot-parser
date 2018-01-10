#include "mqttutil.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#include "common.h"

// cache at most 2048 messages, ring buffer
enum {MSG_BUF_SIZE = 10240};
typedef struct {
    uint8_t* data;
    int data_len;
    char* instance_number;
} DataToSend;
DataToSend g_data_to_send[MSG_BUF_SIZE];
int g_buff_head = 0;
int g_buff_size = 0;
const char* const PEM_FILE = "root_cert.pem";
MQTTClient_SSLOptions g_sslopts = MQTTClient_SSLOptions_initializer;

char g_log_buff[BUFF_LEN] = {0};

int is_buffer_full()
{
    return g_buff_size >= MSG_BUF_SIZE;
}

int is_buffer_empty()
{
    return g_buff_size <= 0;
}

int put_buff_data(uint8_t* data, int data_len, char* instance_number)
{
    // if full, them overwrite the oldest one
    int idx = (g_buff_size + g_buff_head) % MSG_BUF_SIZE;
    if (is_buffer_full())
    {
        free(g_data_to_send[idx].data);
        free(g_data_to_send[idx].instance_number);
        g_data_to_send[idx].data = data;
        g_data_to_send[idx].data_len = data_len;
        g_data_to_send[idx].instance_number = instance_number;
        g_buff_head = (g_buff_head + 1) % MSG_BUF_SIZE;
        g_buff_size = MSG_BUF_SIZE;
        return 0;
    }
    else
    {
        g_data_to_send[idx].data = data;
        g_data_to_send[idx].data_len = data_len;
        g_data_to_send[idx].instance_number = instance_number;
        g_buff_size++;
        return 1;
    }
}

DataToSend get_buff_data()
{
    if (is_buffer_empty())
    {
        return (DataToSend){};
    }
    
    DataToSend ret = g_data_to_send[g_buff_head];
    g_buff_head = (g_buff_head + 1) % MSG_BUF_SIZE;
    g_buff_size--;
    return ret;
}

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


void start_mqtt_client(GlobalVar* vars,
                       connection_lost_fun connection_lost,
                       msg_arrived_fun msg_arrived,
                       delivered_fun delivered)
{
    printf("connecting gateway to cloud...\n");
    printf("endpoint:%s\n", vars->g_mqtt_info.endpoint);
    printf("user:%s\n", vars->g_mqtt_info.user);

    Thread_lock_mutex((vars->g_mqtt_client_mutex));
    MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;

    if (vars->g_mqtt_client == NULL)
    {
        char clientid[MAX_LEN];
        snprintf(clientid, MAX_LEN, "bacnetGW%lld", (long long)time(NULL));
        MQTTClient_create(&(vars->g_mqtt_client), vars->g_mqtt_info.endpoint, clientid,
                      MQTTCLIENT_PERSISTENCE_NONE, NULL);
    }
    conn_opts.keepAliveInterval = 50;
    conn_opts.cleansession = 1;
    conn_opts.username = vars->g_mqtt_info.user;
    conn_opts.password = vars->g_mqtt_info.password;
    set_ssl_option(&conn_opts, vars->g_mqtt_info.endpoint);

    MQTTClient_setCallbacks(vars->g_mqtt_client, NULL, connection_lost, msg_arrived, delivered);

    int rc = 0;
    if ((rc = MQTTClient_connect(vars->g_mqtt_client, &conn_opts)) != MQTTCLIENT_SUCCESS)
    {
        printf("Failed to connect, return code %d\n", rc);
        Thread_unlock_mutex((vars->g_mqtt_client_mutex));

        return;
    }

    MQTTClient_subscribe(vars->g_mqtt_client, vars->g_mqtt_info.sub_config, 0);
    MQTTClient_subscribe(vars->g_mqtt_client, vars->g_mqtt_info.sub_topic, 0);

    vars->g_gateway_connected = 1;
    Thread_unlock_mutex((vars->g_mqtt_client_mutex));

    printf("gateway started!\n");
}

// return -1 if failed, 0 otherwise
int send_message(uint8_t* data, int data_len, GlobalVar* vars, char* instance_number)
{
    MQTTClient client = vars->g_mqtt_client;

    char* topic = NULL;
    switch((char)instance_number[0]) {
        case '0' :
            topic = create_topic(vars->g_mqtt_info.pub_iam, "");
            break;
        case '1' :
            topic = create_topic(vars->g_mqtt_info.pub_rpmack, &instance_number[1]);
            break;
        case '2' :
            topic = create_topic(vars->g_mqtt_info.pub_covnotice, &instance_number[1]);
            break;
        case '3' :
            topic = create_topic(vars->g_mqtt_info.pub_heartbeat, "");
            break;
        default :
            log_debug("unknow topic index, skip it simply");
            return 0;
    }

    MQTTClient_message pubmsg = MQTTClient_message_initializer;
    MQTTClient_deliveryToken delivery_token;
    pubmsg.payload = data;
    pubmsg.payloadlen = data_len;
    pubmsg.qos = 0;
    pubmsg.retained = 0;

    int rc = MQTTClient_publishMessage(client, topic, &pubmsg, &delivery_token);
    if (rc == MQTTCLIENT_SUCCESS)
    {
        vars->g_mqtt_info.last_heartbeat_time = time(NULL);
        MQTTClient_waitForCompletion(client, delivery_token, 1000L);
        free(topic);
        return 0;
    }
    else
    {
	// mark the gateway disconnected
	vars->g_gateway_connected = 0;
        free(topic);
        return -1; // failed to send
    }
}

int send_data(uint8_t* data, int data_len, GlobalVar* vars, char* instance_number)
{
    // if mqtt client is not in a good state, then cache the data for later send
    if (data == NULL)
    {
        return -1;
    }

    if (vars == NULL || !MQTTClient_isConnected(vars->g_mqtt_client))
    {
        put_buff_data(data, data_len, instance_number);
        log_debug("mqtt client is not connected, caching the data");
        return 0;
    }

    // send the data

    int rc = send_message(data, data_len, vars, instance_number);
    if (rc == 0)
    {
        free(data);

        // send the buffered data if any
        DataToSend buff_data = {0};
        int cnt = 0;
        while (rc == 0 && ! is_buffer_empty())
        {
            buff_data = get_buff_data();
            rc = send_message(buff_data.data, buff_data.data_len, vars, buff_data.instance_number);
            if (rc == 0)
            {
                cnt++;
                free(buff_data.data);
                free(buff_data.instance_number);
                buff_data.data = NULL;
                buff_data.instance_number = NULL;
            }
            else
            {
                // send mqtt message failed
                put_buff_data(buff_data.data, buff_data.data_len, buff_data.instance_number);
                break;
            }
        }
        if (cnt > 0)
        {
            snprintf(g_log_buff, BUFF_LEN, "sent %d message from cache", cnt);
            log_debug(g_log_buff);
        }

    }
    else
    {
        // cache the data, and mark the mqtt client need reconnect
        snprintf(g_log_buff, BUFF_LEN,
                 "failed to send a mqtt message, return code=%d. Caching it for later sending", rc);
        log_debug(g_log_buff);
        put_buff_data(data, data_len, instance_number);
        Thread_lock_mutex( vars->g_gateway_mutex);
        vars->g_gateway_connected = 0;
        Thread_unlock_mutex( vars->g_gateway_mutex);
    }

    return 0;
}

void mqtt_cleanup(GlobalVar* vars)
{
    MQTTClient_disconnect(vars->g_mqtt_client, 500);
    DataToSend buff_data = {0};
    while(! is_buffer_empty())
    {
        buff_data = get_buff_data();
        free(buff_data.data);
        buff_data.data = NULL;
        free(buff_data.instance_number);
        buff_data.instance_number = NULL;
    }
}

void mqtt_send_iam(uint8_t* data, int data_len, GlobalVar* vars)
{
    char* instance_number_ptr = (char*)malloc(2*sizeof(char));
    snprintf(instance_number_ptr, 2, "%d", 0);

    send_data(data, data_len, vars, instance_number_ptr);
}

void mqtt_send_rpmack(uint8_t* data, int data_len, GlobalVar* vars, uint32_t instance_number)
{
    char* instance_number_ptr = (char*)malloc(13*sizeof(char));
    snprintf(instance_number_ptr, 13, "%d%d", 1, instance_number);

    send_data(data, data_len, vars, instance_number_ptr);
}

void mqtt_send_ucovnotifica(uint8_t* data, int data_len, GlobalVar* vars, uint32_t instance_number)
{
    char* instance_number_ptr = (char*)malloc(13*sizeof(char));
    snprintf(instance_number_ptr, 13, "%d%d", 2, instance_number);

    send_data(data, data_len, vars, instance_number_ptr);
}

void mqtt_send_heartbeat(GlobalVar* vars)
{
    uint8_t* heartbeat = calloc(2, sizeof(uint8_t));
    snprintf((char*) heartbeat, 2, "%d", 0);

    char* instance_number_ptr = (char*)malloc(2*sizeof(char));
    snprintf(instance_number_ptr, 2, "%d", 3);

    send_data(heartbeat, 1, vars, instance_number_ptr);
}
