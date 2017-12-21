#ifndef INF_BCE_IOT_BAC2MQTT_DATA_H
#define INF_BCE_IOT_BAC2MQTT_DATA_H

#include <stdint.h>
#include <time.h>
#include <MQTTClient.h>
#include <stdlib.h>

#include "bacenum.h"
#include "bacdef.h"
#include "thread.h"
#include "requestlist.h"

#define DEVICE_NUM_MAX 255

// constants
enum
{
    MAX_LEN = 256,
    BUFF_LEN = 2048,
    MAX_PROPERTY_PER_MQTT_MSG = 50
};

typedef struct
{
    char* endpoint;
    char* pub_iam;
    char* pub_rpmack;
    char* pub_covnotice;
    char* pub_heartbeat;
    char* sub_topic;
    char* sub_config;
    char* sub_whois;
    char* sub_rpm;
    char* sub_wp;
    char* user;
    char* password;

    time_t last_heartbeat_time;
} MqttInfo;


// bacnet property identifier
typedef struct
{
    BACNET_OBJECT_TYPE objectType;
    uint32_t objectInstance;
    BACNET_PROPERTY_ID property;
    uint32_t index;	// -1: no index; 0: array size; BACNET_ARRAY_ALL: all elements
} BacProperty;

BacProperty* new_bac_property();

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

PullPolicy* new_pull_policy() ;

// bacnet device
typedef struct
{
    uint32_t instanceNumber;
    char* ipOrInterface;	// default NULL, useful only if there are multiple network adapters
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

typedef enum {
    UN_SUPPORT_SUB = 0,
    SUPPORT_SUB = 1,
    NEED_TO_SUB = 2,
    SUBSCRIBED = 3,
    UN_NEED_TO_CANCEL = 4,
    NEED_TO_CANCEL_OBJECT = 5,
    NEED_TO_CANCEL_PROPERTY = 6,
    UN_SUPPORT_SUB_OBJECT = 7,
    UN_SUPPORT_SUB_PROPERTY = 8
} SUBSCRIBE_STATE;

typedef struct BacObject_t
{
    BACNET_OBJECT_TYPE objectType;
    uint32_t objectInstance;
    char need_to_read;

    SUBSCRIBE_STATE support_cov;
    SUBSCRIBE_STATE un_support_state;

    time_t subscribe_time;

    // if support_cov is true , when get a COV notification , it will be updated
    time_t last_read_time;

    struct BacObject_t* next;
} BacObject;

BacObject* new_bac_object();

typedef enum {
    INIT_STATE = 0,
    READ_OBJECT_LIST = 1,
    SUBSCRIBE_ALL_OBJECT = 2,
    CANCEL_SUBSCRIBE_OBJECT = 3,
    CANCEL_AND_RE_SUBSCRIBE = 4,
    ENTER_MAIN_LOOP = 5
} DEVICE_CURRENT_STATE;

typedef struct BacDevice2_t
{
    uint32_t instanceNumber;
    time_t last_discover_time;

    char update_objects;
    uint32_t objects_size;
    uint32_t next_index;
    DEVICE_CURRENT_STATE current_state;

    char disable;
    char send_next;
    // invokeId also is the index of this device in devices_header
    uint8_t invokeId;
    time_t last_iam_time;
    time_t last_ack_time;

    mutex_type request_list_mutex;
    REQUEST_LIST request_list;

    BacObject* objects_header;

    BacObject* handle_next_object;
} BacDevice2;

BacDevice2* new_bac_device2();

// all bacnet devices
typedef struct
{
    int devices_num;

    // actaully an array of pointer to BacDevice2_t, eg BacDevice2_t* devices_header[]
    struct BacDevice2_t** devices_header;
} AllDevices;

typedef enum {
    NO_SUBSCRIBE = 0,
    SUBSCRIBE_OBJECT = 1,
    SUBSCRIBE_PORPERTY = 2,
    SUBSCRIBE_PROPERTY_WITH_COV_INCREMENT = 3
} SUBSCRIBE_TYPE;

typedef struct
{
    int poll_interval;
    int poll_interval_cov;
    int who_is_interval;
    int heart_beat_interval;
    int object_discover_interval;
    int subscribe_duration;

    SUBSCRIBE_TYPE subscribe_type;
    double cov_increment;
} Interval;

typedef struct
{
    // mqtt info
    MqttInfo g_mqtt_info;

    MQTTClient g_mqtt_client;
    mutex_type g_mqtt_client_mutex;

    int g_gateway_connected;
    mutex_type g_gateway_mutex;

    // interval
    Interval g_interval;

    // all bacnet devices handled by this gateway
    AllDevices g_all_devices;

    // bacnet data sampling config
    Bac2mqttConfig g_config;
    mutex_type g_policy_lock;

    int g_policy_updated;
    mutex_type g_policy_update_lock;

    // reset gateway
    char g_reset_gateway;
} GlobalVar;

#endif
