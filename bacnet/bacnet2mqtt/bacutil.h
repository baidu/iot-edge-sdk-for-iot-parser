#ifndef INF_BCE_IOT_BAC2MQTT_BACUTIL_H
#define INF_BCE_IOT_BAC2MQTT_BACUTIL_H


#include "bacenum.h"

BACNET_OBJECT_TYPE str2BacObjectType(char* str);

BACNET_PROPERTY_ID str2PropertyId(char* str);


#endif