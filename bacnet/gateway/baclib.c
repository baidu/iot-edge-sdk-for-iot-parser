/*************************************************************************
* Copyright (C) 2006 Steve Karg <skarg@users.sourceforge.net>
*
* Permission is hereby granted, free of charge, to any person obtaining
* a copy of this software and associated documentation files (the
* "Software"), to deal in the Software without restriction, including
* without limitation the rights to use, copy, modify, merge, publish,
* distribute, sublicense, and/or sell copies of the Software, and to
* permit persons to whom the Software is furnished to do so, subject to
* the following conditions:
*
* The above copyright notice and this permission notice shall be included
* in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*
*********************************************************************/

/* command line tool that sends a BACnet service, and displays the reply */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>       /* for time */
#include <cjson/cJSON.h>

#define PRINT_ENABLED 1

#include "bacdef.h"
#include "bactext.h"
#include "bacerror.h"
#include "address.h"
#include "npdu.h"
#include "apdu.h"
#include "device.h"
#include "datalink.h"
#include "whois.h"
#include "tsm.h"
#include "cov.h"
#include "iam.h"
#include "wpm.h"
/* some demo stuff needed */
#include "handlers.h"
#include "client.h"
#include "dlenv.h"
#include "baclib.h"
#include "jsonutil.h"
#include "mqttutil.h"
#include "common.h"
#include "bacutil.h"

static GlobalVar* s_vars = NULL;
static char s_log_buff[BUFF_LEN] = {0};
static char s_rpm_object_num_max = 50;

uint8_t g_rx_buf1[MAX_MPDU] = { 0 };
uint16_t g_pdu_len = 0;

bool is_need_to_subscribe(BacObject* object)
{
    bool need = true;
    if(s_vars->g_interval.subscribe_type == SUBSCRIBE_OBJECT
        && object->un_support_state == UN_SUPPORT_SUB_OBJECT)
    {
        need = false;
        return need;
    }

    if((s_vars->g_interval.subscribe_type == SUBSCRIBE_PORPERTY
        || s_vars->g_interval.subscribe_type == SUBSCRIBE_PROPERTY_WITH_COV_INCREMENT)
        && object->un_support_state == UN_SUPPORT_SUB_PROPERTY)
    {
        need = false;
        return need;
    }

    // if(s_vars->g_interval.subscribe_type == SUBSCRIBE_PROPERTY_WITH_COV_INCREMENT
    //     && !is_analog_object(object->objectType))
    // {
    //     need = false;
    //     return need;
    // }

    return need;
}

// this func only invoked at first time or re_discovery object list
void subscribe_if_need(BacDevice2* device)
{
    if(device == NULL)
    {
        return;
    }

    BacObject* next_subscribe_object = NULL;
    if(s_vars->g_interval.subscribe_type != NO_SUBSCRIBE)
    {
        if(device->handle_next_object == NULL)
        {
            device->handle_next_object = device->objects_header;
            next_subscribe_object = device->handle_next_object;
        }
        else
        {
            next_subscribe_object = device->handle_next_object->next;
        }
    
        while(next_subscribe_object)
        {
            if(next_subscribe_object->support_cov == NEED_TO_SUB)
            {
                if(!is_need_to_subscribe(next_subscribe_object))
                {
                    next_subscribe_object->support_cov = SUPPORT_SUB;
                    next_subscribe_object = next_subscribe_object->next;
                    continue;
                }

                // keep this object for handle unexpected message
                device->handle_next_object = next_subscribe_object;
                next_subscribe_object->support_cov = SUPPORT_SUB;
                subscribe_unconfirmed(
                        device,
                        next_subscribe_object);
                break;
            }
            next_subscribe_object = next_subscribe_object->next;
        }
    }
    
    if(next_subscribe_object == NULL)
    {
        // already subscribe for all objects which support subscribe
        // this device can enter main loop
        device->current_state = ENTER_MAIN_LOOP;
        device->handle_next_object = device->objects_header;
        device->disable = 0;
        device->send_next = 1;
    }
}

void cancel_subscribe_object_if_need(BacDevice2* device)
{
    if(device == NULL)
    {
        return;
    }

    BacObject* next_subscribe_object = NULL;
    if(device->handle_next_object == NULL)
    {
        device->handle_next_object = device->objects_header;
        next_subscribe_object = device->handle_next_object;
    }
    else
    {
        next_subscribe_object = device->handle_next_object->next;
    }

    while(next_subscribe_object)
    {
        if(next_subscribe_object->support_cov == NEED_TO_CANCEL_OBJECT
            || next_subscribe_object->support_cov == NEED_TO_CANCEL_PROPERTY)
        {
            device->handle_next_object = next_subscribe_object;
            // un-subscribe
            BACNET_SUBSCRIBE_COV_DATA *cov_data = calloc(1, sizeof(BACNET_SUBSCRIBE_COV_DATA));
            cov_data->monitoredObjectIdentifier.type = next_subscribe_object->objectType;
            cov_data->monitoredObjectIdentifier.instance
                = next_subscribe_object->objectInstance;
            cov_data->subscriberProcessIdentifier = 1;
            cov_data->cancellationRequest = true;
            cov_data->next = NULL;
            if(next_subscribe_object->support_cov == NEED_TO_CANCEL_PROPERTY)
            {
                cov_data->monitoredProperty.propertyIdentifier = PROP_PRESENT_VALUE;
                cov_data->monitoredProperty.propertyArrayIndex = BACNET_ARRAY_ALL;
                cov_data->monitoredProperty.next = NULL;
                my_tsm_logic_invokeID_set(device->invokeId);
                Send_COV_Subscribe_Property(device->instanceNumber, cov_data);
            }
            else
            {
                my_tsm_logic_invokeID_set(device->invokeId);
                Send_COV_Subscribe(device->instanceNumber, cov_data);
            }
            free(cov_data);
            if(device->current_state == CANCEL_AND_RE_SUBSCRIBE)
            {
                next_subscribe_object->support_cov = NEED_TO_SUB;
            }
            else
            {
                next_subscribe_object->support_cov = SUPPORT_SUB;
            }
            break;
        }
        next_subscribe_object = next_subscribe_object->next;
    }
    
    if(next_subscribe_object == NULL)
    {
        if(device->current_state == CANCEL_AND_RE_SUBSCRIBE)
        {
            device->handle_next_object = NULL;
            device->current_state = SUBSCRIBE_ALL_OBJECT;
            subscribe_if_need(device);
        }
        else
        {
            device->current_state = ENTER_MAIN_LOOP;
            device->handle_next_object = device->objects_header;
            device->disable = 0;
            device->send_next = 1;
        }
    }
}

