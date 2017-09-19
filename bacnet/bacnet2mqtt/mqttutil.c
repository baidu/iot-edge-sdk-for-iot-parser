
#include "mqttutil.h"

#include <string.h>
#include <stdlib.h>

#include "common.h"

// cache at most 2048 messages, ring buffer
enum {MSG_BUF_SIZE = 2048};
char* g_dataToSend[MSG_BUF_SIZE];
int g_buffHead = 0;
int g_buffSize = 0;
const char* const PEM_FILE = "root_cert.pem";
MQTTClient_SSLOptions g_sslopts = MQTTClient_SSLOptions_initializer;

char LOG_BUFF[BUFF_LEN] = {0};


int isBufferFull() {
	return g_buffSize >= MSG_BUF_SIZE;
}

int isBufferEmpty() {
	return g_buffSize <= 0;
}

int putBuffData(char* data) {
	// if full, them overwrite the oldest one
	int idx = (g_buffSize + g_buffHead) % MSG_BUF_SIZE;
	if (isBufferFull()) {
		free(g_dataToSend[idx]);
		g_dataToSend[idx] = data;
		g_buffHead = (g_buffHead + 1) % MSG_BUF_SIZE;
		g_buffSize = MSG_BUF_SIZE;
		return 0;
	} else {
		g_dataToSend[idx] = data;
		g_buffSize++;
		return 1;
	}
}

char* getBuffData() {
	if (isBufferEmpty()) {
		return NULL;
	} else {
		char* ret = g_dataToSend[g_buffHead];
		g_buffHead = (g_buffHead + 1) % MSG_BUF_SIZE;
		g_buffSize--;
		return ret;
	}
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
	delivered_fun delivered) {
	printf("connecting gateway to cloud...\n");
	printf("endpoint:%s\n", vars->g_mqtt_info.endpoint);
	printf("user:%s\n", vars->g_mqtt_info.user);
	printf("sub config from topic:%s\n", vars->g_mqtt_info.configTopic);
	printf("pub data to topic:%s\n", vars->g_mqtt_info.dataTopic);

	Thread_lock_mutex((vars->g_mqtt_client_mutex));
    MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;

    char clientid[MAX_LEN];
    snprintf(clientid, MAX_LEN, "bacnetGW%lld", (long long)time(NULL));
    MQTTClient_create(&(vars->g_mqtt_client), vars->g_mqtt_info.endpoint, clientid,
        MQTTCLIENT_PERSISTENCE_NONE, NULL);

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
    if (vars->g_mqtt_info.controlTopic != NULL && strlen(vars->g_mqtt_info.controlTopic) > 0) {
    	char* topics[2];
    	topics[0] = vars->g_mqtt_info.configTopic;
    	topics[1] = vars->g_mqtt_info.controlTopic;
    	int qoss[2];
    	qoss[0] = 0;
    	qoss[1] = 0;
    	MQTTClient_subscribeMany(vars->g_mqtt_client, 2, topics, qoss);
    } else {
    	MQTTClient_subscribe(vars->g_mqtt_client, vars->g_mqtt_info.configTopic, 0);
	}
    vars->g_gateway_connected = 1;
    Thread_unlock_mutex((vars->g_mqtt_client_mutex));

    printf("gateway started!\n");
}

// return -1 if failed, 0 otherwise
int sendAMessage(char*data, MQTTClient client, char* topic) {
	MQTTClient_message pubmsg = MQTTClient_message_initializer;
	MQTTClient_deliveryToken delivery_token;
	pubmsg.payload = data;
	pubmsg.payloadlen = strlen(data);
	pubmsg.qos = 0;
	pubmsg.retained = 0;

	int rc = MQTTClient_publishMessage(client, topic, &pubmsg, &delivery_token);
	if (rc == MQTTCLIENT_SUCCESS) {
		MQTTClient_waitForCompletion(client, delivery_token, 1000L);
		return 0;
	} else {
		return -1; // failed to send
	}
}

int sendData(char* data, GlobalVar* vars) {
	// send data to the dataTopic

	// if mqtt client is not in a good state, then cache the data for later send
	if (data == NULL) {
		return -1;
	}

	if (vars == NULL || !MQTTClient_isConnected(vars->g_mqtt_client)) {
		putBuffData(data);
		log_debug("mqtt client is not connected, caching the data");
		return 0;
	}

	// send the data
	int rc = sendAMessage(data, vars->g_mqtt_client, vars->g_mqtt_info.dataTopic);
	if (rc == 0) {
		free(data);

		// send the buffered data if any
		char* buffData = NULL;
		int cnt = 0;
		while (rc == 0 && ! isBufferEmpty()) {
			buffData = getBuffData();
			rc = sendAMessage(buffData, vars->g_mqtt_client, vars->g_mqtt_info.dataTopic);
			if (rc == 0) {
				cnt++;
				free(buffData);
				buffData = NULL;
			} else {
				// send mqtt message failed
				putBuffData(buffData);
				break;
			}
		}
		if (cnt > 0) {
			snprintf(LOG_BUFF, BUFF_LEN, "sent %d message from cache", cnt);
			log_debug(LOG_BUFF);
		}

	} else {
		// cache the data, and mark the mqtt client need reconnect
		snprintf(LOG_BUFF, BUFF_LEN, 
			"failed to send a mqtt message, return code=%d. Caching it for later sending", rc);
		log_debug(LOG_BUFF);
		putBuffData(data);
		Thread_lock_mutex( vars->g_gateway_mutex);
		vars->g_gateway_connected = 0;
		Thread_unlock_mutex( vars->g_gateway_mutex);
	}

	return 0;
}

void mqtt_cleanup(GlobalVar* vars) {
	MQTTClient_disconnect(vars->g_mqtt_client, 500);
}
