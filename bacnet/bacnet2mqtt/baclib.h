
#ifndef INF_BCE_IOT_BAC2MQTT_BACLIB_H
#define INF_BCE_IOT_BAC2MQTT_BACLIB_H

#include "data.h"

int start_local_bacnet_device(Bac2mqttConfig* pconfig);

int bind_bac_device_address(Bac2mqttConfig* pconfig);

// pass the global variables pointer into this lib
void set_global_vars(GlobalVar* pVars);

int issue_read_property_multiple(PullPolicy* pPolicy);

typedef struct BacValueOutput_t {
	char* id;
	uint32_t instanceNumber;	// TODO: need to find the instance number and upload to cloud
	const char* objectType;
	int objectInstance;
	const char* propertyId;
	uint32_t index;
	const char* type;
	char* value;
	struct BacValueOutput_t* next;
} BacValueOutput;

#endif