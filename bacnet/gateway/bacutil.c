#include "bacutil.h"
#include <string.h>
#include "common.h"

static char s_log_buff[BUFF_LEN] = {0};

BACNET_OBJECT_TYPE str2_bac_object_type(char* str)
{
    if (strcmp("ANALOG_INPUT", str) == 0)
    {
        return OBJECT_ANALOG_INPUT;
    }
    else if (strcmp("ANALOG_OUTPUT", str) == 0)
    {
        return OBJECT_ANALOG_OUTPUT;
    }
    else if (strcmp("ANALOG_VALUE", str) == 0)
    {
        return OBJECT_ANALOG_VALUE;
    }
    else if (strcmp("BINARY_INPUT", str) == 0)
    {
        return OBJECT_BINARY_INPUT;
    }
    else if (strcmp("BINARY_OUTPUT", str) == 0)
    {
        return OBJECT_BINARY_OUTPUT;
    }
    else if (strcmp("BINARY_VALUE", str) == 0)
    {
        return OBJECT_BINARY_VALUE;
    }
    else if (strcmp("MULTI_STATE_OUTPUT", str) == 0)
    {
        return OBJECT_MULTI_STATE_OUTPUT;
    }
    else if (strcmp("MULTI_STATE_INPUT", str) == 0)
    {
        return OBJECT_MULTI_STATE_INPUT;
    }
    else if (strcmp("MULTI_STATE_VALUE", str) == 0)
    {
        return OBJECT_MULTI_STATE_VALUE;
    }
    else if (strcmp("DEVICE", str) == 0)
    {
        return OBJECT_DEVICE;
    }
    else if (strcmp("LIFE_SAFETY_POINT", str) == 0)
    {
        return OBJECT_LIFE_SAFETY_POINT;
    }
    else if (strcmp("LOAD_CONTROL", str) == 0)
    {
        return OBJECT_LOAD_CONTROL;
    }
    else if (strcmp("POSITIVE_INTEGER_VALUE", str) == 0)
    {
        return OBJECT_POSITIVE_INTEGER_VALUE;
    }

    snprintf(s_log_buff, BUFF_LEN, "Unsupported object type: %s", str);
    log_debug(s_log_buff);
    // NOT SUPPORTED TYPE
    return MAX_BACNET_OBJECT_TYPE;
}

BACNET_PROPERTY_ID str2_property_id(char* str)
{
    if (strcmp("PRESENT_VALUE", str) == 0)
    {
        return PROP_PRESENT_VALUE;
    }
    else if (strcmp("DESCRIPTION", str) == 0)
    {
        return PROP_DESCRIPTION;
    }
    else if (strcmp("EVENT_STATE", str) == 0)
    {
        return PROP_EVENT_STATE;
    }
    else if (strcmp("OBJECT_NAME", str) == 0)
    {
        return PROP_OBJECT_NAME;
    }
    else if (strcmp("OUT_OF_SERVICE", str) == 0)
    {
        return PROP_OUT_OF_SERVICE;
    }
    else if (strcmp("RELIABILITY", str) == 0)
    {
        return PROP_RELIABILITY;
    }
    else if (strcmp("STATUS_FLAGS", str) == 0)
    {
        return PROP_STATUS_FLAGS;
    }
    else if (strcmp("TIME_DELAY", str) == 0)
    {
        return PROP_TIME_DELAY;
    }
    else if (strcmp("PRIORITY_ARRAY", str) == 0)
    {
        return PROP_PRIORITY_ARRAY;
    }
    else if (strcmp("STATE_TEXT", str) == 0)
    {
        return PROP_STATE_TEXT;
    }
    else if (strcmp("TYPES_SUPPORTED", str) == 0)
    {
        return PROP_PROTOCOL_OBJECT_TYPES_SUPPORTED;
    }
    else if (strcmp("SERVICES_SUPPORTED", str) == 0)
    {
        return PROP_PROTOCOL_SERVICES_SUPPORTED;
    }
    else if (strcmp("VENDOR_NAME", str) == 0)
    {
        return PROP_VENDOR_NAME;
    }
    else if (strcmp("VENDOR_IDENTIFIER", str) == 0)
    {
        return PROP_VENDOR_IDENTIFIER;
    }
    else if (strcmp("PROTOCAL_VERSION", str) == 0)
    {
        return PROP_PROTOCOL_VERSION;
    }
    else if (strcmp("PROTOCAL_REVISION", str) == 0)
    {
        return PROP_PROTOCOL_REVISION;
    }

    snprintf(s_log_buff, BUFF_LEN, "Unsupported property id: %s", str);
    log_debug(s_log_buff);
    return MAX_BACNET_PROPERTY_ID;
}
