

#ifndef INF_BCE_IOT_BAC2MQTT_MQTTUTIL_H
#define INF_BCE_IOT_BAC2MQTT_MQTTUTIL_H

#include <MQTTClient.h>

#include "data.h"

typedef void (*connection_lost_fun)(void*, char*);

typedef void (*delivered_fun)(void*, MQTTClient_deliveryToken);

typedef int (*msg_arrived_fun)(void*, char*, int, MQTTClient_message*);


void start_mqtt_client(GlobalVar* vars, 
	connection_lost_fun connection_lost, 
	msg_arrived_fun msg_arrived,
	delivered_fun delivered);

int sendData(char* data, GlobalVar* vars);

void mqtt_cleanup(GlobalVar* vars);

#endif