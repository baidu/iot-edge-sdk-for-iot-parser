#ifndef INF_BCE_IOT_BAC2MQTT_BACLIB_H
#define INF_BCE_IOT_BAC2MQTT_BACLIB_H

#include "data.h"
#include "jsonutil.h"

int start_local_bacnet_device(Bac2mqttConfig* pconfig);

// int bind_bac_device_address(Bac2mqttConfig* pconfig);

// pass the global variables pointer into this lib
void set_global_vars(GlobalVar* pVars);

void receive_and_handle();

void free_device(BacDevice2* device, char need_to_un_subscribe);

char is_analog_object(BACNET_OBJECT_TYPE object);

// int issue_read_property_multiple(PullPolicy* pPolicy);

void send_whois_immediately();

void send_rpm_immediately(cJSON* root);

void send_wpm_immediately(cJSON* root);

void send_read_struct_list(BacDevice2* device);

void handle_for_unexpected(uint8_t invoke_id);

void subscribe_unconfirmed(BacDevice2* device, BacObject* object);

void subscribe_if_need(BacDevice2* device);

void cancel_subscribe_object_if_need(BacDevice2* device);

void add_request(BacDevice2* device, REQUEST_DATA* request);

void add_request_if_need(BacDevice2* device);

void send_next_request(BacDevice2* device);

void clean_request(BacDevice2* device);

typedef struct BacValueOutput_t
{
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
