#ifndef INF_BCE_IOT_BAC2MQTT_DATA_H
#define INF_BCE_IOT_BAC2MQTT_DATA_H

#include <stdint.h>
#include <time.h>
#include <MQTTClient.h>
#include <stdlib.h>

#include "bacenum.h"
#include "bacdef.h"
#include "thread.h"

// constants
enum {
	MAX_LEN = 256,
	BUFF_LEN = 2048,
	MAX_PROPERTY_PER_MQTT_MSG = 50
};

typedef struct
{
	char* endpoint;
	char* configTopic;
    char* dataTopic;
    char* controlTopic;	// topic to receive control cmd from cloud, null means disable controlling
    char* user;
    char* password;
} MqttInfo;


// bacnet property identifier
typedef struct
{
	BACNET_OBJECT_TYPE objectType;
	uint32_t objectInstance;
	BACNET_PROPERTY_ID property;
	uint32_t index;	// -1: no index; 0: array size; BACNET_ARRAY_ALL: all elements
} BacProperty;

BacProperty* newBacProperty() ;

// data sampling polic
typedef struct PullPolicy_t
{
	///////////////////////////////
	// bacnet runtime properties
	BACNET_ADDRESS rtTargetAddress;
	int rtAddressBund;
	uint8_t rtReqInvokeId;
	///////////////////////////////


	uint32_t targetInstanceNumber;
	int interval;
	time_t nextRun;	// ts that this policy is schedule to run

	int propNum; // number of BacProperty in properites fields

	// actaully an array of pointer to BacProperty, eg BacProperty* properties[]
	BacProperty ** properties;	

	struct PullPolicy_t* next;
} PullPolicy;

PullPolicy* newPullPolicy() ;

// bacnet device
typedef struct 
{
	uint32_t instanceNumber;
	char* ip;	// default NULL
	char* broadcastIp;	// default NULL
} BacDevice;

// config
typedef struct 
{
	// two runtime properties, not received from cloud
	int rtConfLoaded;
	int rtDeviceStarted;

	int bdBacVer;
	BacDevice device;
	
	PullPolicy policyHeader;
} Bac2mqttConfig;


typedef struct
{
	// mqtt info
	MqttInfo g_mqtt_info;

	MQTTClient g_mqtt_client;
	mutex_type g_mqtt_client_mutex;

	int g_gateway_connected;
	mutex_type g_gateway_mutex;

	// bacnet data sampling config
	Bac2mqttConfig g_config;
	mutex_type g_policy_lock;

	int g_policy_updated;
	mutex_type g_policy_update_lock;
} GlobalVar;

#endif