void read_object_list_if_need(BacDevice2* device)
{
    if(device->next_index <= device->objects_size)
    {    
        my_tsm_logic_invokeID_set(device->invokeId);
        Send_Read_Property_Request(device->instanceNumber,
                        OBJECT_DEVICE, device->instanceNumber,
                        PROP_OBJECT_LIST, device->next_index);
    }
    else
    {
        device->handle_next_object = NULL;
        device->current_state = SUBSCRIBE_ALL_OBJECT;
        subscribe_if_need(device);
    }
}

void cleanup_read_access_data(BACNET_READ_ACCESS_DATA* header)
{
    BACNET_READ_ACCESS_DATA *rpm_object = NULL;
    BACNET_READ_ACCESS_DATA *old_rpm_object = NULL;
    BACNET_PROPERTY_REFERENCE *rpm_property = NULL;
    BACNET_PROPERTY_REFERENCE *old_rpm_property = NULL;

    rpm_object = header;
    while (rpm_object)
    {
        rpm_property = rpm_object->listOfProperties;
        while (rpm_property)
        {
            old_rpm_property = rpm_property;
            rpm_property = rpm_property->next;
            free(old_rpm_property);
        }
        old_rpm_object = rpm_object;
        rpm_object = rpm_object->next;
        free(old_rpm_object);
    }
}

void cleanup_write_access_data(BACNET_WRITE_ACCESS_DATA* header)
{
    BACNET_WRITE_ACCESS_DATA* wpm_object = NULL;
    BACNET_WRITE_ACCESS_DATA* old_wpm_object = NULL;
    BACNET_PROPERTY_VALUE* wpm_property = NULL;
    BACNET_PROPERTY_VALUE* old_wpm_property = NULL;

    wpm_object = header;
    while(wpm_object)
    {
        wpm_property = wpm_object->listOfProperties;
        while(wpm_property)
        {
            old_wpm_property = wpm_property;
            wpm_property = old_wpm_property->next;
            free(old_wpm_property);
        }
        old_wpm_object = wpm_object;
        wpm_object = old_wpm_object->next;
        free(old_wpm_object);
    }
}

void cancel_cov_subscribe(BacDevice2* device, BacObject* object)
{
    BACNET_SUBSCRIBE_COV_DATA *cov_data = calloc(1, sizeof(BACNET_SUBSCRIBE_COV_DATA));
    cov_data->monitoredObjectIdentifier.type = object->objectType;
    cov_data->monitoredObjectIdentifier.instance = object->objectInstance;
    cov_data->subscriberProcessIdentifier = 1;
    cov_data->cancellationRequest = true;
    cov_data->next = NULL;
    my_tsm_logic_invokeID_set(device->invokeId);
    Send_COV_Subscribe(device->instanceNumber, cov_data);
    free(cov_data);
}

void free_device(BacDevice2* device, char need_to_un_subscribe)
{
    BacObject* object = device->objects_header;
    BacObject* old_object = NULL;
    while(object)
    {
        if(need_to_un_subscribe == 1
            && object->support_cov == SUBSCRIBED
            && s_vars->g_interval.subscribe_type != NO_SUBSCRIBE)
        {
            // un-subscribe
            cancel_cov_subscribe(device, object);
        }
        old_object = object;
        object = old_object->next;
        free(old_object);
    }
    clean_request(device);
    free(device);
}

int free_invoke_id()
{
    // to get a invoke id, we must delete a device
    BacDevice2** header = s_vars->g_all_devices.devices_header;
    int index = 1;
    time_t last_iam_time = header[index]->last_iam_time;
    int i = 0;
    for(i = index + 1; i <= s_vars->g_all_devices.devices_num; i++)
    {
        if (header[i]->last_iam_time < last_iam_time)
        {
            last_iam_time = header[i]->last_iam_time;
            index = i;
        }
    }

    free_device(header[index], 1);
    header[index] = NULL;

    return index;
}

int get_invoke_id()
{
    if (s_vars->g_all_devices.devices_num < DEVICE_NUM_MAX)
    {
        s_vars->g_all_devices.devices_num++;
        return s_vars->g_all_devices.devices_num;
    }
    else
    {
        return free_invoke_id();
    }
}

BacDevice2* is_device_exist(uint32_t instance_number)
{
    BacDevice2** device = s_vars->g_all_devices.devices_header;
    int i = 0;
    for(i = 1; i <= s_vars->g_all_devices.devices_num; i++)
    {
        if (device[i] != NULL && device[i]->instanceNumber == instance_number)
        {
            return device[i];
        }
    }
    return NULL;
}

BacDevice2* get_device_by_instance_number(uint32_t instance_number)
{
    return is_device_exist(instance_number);
}

BacDevice2* get_device_by_invoke_id(uint8_t invoke_id)
{
    if(invoke_id <= 0 || invoke_id > DEVICE_NUM_MAX)
    {
        return NULL;
    }
    return s_vars->g_all_devices.devices_header[invoke_id];
}

