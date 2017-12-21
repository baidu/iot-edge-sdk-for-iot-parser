#ifndef INF_BCE_IOT_BAC2MQTT_REQUESTLIST_H
#define INF_BCE_IOT_BAC2MQTT_REQUESTLIST_H

typedef enum {
    RPM_REQUEST = 0,
    WPM_REQUEST = 1,
    SUBSCRIBE_OBJECT_REQUEST = 2,
    SUBSCRIBE_PROPERTY_REQUEST = 3
} REQUEST_TYPE;

typedef struct request_data_t{
    void* request;
    REQUEST_TYPE type;
    struct request_data_t* next;
} REQUEST_DATA;

typedef struct {
    REQUEST_DATA* head;
    REQUEST_DATA* tail;
} REQUEST_LIST;

#endif // INF_BCE_IOT_BAC2MQTT_REQUEST_LIST_H