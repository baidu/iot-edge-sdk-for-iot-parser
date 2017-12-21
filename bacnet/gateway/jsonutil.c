#include "jsonutil.h"

#include <cjson/cJSON.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "bacutil.h"
#include "common.h"

void copy_str_value_from_json(char** dest, cJSON* json, char* key, int maxLen)
{
    *dest = NULL;
    if (key == NULL)
    {
        printf("ERROR:key is null\n");
        return;
    }
    if (!cJSON_HasObjectItem(json, key))
    {
        printf("ERROR:key %s does not exist\n", key);
        return;
    }
    cJSON* val = cJSON_GetObjectItem(json, key);
    if (cJSON_IsNull(val))
    {
        printf("WARN:key %s is an null node\n", key);
        return;
    }
    char* tmp = val->valuestring;
    if (tmp == NULL)
    {
        printf("ERROR:string value of %s is null\n", key);
        return;
    }
    *dest = (char*) malloc(strlen(tmp) + 1);
    mystrncpy(*dest, tmp, maxLen);

}

void json2_mqtt_info(const char* str, MqttInfo* info)
{
    if (str == NULL)
    {
        return;
    }

    cJSON* root = cJSON_Parse(str);
    if (root == NULL)
    {
        printf("ERROR:the config string is not a valid json object, content=%s\n", str);
        return;
    }

    copy_str_value_from_json(&info->endpoint, root, "endpoint", MAX_LEN);

    char* pub_topic = NULL;
    char* sub_topic = NULL;
    copy_str_value_from_json(&sub_topic, root, "cmdTopic", MAX_LEN);
    copy_str_value_from_json(&pub_topic, root, "dataTopic", MAX_LEN);
    info->pub_iam = create_topic(pub_topic, "iam");
    info->pub_rpmack = create_topic(pub_topic, "rpmack");
    info->pub_covnotice = create_topic(pub_topic, "covnotice");
    info->pub_heartbeat = create_topic(pub_topic, "heartbeat");
    info->sub_topic = create_topic(sub_topic, "#");
    info->sub_config = create_topic(sub_topic, "config");
    info->sub_whois = create_topic(sub_topic, "whois");
    info->sub_rpm = create_topic(sub_topic, "rpm");
    info->sub_wp = create_topic(sub_topic, "wp");
    free(pub_topic);
    free(sub_topic);

    copy_str_value_from_json(&info->user, root, "username", MAX_LEN);
    copy_str_value_from_json(&info->password, root, "password", MAX_LEN);

    cJSON_Delete(root);
}

int json2_bac2_mqtt_config(const char* str, Bac2mqttConfig* config)
{
    if (str == NULL)
    {
        return -1;
    }

    cJSON* root = cJSON_Parse(str);
    if (root == NULL)
    {
        printf("the config string is not a valid json object, content=%s\n", str);
        return -1;
    }

    config->bdBacVer = json_int(root, "bdBacVer");

    // device
    cJSON* device = cJSON_GetObjectItem(root, "device");
    config->device.instanceNumber = (uint32_t) json_int(device, "instanceNumber");
    config->device.ipOrInterface = NULL;
    if (cJSON_HasObjectItem(device, "ipOrInterface"))
    {
        cJSON* ip = cJSON_GetObjectItem(device, "ipOrInterface");
        if (ip != NULL && !cJSON_IsNull(ip))
        {
            char* ipStr = ip->valuestring;
            config->device.ipOrInterface = (char*) malloc(sizeof(char) * strlen(ipStr) + 1);
            mystrncpy(config->device.ipOrInterface, ipStr, strlen(ipStr) + 1);
        }
    }

    // pullPolices
    cJSON* pullPolices = cJSON_GetObjectItem(root, "pullPolices");
    int pullPolicyNum = cJSON_GetArraySize(pullPolices);
    config->policyHeader.next = NULL;
    time_t now = time(NULL);
    int i = 0;
    for (i = 0; i < pullPolicyNum; i++)
    {
        cJSON* policyNode = cJSON_GetArrayItem(pullPolices, i);
        PullPolicy* policy = new_pull_policy(); // (PullPolicy*) malloc(sizeof(PullPolicy));
        policy->targetInstanceNumber = (uint32_t) json_int(policyNode, "targetInstanceNumber");
        policy->interval = json_int(policyNode, "interval");
        policy->nextRun = policy->interval + now;

        cJSON* propertyArray = cJSON_GetObjectItem(policyNode, "properties");
        policy->propNum = cJSON_GetArraySize(propertyArray);
        policy->properties = (BacProperty**) malloc(policy->propNum * sizeof(BacProperty*));
        // init them
        int j = 0;
        for (j = 0; j < policy->propNum; j++)
        {
            cJSON* propNode = cJSON_GetArrayItem(propertyArray, j);
            BacProperty* property = new_bac_property();
            property->objectType = str2_bac_object_type(json_string(propNode, "objectType"));
            property->objectInstance = (uint32_t) json_int(propNode, "objectInstance");
            property->property = str2_property_id(json_string(propNode, "property"));

            if (cJSON_HasObjectItem(propNode, "index"))
            {
                property->index = (uint32_t) json_int(propNode, "index");
            }

            policy->properties[j] = property;

        }

        policy->next = config->policyHeader.next;
        config->policyHeader.next = policy;
    }

    cJSON_Delete(root);

    return 0;
}

int is_string_valid_json(const char* str)
{
    cJSON* root = cJSON_Parse(str);
    if (root == NULL)
    {
        return 0;
    }
    cJSON_Delete(root);

    return 1;
}