void handle_for_unexpected(uint8_t invoke_id)
{
    BacDevice2* device = get_device_by_invoke_id(invoke_id);
    if(device == NULL)
    {
        snprintf(s_log_buff, BUFF_LEN,
            "handle_for_unexpected : get unexpected message from unknow device (invoke_id is %d)",
            invoke_id);
        log_debug(s_log_buff);
        return;
    }
    device->last_ack_time = time(NULL);

    if(device->current_state == READ_OBJECT_LIST)
    {
        if(device->objects_size == 0)
        {
            send_read_struct_list(device);
        }
        else
        {
            read_object_list_if_need(device);
        }
    }
    else if(device->current_state == SUBSCRIBE_ALL_OBJECT)
    {
        if(device->handle_next_object != NULL)
        {
            if(s_vars->g_interval.subscribe_type == SUBSCRIBE_PORPERTY
                || s_vars->g_interval.subscribe_type == SUBSCRIBE_PROPERTY_WITH_COV_INCREMENT)
            {
                // un-support subscribe property
                if(device->handle_next_object->un_support_state == UN_SUPPORT_SUB_OBJECT)
                {
                    // both object and property are un-support
                    device->handle_next_object->support_cov = UN_SUPPORT_SUB;
                }
                else
                {
                    // maybe support subscribe object
                    device->handle_next_object->support_cov = SUPPORT_SUB;
                    device->handle_next_object->un_support_state = UN_SUPPORT_SUB_PROPERTY;
                }
                
            }
            else if(s_vars->g_interval.subscribe_type == SUBSCRIBE_OBJECT)
            {
                // un-support subscribe object
                if(device->handle_next_object->un_support_state == UN_SUPPORT_SUB_PROPERTY)
                {
                    // both object and property are un-support
                    device->handle_next_object->support_cov = UN_SUPPORT_SUB;
                }
                else
                {
                    // maybe support subscribe property
                    device->handle_next_object->support_cov = SUPPORT_SUB;
                    device->handle_next_object->un_support_state = UN_SUPPORT_SUB_OBJECT;
                }
            }
        }
        subscribe_if_need(device);
    }
    else if(device->current_state == CANCEL_SUBSCRIBE_OBJECT
            || device->current_state == CANCEL_AND_RE_SUBSCRIBE)
    {
        cancel_subscribe_object_if_need(device);
    }
    else
    {
        device->send_next = 1;
        send_next_request(device);
    }
}

void set_global_vars(GlobalVar* pVars)
{
    s_vars = pVars;
}

static void my_error_handler(
    BACNET_ADDRESS * src,
    uint8_t invoke_id,
    BACNET_ERROR_CLASS error_class,
    BACNET_ERROR_CODE error_code)
{

    log_debug("my_error_handler");
    snprintf(s_log_buff, BUFF_LEN, "BACnet Error: %s: %s\r\n",
        bactext_error_class_name((int) error_class),
        bactext_error_code_name((int) error_code));
    log_debug(s_log_buff);

    handle_for_unexpected(invoke_id);
}

void my_abort_handler(
    BACNET_ADDRESS * src,
    uint8_t invoke_id,
    uint8_t abort_reason,
    bool server)
{
    (void) server;
    log_debug("my_abort_handler");
    snprintf(s_log_buff, BUFF_LEN, "BACnet Abort: %s\r\n",
        bactext_abort_reason_name((int) abort_reason));
    log_debug(s_log_buff);

    handle_for_unexpected(invoke_id);
}

void my_reject_handler(
    BACNET_ADDRESS * src,
    uint8_t invoke_id,
    uint8_t reject_reason)
{
    log_debug("my_reject_handler");
    snprintf(s_log_buff, BUFF_LEN, "BACnet Reject: %s\r\n",
        bactext_reject_reason_name((int) reject_reason));
    log_debug(s_log_buff);

    handle_for_unexpected(invoke_id);
}

int is_support_object(BACNET_OBJECT_TYPE type)
{
    int result = 0;

    switch(type) {
        case OBJECT_ANALOG_INPUT:
        case OBJECT_ANALOG_OUTPUT:
        case OBJECT_ANALOG_VALUE:
        case OBJECT_BINARY_INPUT:
        case OBJECT_BINARY_OUTPUT:
        case OBJECT_BINARY_VALUE:
        case OBJECT_MULTI_STATE_INPUT:
        case OBJECT_MULTI_STATE_OUTPUT:
        case OBJECT_MULTI_STATE_VALUE:
        case OBJECT_LIFE_SAFETY_POINT:
        case OBJECT_LOAD_CONTROL:
        case OBJECT_POSITIVE_INTEGER_VALUE:
        case OBJECT_DEVICE:
            result = 1;
            break;
        default:
            result = 0;
            break;
    }

    return result;
}

/** Handler for a ReadProperty ACK.
 *
 * @param service_request [in] The contents of the service request.
 * @param service_len [in] The length of the service_request.
 * @param src [in] BACNET_ADDRESS of the source of the message
 * @param service_data [in] The BACNET_CONFIRMED_SERVICE_DATA information
 *                          decoded from the APDU header of this message.
//  */
void my_read_property_ack_handler(
    uint8_t * service_request,
    uint16_t service_len,
    BACNET_ADDRESS * src,
    BACNET_CONFIRMED_SERVICE_ACK_DATA * service_data)
{
    int len = 0;
    BACNET_READ_PROPERTY_DATA data;
    log_debug("my_read_property_ack_handler");
    len = rp_ack_decode_service_request(service_request, service_len, &data);
    if (len > 0)
    {
        uint8_t invoke_id = service_data->invoke_id;
        BacDevice2* device = s_vars->g_all_devices.devices_header[invoke_id];
        if(device == NULL)
        {
            return;
        }
        device->last_ack_time = time(NULL);

        if(data.object_property == PROP_OBJECT_LIST)
        {
            if(device->update_objects == 1)
            {
                // clean up objects
                device->handle_next_object = NULL;
                BacObject* object = device->objects_header;
                BacObject* old_object = NULL;
                while(object)
                {
                    old_object = object;
                    object = old_object->next;
                    free(old_object);
                }
                device->objects_header = NULL;
                clean_request(device);
                device->update_objects = 0;
                device->objects_size = 0;
            }
            
            // decode
            BACNET_APPLICATION_DATA_VALUE value;
            uint8_t *application_data = data.application_data;
            int application_data_len = data.application_data_len;
            for(;;)
            {
                len =
                bacapp_decode_application_data(application_data,
                (uint8_t) application_data_len, &value);
                if (len > 0)
                {
                    if(data.array_index == 0 && value.tag == BACNET_APPLICATION_TAG_UNSIGNED_INT)
                    {
                        device->objects_size = value.type.Unsigned_Int;
                        device->current_state = READ_OBJECT_LIST;
                    }
                    else if (value.tag == BACNET_APPLICATION_TAG_OBJECT_ID)
                    {
                        BACNET_OBJECT_ID object_id = value.type.Object_Id;
                        // if object_type is OBJECT_DEVICE, skip it
                        if(object_id.type != OBJECT_DEVICE)
                        {
                            if(is_support_object(object_id.type))
                            {
                                // save new object
                                BacObject* new_object = new_bac_object();
                                new_object->objectType = object_id.type;
                                new_object->objectInstance = object_id.instance;
                                new_object->next = device->objects_header;
                                new_object->support_cov = NEED_TO_SUB;
                                device->objects_header = new_object;
                            }
                        }
                    }

                    if(data.array_index < device->objects_size)
                    {
                        device->next_index = data.array_index + 1;
                        my_tsm_logic_invokeID_set(device->invokeId);
                        Send_Read_Property_Request(device->instanceNumber,
                                        data.object_type, data.object_instance,
                                        data.object_property, data.array_index + 1);
                    }
                    else
                    {
                        if(s_vars->g_interval.subscribe_type != NO_SUBSCRIBE)
                        {
                            device->handle_next_object = NULL;
                            device->current_state = SUBSCRIBE_ALL_OBJECT;
                            subscribe_if_need(device);
                        }
                        else
                        {
                            device->current_state = ENTER_MAIN_LOOP;
                            device->handle_next_object = device->objects_header;
                            device->disable = 0;
                            device->send_next = 1;
                        }
                    }
        
                    if (len < application_data_len)
                    {
                        // there's more
                        application_data += len;
                        application_data_len -= len;
                    }
                    else
                    {
                        break;
                    }
                }
                else
                {
                    break;
                }
            }
        }
    }
}

