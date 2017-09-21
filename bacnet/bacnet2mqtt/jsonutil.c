#include "jsonutil.h"

#include <cjson/cJSON.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bacutil.h"
#include "common.h"

void copyStrValueFromJson(char** dest, cJSON* json, char* key, int maxLen) {
	*dest = NULL;
	if (key == NULL) {
		printf("ERROR:key is null\n");
		return;
	}
	if (!cJSON_HasObjectItem(json, key)) {
		printf("ERROR:key %s does not exist\n", key);
		return;
	}
	cJSON* val = cJSON_GetObjectItem(json, key);
	if (cJSON_IsNull(val)) {
		printf("WARN:key %s is an null node\n", key);
		return;
	}
	char* tmp = val->valuestring;
    if (tmp == NULL) {
    	printf("ERROR:string value of %s is null\n", key);
    	return;
    }
    *dest = (char*) malloc(strlen(tmp) + 1);
    mystrncpy(*dest, tmp, maxLen);

}

void json2MqttInfo(const char* str, MqttInfo* info) {
	if (str == NULL) {
		return;
	}

	cJSON* root = cJSON_Parse(str);
    if (root == NULL)
    {
        printf("ERROR:the config string is not a valid json object, content=%s\n", str);
        return;
    }
    
    copyStrValueFromJson(&info->endpoint, root, "endpoint", MAX_LEN);
    copyStrValueFromJson(&info->configTopic, root, "configTopic", MAX_LEN);
    copyStrValueFromJson(&info->dataTopic, root, "dataTopic", MAX_LEN);
    copyStrValueFromJson(&info->controlTopic, root, "controlTopic", MAX_LEN);
    if (info->controlTopic == NULL) {
    	printf("INFO:control topic is null, control bacnet device is disabled now\n");
    }
    copyStrValueFromJson(&info->user, root, "user", MAX_LEN);
    copyStrValueFromJson(&info->password, root, "password", MAX_LEN);


    cJSON_Delete(root);
}

int json2Bac2mqttConfig(const char* str, Bac2mqttConfig* config) {
	if (str == NULL) {
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
    	if (ip != NULL && !cJSON_IsNull(ip)) {
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
    for (i = 0; i < pullPolicyNum; i++) {
    	cJSON* policyNode = cJSON_GetArrayItem(pullPolices, i);
    	PullPolicy* policy = newPullPolicy(); // (PullPolicy*) malloc(sizeof(PullPolicy));
    	policy->targetInstanceNumber = (uint32_t) json_int(policyNode, "targetInstanceNumber");
    	policy->interval = json_int(policyNode, "interval");
    	policy->nextRun = policy->interval + now;

    	cJSON* propertyArray = cJSON_GetObjectItem(policyNode, "properties");
    	policy->propNum = cJSON_GetArraySize(propertyArray);
    	policy->properties = (BacProperty**) malloc(policy->propNum * sizeof(BacProperty*));
    	// init them 
    	int j = 0;
    	for (j = 0; j < policy->propNum; j++) {
    		cJSON* propNode = cJSON_GetArrayItem(propertyArray, j);
    		BacProperty* property = newBacProperty(); 
    		property->objectType = str2BacObjectType(json_string(propNode, "objectType"));
    		property->objectInstance = (uint32_t) json_int(propNode, "objectInstance");
    		property->property = str2PropertyId(json_string(propNode, "property"));

    		if (cJSON_HasObjectItem(propNode, "index")) {
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

int isStringValidJson(const char* str) {
	cJSON* root = cJSON_Parse(str);
    if (root == NULL)
    {
        return 0;
    }
    cJSON_Delete(root);

    return 1;
}


char* bacData2Json(BacValueOutput* bacDataList, BacDevice* thisDevice, BacValueOutput** nextPage) {
	// convert data into format like:
// {
//     "bdBacVer": 1,
//     "device": {
//         "instanceNumber": 4194303,
//         "ip": "192.168.0.2",
//         "broadcastIp": "192.168.0.255"
//     },
//     "ts": 1493021816,
//     "data": [
//         {
//             "id": "_117_ANALOG_INPUT_0_PRESENT_VALUE_0",
//             "instance": 117,
//             "objType": "ANALOG_INPUT",
//             "objInstance": 0,
//             "propertyId": "PRESENT_VALUE",
//             "index": -1,
//             "type": "Double",
//             "value": 12.3
//         }
//     ]
// }

    *nextPage = NULL;  // default no next page

	cJSON* root = cJSON_CreateObject(); 
	cJSON_AddNumberToObject(root, "bdBacVer", 1);
    cJSON* device = NULL;
    cJSON_AddItemToObject(root, "device", device = cJSON_CreateObject());
    cJSON_AddNumberToObject(device, "instanceNumber", thisDevice->instanceNumber);
    if (thisDevice->ipOrInterface) {
        cJSON_AddStringToObject(device, "ipOrInterface", thisDevice->ipOrInterface);
    } else {
        cJSON_AddItemToObject(device, "ipOrInterface", cJSON_CreateNull());
    }

    cJSON_AddNumberToObject(root, "ts", time(NULL));

    cJSON* data = NULL;
    cJSON_AddItemToObject(root, "data", data = cJSON_CreateArray());
    int cnt = 0;
    while (bacDataList != NULL && cnt++ < MAX_PROPERTY_PER_MQTT_MSG) {
    	cJSON* elem = cJSON_CreateObject();
    	cJSON_AddStringToObject(elem, "id", bacDataList->id);
    	cJSON_AddNumberToObject(elem, "instance", bacDataList->instanceNumber);
    	cJSON_AddStringToObject(elem, "objType", bacDataList->objectType);
    	cJSON_AddNumberToObject(elem, "objInstance", bacDataList->objectInstance);
    	cJSON_AddStringToObject(elem, "propertyId", bacDataList->propertyId);
    	cJSON_AddNumberToObject(elem, "index", bacDataList->index);
    	cJSON_AddStringToObject(elem, "type", bacDataList->type);
    	cJSON_AddStringToObject(elem, "value", bacDataList->value);

    	cJSON_AddItemToArray(data, elem);

    	bacDataList = bacDataList->next;
    }
    if (bacDataList != NULL && cnt >= MAX_PROPERTY_PER_MQTT_MSG) {
        *nextPage = bacDataList;
    }
    
    char* text = cJSON_Print(root);
    cJSON_Delete(root);

    return text;
}