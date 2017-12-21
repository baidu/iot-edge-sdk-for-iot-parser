#ifndef INF_BCE_IOT_BAC2MQTT_JSONUTIL_H
#define INF_BCE_IOT_BAC2MQTT_JSONUTIL_H

#include "data.h"

void json2_mqtt_info(const char* str, MqttInfo* info);

// return 0, success; failed otherwise
int json2_bac2_mqtt_config(const char* str, Bac2mqttConfig* config);

int is_string_valid_json(const char* str);

// char* bac_data2_json(BacValueOutput* data, BacDevice* thisDevice, BacValueOutput** nextPage);
#endif
