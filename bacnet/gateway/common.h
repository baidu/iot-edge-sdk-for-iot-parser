#ifndef INF_BCE_IOT_BAC2MQTT_COMMON_H
#define INF_BCE_IOT_BAC2MQTT_COMMON_H

#include "data.h"
#include <cjson/cJSON.h>

// common function section
long read_file_as_string(char const* path, char** buf);

void toggle_debug();

void log_debug(char* msg);

void mystrncpy(char* desc, const char* src, int len);

int json_bool(cJSON* root, char* item);

int json_int(cJSON* root, char* item);

double json_double(cJSON* root, char* item);

char* json_string(cJSON* root, char* item);

void sleep_ms(int ms);

char* create_topic(const char* topic_prefix, const char* topic_suffix);

#endif