/** Handler for a ReadPropertyMultiple ACK.
 * @ingroup DSRPM
 * For each read property, print out the ACK'd data,
 * and free the request data items from linked property list.
 *
 * @param service_request [in] The contents of the service request.
 * @param service_len [in] The length of the service_request.
 * @param src [in] BACNET_ADDRESS of the source of the message
 * @param service_data [in] The BACNET_CONFIRMED_SERVICE_DATA information
 *                          decoded from the APDU header of this message.
 */
void my_read_property_multiple_ack_handler(
    uint8_t * service_request,
    uint16_t service_len,
    BACNET_ADDRESS * src,
    BACNET_CONFIRMED_SERVICE_ACK_DATA * service_data)
{
    log_debug("my_read_property_multiple_ack_handler");
    BacDevice2* device = get_device_by_invoke_id(service_data->invoke_id);
    if(device == NULL)
    {
        log_debug("get a rpm_ack from unknow device");
        return;
    }
    time_t now_time = time(NULL);
    device->last_iam_time = now_time;

    // just publish to cloud
    int len = g_pdu_len + 1;
    uint8_t* original_apdu = malloc(len*sizeof(uint8_t));
    int i = 0;
    for(i = 0; i < g_pdu_len; i++)
    {
        original_apdu[i] = g_rx_buf1[i];
    }
    original_apdu[g_pdu_len] = '\0';

    mqtt_send_rpmack(original_apdu, g_pdu_len, s_vars, device->instanceNumber);
    device->last_ack_time = now_time;
    device->send_next = 1;
    send_next_request(device);
}

void my_unconfirmed_cov_notification_handler(
    uint8_t * service_request,
    uint16_t service_len,
    BACNET_ADDRESS * src)
{
    log_debug("my_unconfirmed_cov_notification_handler");

    // decode
    BACNET_COV_DATA cov_data;
    BACNET_PROPERTY_VALUE property_value[2];
    BACNET_PROPERTY_VALUE *p_property_value = NULL;
    unsigned index = 0;
    int len = 0;

    p_property_value = &property_value[0];
    while (p_property_value) {
        index++;
        if (index < 2) {
            p_property_value->next = &property_value[index];
        } else {
            p_property_value->next = NULL;
        }
        p_property_value = p_property_value->next;
    }
    cov_data.listOfValues = &property_value[0];

    len =
        cov_notify_decode_service_request(service_request, service_len,
        &cov_data);

    if (len > 0)
    {
        BacDevice2* device = get_device_by_instance_number(cov_data.initiatingDeviceIdentifier);
        if (device == NULL)
        {
            log_debug("get a ucov_notification from unknow device");
            return;
        }
        
        // update the last_iam_time of those property
        time_t now_time = time(NULL);
        device->last_iam_time = now_time;
    
        // just publish to cloud
        int len = g_pdu_len + 1;
        uint8_t* original_apdu = malloc(len*sizeof(uint8_t));
        int i = 0;
        for(i = 0; i < g_pdu_len; i++)
        {
            original_apdu[i] = g_rx_buf1[i];
        }
        original_apdu[g_pdu_len] = '\0';
    
        mqtt_send_ucovnotifica(original_apdu, g_pdu_len, s_vars, device->instanceNumber);

        // update the last_read_time of those object
        uint16_t object_type = cov_data.monitoredObjectIdentifier.type;
        uint32_t object_instance = cov_data.monitoredObjectIdentifier.instance;
        BacObject* device_objects = device->objects_header;
        while(device_objects)
        {
            if (device_objects->objectType == object_type
                && device_objects->objectInstance == object_instance)
            {
                device_objects->last_read_time = now_time;
                break;
            }
            device_objects = device_objects->next;
        }
    }
}

