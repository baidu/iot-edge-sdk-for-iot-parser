#ifndef INF_BCE_IOT_BAC2MQTT_JSONUTIL_H
#define INF_BCE_IOT_BAC2MQTT_JSONUTIL_H

#include "data.h"
#include "baclib.h"

void json2MqttInfo(const char* str, MqttInfo* info);

// return 0, success; failed otherwise
int json2Bac2mqttConfig(const char* str, Bac2mqttConfig* config);

int isStringValidJson(const char* str);

char* bacData2Json(BacValueOutput* data, BacDevice* thisDevice, BacValueOutput** nextPage);
#endif