
#include "bacutil.h"
#include <string.h>
#include "common.h"

static char LOG_BUFF[BUFF_LEN] = {0};

BACNET_OBJECT_TYPE str2BacObjectType(char* str) {
	if (strcmp("ANALOG_INPUT", str) == 0) {
		return OBJECT_ANALOG_INPUT;
	} else if (strcmp("ANALOG_OUTPUT", str) == 0) {
		return OBJECT_ANALOG_OUTPUT;
	} else if (strcmp("ANALOG_VALUE", str) == 0) {
		return OBJECT_ANALOG_VALUE;
	} else if (strcmp("BINARY_INPUT", str) == 0) {
		return OBJECT_BINARY_INPUT;
	} else if (strcmp("BINARY_OUTPUT", str) == 0) {
		return OBJECT_BINARY_OUTPUT;
	} else if (strcmp("BINARY_VALUE", str) == 0) {
		return OBJECT_BINARY_VALUE;
	} else if (strcmp("MULTI_STATE_OUTPUT", str) == 0) {
		return OBJECT_MULTI_STATE_OUTPUT;
	} else if (strcmp("MULTI_STATE_INPUT", str) == 0) {
		return OBJECT_MULTI_STATE_INPUT;
	} else if (strcmp("MULTI_STATE_VALUE", str) == 0) {
		return OBJECT_MULTI_STATE_VALUE;
	} 

	snprintf(LOG_BUFF, BUFF_LEN, "Unsupported object type: %s", str);
	log_debug(LOG_BUFF);
	// NOT SUPPORTED TYPE
	return MAX_BACNET_OBJECT_TYPE;
}

BACNET_PROPERTY_ID str2PropertyId(char* str) {
	if (strcmp("PRESENT_VALUE", str) == 0) {
		return PROP_PRESENT_VALUE;
	} else if (strcmp("DESCRIPTION", str) == 0) {
		return PROP_DESCRIPTION;
	} else if (strcmp("EVENT_STATE", str) == 0) {
		return PROP_EVENT_STATE;
	} else if (strcmp("OBJECT_NAME", str) == 0) {
		return PROP_OBJECT_NAME;
	} if (strcmp("OUT_OF_SERVICE", str) == 0) {
		return PROP_OUT_OF_SERVICE;
	} if (strcmp("RELIABILITY", str) == 0) {
		return PROP_RELIABILITY;
	} if (strcmp("STATUS_FLAGS", str) == 0) {
		return PROP_STATUS_FLAGS;
	} if (strcmp("TIME_DELAY", str) == 0) {
		return PROP_TIME_DELAY;
	} if (strcmp("PRIORITY_ARRAY", str) == 0) {
		return PROP_PRIORITY_ARRAY;
	} if (strcmp("STATE_TEXT", str) == 0) {
		return PROP_STATE_TEXT;
	} 

	snprintf(LOG_BUFF, BUFF_LEN, "Unsupported property id: %s", str);
	log_debug(LOG_BUFF);
	return MAX_BACNET_PROPERTY_ID;
}