void my_iam_handler(
    uint8_t * service_request,
    uint16_t service_len,
    BACNET_ADDRESS * src)
{
    log_debug("my_iam_handler");
    // publish original apdu
    int len = g_pdu_len + 1;
    uint8_t* original_apdu = malloc(len*sizeof(uint8_t));
    int i = 0;
    for(i = 0; i < g_pdu_len; i++)
    {
        original_apdu[i] = g_rx_buf1[i];
    }
    original_apdu[g_pdu_len] = '\0';

    // decode
    len = 0;
    uint32_t device_id = 0;
    unsigned max_apdu = 0;
    int segmentation = 0;
    uint16_t vendor_id = 0;

    (void) service_len;
    len =
        iam_decode_service_request(service_request, &device_id, &max_apdu,
        &segmentation, &vendor_id);
    if (len > 0) {
        time_t now_time = time(NULL);
        BacDevice2* device = is_device_exist(device_id);
        if(device == NULL)
        {
            printf("found a new device : %d\n", device_id);
            mqtt_send_iam(original_apdu, g_pdu_len, s_vars);
            // if this device is not be found
            // save it
            if (max_apdu <= 0) {
                char log_buff[BUFF_LEN] = {0};
                snprintf(log_buff, BUFF_LEN,
                    "detected device %d with max_apdu=%d, reseting to %d \r\n",
                    device_id, max_apdu, MAX_APDU);
                log_debug(log_buff);
                max_apdu = MAX_APDU;
            }
            bool bind_result = address_bind_request(device_id, &max_apdu, src);
            if(!bind_result)
            {
                address_add_binding(device_id, max_apdu, src);
            }

            BacDevice2* new_device = new_bac_device2();
            new_device->instanceNumber = device_id;
            new_device->last_discover_time = now_time;
            new_device->last_iam_time = now_time;
            new_device->last_ack_time = now_time;
            new_device->invokeId = get_invoke_id();
            s_vars->g_all_devices.devices_header[new_device->invokeId] = new_device;

            // get all properties of this device
            send_read_struct_list(new_device);
        }
        else
        {
            if (max_apdu <= 0) {
                char log_buff[BUFF_LEN] = {0};
                snprintf(log_buff, BUFF_LEN,
                    "detected device %d with max_apdu=%d, reseting to %d \r\n",
                    device_id, max_apdu, MAX_APDU);
                log_debug(log_buff);
                max_apdu = MAX_APDU;
            }
            // update address
            address_add_binding(device_id, max_apdu, src);
            // if device has be found, update it`s last_iam_time
            device->last_iam_time = now_time;
        }
    }
}

void my_subscribe_simple_ack_handler(
    BACNET_ADDRESS * src,
    uint8_t invoke_id)
{
    log_debug("my_subscribe_simple_ack_handler");
    BacDevice2* device = get_device_by_invoke_id(invoke_id);
    if(device == NULL)
    {
        log_debug("get a subscribe_ack from unknow device");
        return;
    }

    if(device->current_state == SUBSCRIBE_ALL_OBJECT)
    {
        device->handle_next_object->support_cov = SUBSCRIBED;
        subscribe_if_need(device);
    }
    else if(device->current_state == CANCEL_SUBSCRIBE_OBJECT
            || device->current_state == CANCEL_AND_RE_SUBSCRIBE)
    {
        cancel_subscribe_object_if_need(device);
    }
    else
    {
        device->last_ack_time = time(NULL);
        device->send_next = 1;
        device->handle_next_object->support_cov = SUBSCRIBED;
        send_next_request(device);
    }
}

void my_whois_handler(
    uint8_t * service_request,
    uint16_t service_len,
    BACNET_ADDRESS * src)
{
    // do nothing
    log_debug("receive who_is request");
}

void my_write_property_multiple_ack_handler(
    BACNET_ADDRESS * src,
    uint8_t invoke_id)
{
    BacDevice2* device = get_device_by_invoke_id(invoke_id);
    if(device == NULL)
    {
        return;
    }
    device->last_ack_time = time(NULL);
    device->send_next = 1;
    send_next_request(device);
}

static void Init_Service_Handlers(void)
{
    Device_Init(NULL);
    /* we need to handle who-is
       to support dynamic device binding to us */
    apdu_set_unconfirmed_handler(SERVICE_UNCONFIRMED_WHO_IS, my_whois_handler);
    /* handle i-am to support binding to other devices */
    apdu_set_unconfirmed_handler(SERVICE_UNCONFIRMED_I_AM, my_iam_handler);
    /* set the handler for all the services we don't implement
       It is required to send the proper reject message... */
    apdu_set_unrecognized_service_handler_handler
    (handler_unrecognized_service);
    /* we must implement read property - it's required! */
    apdu_set_confirmed_handler(SERVICE_CONFIRMED_READ_PROPERTY,
                               handler_read_property);
    /* handle the data coming back from confirmed requests */
    // handle the rp ack
    apdu_set_confirmed_ack_handler(SERVICE_CONFIRMED_READ_PROPERTY,
                                   my_read_property_ack_handler);

    // handle the rpm ack
    apdu_set_confirmed_ack_handler(SERVICE_CONFIRMED_READ_PROP_MULTIPLE,
                                   my_read_property_multiple_ack_handler);
    
    // handle the wpm ack
    apdu_set_confirmed_simple_ack_handler(SERVICE_CONFIRMED_WRITE_PROP_MULTIPLE,
                                   my_write_property_multiple_ack_handler);

    // handle the Simple ack coming back from Subscribe object
    apdu_set_confirmed_simple_ack_handler(SERVICE_CONFIRMED_SUBSCRIBE_COV,
                                   my_subscribe_simple_ack_handler);

    // handle the Simple ack coming back from Subscribe property                              
    apdu_set_confirmed_simple_ack_handler(SERVICE_CONFIRMED_SUBSCRIBE_COV_PROPERTY,
                                   my_subscribe_simple_ack_handler);

    // handle the COV notification
    apdu_set_unconfirmed_handler(SERVICE_UNCONFIRMED_COV_NOTIFICATION,
                                   my_unconfirmed_cov_notification_handler);

    /* handle any errors coming back */
    apdu_set_error_handler(SERVICE_CONFIRMED_READ_PROPERTY, my_error_handler);
    apdu_set_error_handler(SERVICE_CONFIRMED_READ_PROP_MULTIPLE, my_error_handler);
    apdu_set_error_handler(SERVICE_CONFIRMED_WRITE_PROPERTY, my_error_handler);
    apdu_set_error_handler(SERVICE_CONFIRMED_WRITE_PROP_MULTIPLE, my_error_handler);
    apdu_set_error_handler(SERVICE_CONFIRMED_SUBSCRIBE_COV, my_error_handler);
    apdu_set_error_handler(SERVICE_CONFIRMED_SUBSCRIBE_COV_PROPERTY, my_error_handler);
    apdu_set_abort_handler(my_abort_handler);
    apdu_set_reject_handler(my_reject_handler);
}

