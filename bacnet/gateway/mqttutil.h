
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

void mqtt_cleanup(GlobalVar* vars);

void mqtt_send_heartbeat(GlobalVar* vars);

void mqtt_send_iam(uint8_t* data, int data_len, GlobalVar* vars);

void mqtt_send_rpmack(uint8_t* data, int data_len, GlobalVar* vars, uint32_t instance_number);

void mqtt_send_ucovnotifica(uint8_t* data, int data_len, GlobalVar* vars, uint32_t instance_number);

#endif