int start_local_bacnet_device(Bac2mqttConfig* pconfig)
{
    log_debug("start_local_bacnet_device");
    Device_Set_Object_Instance_Number(pconfig->device.instanceNumber);
    address_init();
    Init_Service_Handlers();
    if (pconfig->device.ipOrInterface != NULL && strlen(pconfig->device.ipOrInterface) > 0)
    {
        char env[MAX_LEN];
        snprintf(env, MAX_LEN, "BACNET_IFACE=%s", pconfig->device.ipOrInterface);
        putenv(env);
    }

    dlenv_init();
    atexit(datalink_cleanup);

    send_whois_immediately();

    return 0;
}

void receive_and_handle()
{
    BACNET_ADDRESS src =
    {
        0
    };  /* address where message came from */
    unsigned timeout = 2;     /* milliseconds */

    // use a while loop to quick handle the bacnet packages
    int cap = 100;
    do {
        /* returns 0 bytes on timeout */
        g_pdu_len = datalink_receive(&src, &g_rx_buf1[0], MAX_MPDU, timeout);
        /* process */
        if (g_pdu_len)
        {
            snprintf(s_log_buff, BUFF_LEN, "datalink_receive returned %d bytes", g_pdu_len);
            log_debug(s_log_buff);
            npdu_handler(&src, &g_rx_buf1[0], g_pdu_len);
        }
    } while (g_pdu_len > 0 && cap-- > 0);
}

void send_whois_immediately()
{
    log_debug("send whois request immediately");
    Send_WhoIs(-1, -1);
}

void send_rpm_immediately(cJSON* root)
{
    log_debug("send rpm request immediately");
    if (!cJSON_HasObjectItem(root, "targetInstanceNumber"))
    {
        log_debug("send_rpm_immediately failed : can`t get the target instance number");
        return;
    }
    uint32_t target_instance_number = (uint32_t) json_int(root, "targetInstanceNumber");

    BacDevice2* device = get_device_by_instance_number(target_instance_number);
    if(device == NULL)
    {
        char log_buff[BUFF_LEN] = {0};
        snprintf(log_buff, BUFF_LEN,
            "send_rpm_immediately failed : can`t find this device by intance number %d",
            target_instance_number);
        log_debug(log_buff);
        return;
    }

    cJSON* properties = cJSON_GetObjectItem(root, "properties");
    int properties_len = cJSON_GetArraySize(properties);
    if (properties_len > 0)
    {
        REQUEST_DATA* request = NULL;
        BACNET_READ_ACCESS_DATA* header = NULL;
        char rpm_object_number = 0;
        int i = 0;
        for(i = 0; i < properties_len; i++)
        {
            cJSON* property = cJSON_GetArrayItem(properties, i);
            char* object_type = json_string(property, "objectType");
            int object_instance = json_int(property, "objectInstance");
            char* property_name = json_string(property, "property");

            BACNET_READ_ACCESS_DATA* rpm_object = calloc(1, sizeof(BACNET_READ_ACCESS_DATA));
            rpm_object->object_type = str2_bac_object_type(object_type);
            rpm_object->object_instance = object_instance;

            BACNET_PROPERTY_REFERENCE* rpm_property = calloc(1, sizeof(BACNET_PROPERTY_REFERENCE));
            rpm_property->next = NULL;
            rpm_property->propertyArrayIndex = BACNET_ARRAY_ALL;
            rpm_property->propertyIdentifier = str2_property_id(property_name);
            if(cJSON_HasObjectItem(property, "index"))
            {
                rpm_property->propertyArrayIndex = json_int(property, "index");
            }

            rpm_object->listOfProperties = rpm_property;

            rpm_object->next = header;
            header = rpm_object;

            rpm_object_number++;
            if (rpm_object_number >= s_rpm_object_num_max && header)
            {
                REQUEST_DATA* req = calloc(1, sizeof(REQUEST_DATA));
                req->request = (void*) header;
                req->type = RPM_REQUEST;
                req->next = request;
                request = req;

                header = NULL;
                rpm_object_number = 0;
            }
        }

        if (header)
        {
            REQUEST_DATA* req = calloc(1, sizeof(REQUEST_DATA));
            req->request = (void*) header;
            req->type = RPM_REQUEST;
            req->next = request;
            request = req;
        }

        if(request != NULL)
        {
            add_request(device, request);
        }
    }
}

void send_wpm_immediately(cJSON* root)
{
    log_debug("send wpm request immediately");
    if (!cJSON_HasObjectItem(root, "targetInstanceNumber"))
    {
        log_debug("send_wpm_immediately failed : can`t get the target instance number");
        return;
    }
    uint32_t target_instance_number = (uint32_t) json_int(root, "targetInstanceNumber");

    BacDevice2* device = get_device_by_instance_number(target_instance_number);
    if(device == NULL)
    {
        char log_buff[BUFF_LEN] = {0};
        snprintf(log_buff, BUFF_LEN,
            "send_wpm_immediately failed : can`t find this device by instance number -> %d",
            target_instance_number);
        log_debug(log_buff);
        return;
    }

    cJSON* properties = cJSON_GetObjectItem(root, "properties");
    int properties_len = cJSON_GetArraySize(properties);
    if (properties_len > 0)
    {
        REQUEST_DATA* request = NULL;
        BACNET_WRITE_ACCESS_DATA* header = NULL;
        char rpm_object_number = 0;
        int i = 0;
        for(i = 0; i < properties_len; i++)
        {
            cJSON* property = cJSON_GetArrayItem(properties, i);
            char* object_type = json_string(property, "objectType");
            int object_instance = json_int(property, "objectInstance");
            char* property_name = json_string(property, "property");
            char* present_value = json_string(property, "presentValue");
            uint8_t tag = cJSON_GetObjectItem(property, "tag")->valueint;
            uint32_t index = BACNET_ARRAY_ALL;
            if(cJSON_HasObjectItem(property, "index"))
            {
                index = json_int(property, "index");
            }
            int priority = json_int(property, "priority");

            // wpm
            BACNET_WRITE_ACCESS_DATA* wpm_object = calloc(1, sizeof(BACNET_WRITE_ACCESS_DATA));
            wpm_object->object_type = str2_bac_object_type(object_type);
            wpm_object->object_instance = object_instance;

            BACNET_PROPERTY_VALUE* wpm_property = calloc(1, sizeof(BACNET_PROPERTY_VALUE));
            wpm_property->propertyIdentifier = str2_property_id(property_name);
            wpm_property->propertyArrayIndex = index;
            wpm_property->value.context_tag = tag;
            bool status = bacapp_parse_application_data(tag, present_value, &(wpm_property->value));
            if(!status)
            {
                char log_buff[BUFF_LEN] = {0};
                snprintf(log_buff, BUFF_LEN,
                    "bacapp_parse_application_data failed, tag = %d, value = \"%s\" ",
                    tag, present_value);
                log_debug(log_buff);
                free(wpm_object);
                free(wpm_property);
            }
            else
            {
                wpm_property->value.next = NULL;
                wpm_property->priority = priority;
                wpm_property->next = NULL;

                wpm_object->listOfProperties = wpm_property;
                wpm_object->next = header;
                header = wpm_object;

                rpm_object_number++;
                if (rpm_object_number >= s_rpm_object_num_max && header)
                {
                    REQUEST_DATA* req = calloc(1, sizeof(REQUEST_DATA));
                    req->request = (void*) header;
                    req->type = WPM_REQUEST;
                    req->next = request;
                    request = req;

                    header = NULL;
                    rpm_object_number = 0;
                }
            }
        }

        if (header)
        {
            REQUEST_DATA* req = calloc(1, sizeof(REQUEST_DATA));
            req->request = (void*) header;
            req->type = WPM_REQUEST;
            req->next = request;
            request = req;
        }

        if(request != NULL)
        {
            add_request(device, request);
        }
    }
}

void add_property_message_if_need(BACNET_SUBSCRIBE_COV_DATA* cov_data)
{
    if(s_vars->g_interval.subscribe_type == SUBSCRIBE_PORPERTY
        || s_vars->g_interval.subscribe_type == SUBSCRIBE_PROPERTY_WITH_COV_INCREMENT)
    {
        cov_data->monitoredProperty.propertyIdentifier = PROP_PRESENT_VALUE;
        cov_data->monitoredProperty.propertyArrayIndex = BACNET_ARRAY_ALL;
        cov_data->monitoredProperty.next = NULL;

        if(s_vars->g_interval.subscribe_type == SUBSCRIBE_PROPERTY_WITH_COV_INCREMENT
            && is_analog_object(cov_data->monitoredObjectIdentifier.type))
        {
            cov_data->covIncrementPresent = 1;
            cov_data->covIncrement = s_vars->g_interval.cov_increment;
        }
    }
}

void subscribe_unconfirmed(BacDevice2* device, BacObject* object)
{
    if(device == NULL
        || s_vars->g_interval.subscribe_type == NO_SUBSCRIBE)
    {
        return;
    }
    object->subscribe_time = time(NULL);

    BACNET_SUBSCRIBE_COV_DATA* cov_data = calloc(1, sizeof(BACNET_SUBSCRIBE_COV_DATA));
    cov_data->subscriberProcessIdentifier = 1;
    cov_data->monitoredObjectIdentifier.type = object->objectType;
    cov_data->monitoredObjectIdentifier.instance = object->objectInstance;
    cov_data->lifetime = s_vars->g_interval.subscribe_duration;
    cov_data->next = NULL;
    add_property_message_if_need(cov_data);
    my_tsm_logic_invokeID_set(device->invokeId);
    if(s_vars->g_interval.subscribe_type == SUBSCRIBE_OBJECT)
    {
        Send_COV_Subscribe(device->instanceNumber, cov_data);
    }
    else
    {
        Send_COV_Subscribe_Property(device->instanceNumber, cov_data);
    }
    free(cov_data);
}

void send_read_struct_list(BacDevice2* device)
{
    log_debug("send_read_struct_list");
    
    device->send_next = 0;
    device->update_objects = 1;
    device->disable = 1;
    device->current_state = READ_OBJECT_LIST;
    device->next_index = 0;
    uint32_t instance_number = device->instanceNumber;
    BACNET_OBJECT_TYPE object_type = OBJECT_DEVICE;
    uint32_t object_instance = device->instanceNumber;
    BACNET_PROPERTY_ID property_id = PROP_OBJECT_LIST;
    int32_t object_index = 0;

    my_tsm_logic_invokeID_set(device->invokeId);
    Send_Read_Property_Request(instance_number,
                    object_type, object_instance,
                    property_id, object_index);
}

void add_request(BacDevice2* device, REQUEST_DATA* request)
{
    if(device == NULL
        || request == NULL)
    {
        return;
    }

    Thread_lock_mutex(device->request_list_mutex);
    if(device->request_list.head == NULL)
    {
        device->request_list.head = request;
    }
    else
    {
        device->request_list.tail->next = request;
    }
    while(request)
    {
        if(request->next == NULL)
        {
            device->request_list.tail = request;
        }
        request = request->next;
    }
    Thread_unlock_mutex(device->request_list_mutex);
}

void add_request_if_need(BacDevice2* device)
{
    if(device == NULL || device->disable == 1)
    {
        return;
    }
    
    BacObject* objects = device->objects_header;
    BACNET_READ_ACCESS_DATA* header = NULL;
    REQUEST_DATA* request = NULL;
    char rpm_object_number = 0;
    while(objects)
    {
        if (objects->support_cov == NEED_TO_SUB
            && s_vars->g_interval.subscribe_type != NO_SUBSCRIBE)
        {
            if(!is_need_to_subscribe(objects))
            {
                objects->support_cov = SUPPORT_SUB;
                objects = objects->next;
                continue;
            }

            BACNET_SUBSCRIBE_COV_DATA* cov_data = calloc(1, sizeof(BACNET_SUBSCRIBE_COV_DATA));
            cov_data->subscriberProcessIdentifier = 1;
            cov_data->monitoredObjectIdentifier.type = objects->objectType;
            cov_data->monitoredObjectIdentifier.instance = objects->objectInstance;
            cov_data->lifetime = s_vars->g_interval.subscribe_duration;
            cov_data->next = NULL;
            add_property_message_if_need(cov_data);

            REQUEST_DATA* req = calloc(1, sizeof(REQUEST_DATA));
            req->request = (void*) cov_data;
            if(s_vars->g_interval.subscribe_type == SUBSCRIBE_OBJECT)
            {
                req->type = SUBSCRIBE_OBJECT_REQUEST;
            }
            else
            {
                req->type = SUBSCRIBE_PROPERTY_REQUEST;
            }
            req->next = request;
            request = req;

            objects->subscribe_time = time(NULL);
            objects->support_cov = SUPPORT_SUB;
        }

        if (objects->need_to_read)
        {
            BACNET_READ_ACCESS_DATA* next_data = calloc(1, sizeof(BACNET_READ_ACCESS_DATA));
            next_data->object_type = objects->objectType;
            next_data->object_instance = objects->objectInstance;

            BACNET_PROPERTY_REFERENCE* next_data_property = calloc(1, sizeof(BACNET_PROPERTY_REFERENCE));
            next_data_property->propertyIdentifier = PROP_PRESENT_VALUE;
            next_data_property->propertyArrayIndex = BACNET_ARRAY_ALL;
            next_data_property->next = NULL;

            next_data->listOfProperties = next_data_property;
            next_data->next = header;
            header = next_data;
            
            objects->need_to_read = 0;
            rpm_object_number++;
            if(rpm_object_number >= s_rpm_object_num_max)
            {
                REQUEST_DATA* req = calloc(1, sizeof(REQUEST_DATA));
                req->request = (void*) header;
                req->type = RPM_REQUEST;
                req->next = request;
                request = req;

                header = NULL;
                rpm_object_number = 0;
            }
        }
        objects = objects->next;
    }
    if(header)
    {
        REQUEST_DATA* req = calloc(1, sizeof(REQUEST_DATA));
        req->request = (void*) header;
        req->type = RPM_REQUEST;
        req->next = request;
        request = req;
    }

    add_request(device, request);
}

void mark_object_for_subscribe(
    BacDevice2* device,
    BACNET_SUBSCRIBE_COV_DATA* cov_data
)
{
    BacObject* object = device->objects_header;
    BACNET_OBJECT_ID target_object = cov_data->monitoredObjectIdentifier;
    while(object)
    {
        if (object->objectType == target_object.type
                && object->objectInstance == target_object.instance)
        {
            device->handle_next_object = object;
            break;
        }
        object = object->next;
    }
}

void send_next_request(BacDevice2* device)
{
    if(device == NULL || device->disable == 1)
    {
        return;
    }

    REQUEST_DATA* request = NULL;
    Thread_lock_mutex(device->request_list_mutex);
    if(device->request_list.head != NULL)
    {
        request = device->request_list.head;
        device->request_list.head = request->next;
    }
    Thread_unlock_mutex(device->request_list_mutex);

    if(request != NULL)
    {
        device->send_next = 0;
        switch(request->type)
        {
            case RPM_REQUEST :
                {
                    BACNET_READ_ACCESS_DATA* header = request->request;
                    uint8_t buffer[MAX_PDU] = {0};
                    log_debug("Send Read Property Multiple Request");
                    my_tsm_logic_invokeID_set(device->invokeId);
                    Send_Read_Property_Multiple_Request(&buffer[0],
                                         sizeof(buffer), device->instanceNumber,
                                         header);
                    cleanup_read_access_data(header);
                }
                break;
            case WPM_REQUEST :
                {
                    BACNET_WRITE_ACCESS_DATA* header = request->request;
                    log_debug("send wpm request immediately");
                    my_tsm_logic_invokeID_set(device->invokeId);
                    Send_Write_Property_Multiple_Request_Data(
                                        device->instanceNumber,
                                        header);
                    cleanup_write_access_data(header);
                }
                break;
            case SUBSCRIBE_OBJECT_REQUEST :
                {
                    BACNET_SUBSCRIBE_COV_DATA* cov_data = request->request;
                    if(s_vars->g_interval.subscribe_type != NO_SUBSCRIBE)
                    {
                        mark_object_for_subscribe(device, cov_data);
                        my_tsm_logic_invokeID_set(device->invokeId);
                        Send_COV_Subscribe(device->instanceNumber, cov_data);
                    }
                    free(cov_data);
                }
                break;
            case SUBSCRIBE_PROPERTY_REQUEST :
                {
                    BACNET_SUBSCRIBE_COV_DATA* cov_data = request->request;
                    if(s_vars->g_interval.subscribe_type != NO_SUBSCRIBE)
                    {
                        mark_object_for_subscribe(device, cov_data);
                        my_tsm_logic_invokeID_set(device->invokeId);
                        Send_COV_Subscribe_Property(device->instanceNumber, cov_data);
                    }
                    free(cov_data);
                }
                break;
            default :
                break;
        }
        free(request);
    }
}

void clean_request(BacDevice2* device)
{
    if(device == NULL)
    {
        return;
    }

    Thread_lock_mutex(device->request_list_mutex);
    REQUEST_DATA* request = device->request_list.head;
    REQUEST_DATA* old_request = NULL;
    while(request)
    {
        switch(request->type)
        {
            case RPM_REQUEST :
                {
                    BACNET_READ_ACCESS_DATA* header = request->request;
                    cleanup_read_access_data(header);
                }
                break;
            case WPM_REQUEST :
                {
                    BACNET_WRITE_ACCESS_DATA* header = request->request;
                    cleanup_write_access_data(header);
                }
                break;
            case SUBSCRIBE_OBJECT_REQUEST :
            case SUBSCRIBE_PROPERTY_REQUEST :
                {
                    BACNET_SUBSCRIBE_COV_DATA* cov_data = request->request;
                    free(cov_data);
                }
                break;
            default :
                break;
        }
        old_request = request;
        request = old_request->next;
        free(old_request);
    }
    device->request_list.head = NULL;
    device->request_list.tail = NULL;
    Thread_unlock_mutex(device->request_list_mutex);
}

char is_analog_object(BACNET_OBJECT_TYPE object)
{
    char is_analog_object = 0;
    switch(object)
    {
        case OBJECT_ANALOG_INPUT:
        case OBJECT_ANALOG_OUTPUT:
        case OBJECT_ANALOG_VALUE:
            is_analog_object = 1;
            break;
        default:
            break;
    }
    return is_analog_object;
}